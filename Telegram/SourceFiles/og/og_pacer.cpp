/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_pacer.h"

#include <algorithm>
#include <cstdio>

namespace OG {
int EffectiveAccountInterval(const PacerConfig &config) {
	return std::max(config.accountIntervalMs, config.requestIntervalMs);
}

Pacer::Pacer(const PacerConfig &config)
: _config(config) {
	// A negative interval is not a faster pace, it is a typo: the reference
	// treats both numbers as gaps that can only grow.
	_config.requestIntervalMs = std::max(_config.requestIntervalMs, 0);
	_config.accountIntervalMs = std::max(_config.accountIntervalMs, 0);
	_config.successCap = std::max(_config.successCap, 0);
	_config.execCap = std::max(_config.execCap, 0);
	_config.floodCap = std::max(_config.floodCap, 0);
}

bool Pacer::capped() const {
	if (_config.successCap && (_successes >= _config.successCap)) {
		return true;
	}
	if (_config.execCap && (_attempts >= _config.execCap)) {
		return true;
	}
	if (_config.floodCap && (_floods >= _config.floodCap)) {
		return true;
	}
	return false;
}

int Pacer::waitFor(const QString &account) const {
	if (capped()) {
		return 0;
	}
	// The first request of a run waits the pool interval: the reference starts
	// every batch with that gap, and it is also what keeps a run from starting
	// with a burst. A flood wait recorded before the first request still
	// applies, so this baseline is not an early return.
	auto wait = _started
		? qint64(_config.requestIntervalMs) - _lastOfRun
		: qint64(_config.requestIntervalMs);
	const auto i = _lastOfAccount.find(account);
	if (i != _lastOfAccount.end()) {
		wait = std::max(
			wait,
			qint64(EffectiveAccountInterval(_config)) - i->second);
	}
	// A flood wait is a deadline, so it is the one number that can still be in
	// the future when everything else has passed.
	if (const auto flood = _floodUntil.find(account);
		flood != _floodUntil.end()) {
		wait = std::max(wait, flood->second - _lastOfRun);
	}
	return int(std::max(wait, qint64(0)));
}

PacerVerdict Pacer::claim(const QString &account) {
	if (capped()) {
		return PacerVerdict::RunOver;
	}
	// claim() records the request as having happened: the caller is expected
	// to have waited waitFor() first, and asking is the moment the run's clock
	// moves. A caller that claims twice in a row gets the second one paced by
	// the account interval, which is the failure mode a live run would show as
	// a flood wait.
	const auto now = _started ? (_lastOfRun + waitFor(account)) : 0;
	_started = true;
	_lastOfRun = now;
	_lastOfAccount[account] = now;
	++_attempts;
	return PacerVerdict::Go;
}

void Pacer::recordSuccess() {
	++_successes;
}

void Pacer::recordFlood(const QString &account, int seconds) {
	++_floods;
	if (seconds <= 0) {
		return;
	}
	// A flood wait is the platform's own promise: the account may not send
	// again until it expires, and it outlasts any configured interval.
	const auto until = _lastOfRun + qint64(seconds) * 1000;
	const auto i = _floodUntil.find(account);
	if (i == _floodUntil.end()) {
		_floodUntil.emplace(account, until);
	} else {
		i->second = std::max(i->second, until);
	}
}

int Pacer::attempts() const {
	return _attempts;
}

int Pacer::successes() const {
	return _successes;
}

int Pacer::floods() const {
	return _floods;
}

int RunPacerSelfCheck() {
	auto failures = 0;
	const auto check = [&](const char *name, bool passed) {
		if (!passed) {
			++failures;
		}
		std::printf(
			"[og][pacer] %-46s %s\n",
			name,
			passed ? "PASS" : "FAIL");
	};

	// 1. The first claim of a run waits the pool interval and nothing else.
	auto pool = PacerConfig();
	pool.requestIntervalMs = 5000;
	pool.accountIntervalMs = 10000;

	auto pacer = Pacer(pool);
	check("1 first claim waits the pool interval",
		pacer.waitFor(QString::fromLatin1("t1")) == 5000);

	// 2. After the first claim the same account owes the account interval.
	pacer.claim(QString::fromLatin1("t1"));
	check("2 same account owes the account interval",
		pacer.waitFor(QString::fromLatin1("t1")) == 10000);

	// 3. A different account only owes the pool interval: the account
	// interval is a promise to one account, not to the run.
	check("3 other account owes the pool interval",
		pacer.waitFor(QString::fromLatin1("t2")) == 5000);

	// 4. The account interval is never below the pool interval, whatever the
	// page sets.
	auto oneSided = PacerConfig();
	oneSided.requestIntervalMs = 3000;
	oneSided.accountIntervalMs = 0;
	check("4 account interval floors at the pool interval",
		EffectiveAccountInterval(oneSided) == 3000);

	// 5. Two claims in a row on one account are priced at the account
	// interval: a caller that forgets to wait cannot send for free.
	auto twice = Pacer(oneSided);
	twice.claim(QString::fromLatin1("t1"));
	const auto second = twice.waitFor(QString::fromLatin1("t1"));
	check("5 back to back claim owes the interval", second == 3000);

	// 6. Alternating two accounts costs the pool interval per request, which
	// is the pace a two lane run shows.
	auto lanes = PacerConfig();
	lanes.requestIntervalMs = 1000;
	lanes.accountIntervalMs = 2000;
	auto alternating = Pacer(lanes);
	alternating.claim(QString::fromLatin1("t1"));
	check("6 alternating accounts cost the pool interval",
		alternating.waitFor(QString::fromLatin1("t2")) == 1000);

	// 7. The success cap stops the run even when attempts remain.
	auto capped = PacerConfig();
	capped.requestIntervalMs = 0;
	capped.successCap = 2;
	auto successCapped = Pacer(capped);
	successCapped.claim(QString::fromLatin1("t1"));
	successCapped.recordSuccess();
	successCapped.claim(QString::fromLatin1("t1"));
	successCapped.recordSuccess();
	check("7 success cap ends the run",
		successCapped.claim(QString::fromLatin1("t1"))
			== PacerVerdict::RunOver);

	// 8. The exec cap counts the attempts, not the successes: a run of
	// failures must still stop.
	auto execConfig = PacerConfig();
	execConfig.requestIntervalMs = 0;
	execConfig.execCap = 3;
	auto execCapped = Pacer(execConfig);
	execCapped.claim(QString::fromLatin1("t1"));
	execCapped.claim(QString::fromLatin1("t1"));
	execCapped.claim(QString::fromLatin1("t1"));
	check("8 exec cap ends the run",
		execCapped.claim(QString::fromLatin1("t1")) == PacerVerdict::RunOver);

	// 9. A flood wait outlasts the configured interval: ten seconds is ten
	// seconds even when the page asked for one.
	auto flooded = Pacer(lanes);
	flooded.claim(QString::fromLatin1("t1"));
	flooded.recordFlood(QString::fromLatin1("t1"), 10);
	check("9 flood wait wins over the interval",
		flooded.waitFor(QString::fromLatin1("t1")) == 10000);

	// 10. The flood wait belongs to the one account that hit it.
	check("10 flood wait does not slow another account",
		flooded.waitFor(QString::fromLatin1("t2")) == 1000);

	// 11. A flood wait on an account that has not sent yet still counts: the
	// platform said so, and the run has to respect it.
	auto fresh = Pacer(lanes);
	fresh.recordFlood(QString::fromLatin1("t3"), 5);
	check("11 flood wait on a fresh account counts",
		fresh.waitFor(QString::fromLatin1("t3")) == 5000);

	// 12. A negative interval is a typo, not a faster pace.
	auto negative = PacerConfig();
	negative.requestIntervalMs = -500;
	check("12 negative interval reads as zero",
		Pacer(negative).waitFor(QString::fromLatin1("t1")) == 0);

	// 13. The counters the page shows are the run's own.
	auto counted = Pacer(lanes);
	counted.claim(QString::fromLatin1("t1"));
	counted.recordSuccess();
	counted.claim(QString::fromLatin1("t2"));
	check("13 counters follow the run",
		counted.attempts() == 2
		&& counted.successes() == 1
		&& counted.floods() == 0);

	std::printf(
		"[og][pacer] %d failure(s)\n",
		failures);
	return failures;
}

} // namespace OG
