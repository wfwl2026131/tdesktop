/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>

#include <map>
#include <vector>

namespace OG {

// The pacing of a batch, shared by every page that sends more than one request.
//
// The console pages all pace themselves the same way, and the reference states
// it in three numbers: a pool interval (the gap between any two requests), an
// account interval (the gap one account keeps between its own requests, never
// smaller than the pool interval) and the two caps that stop the run. The rules
// below are the ones a live account enforces, written down so a page cannot
// invent its own reading:
//
//   * the first request of a run waits the pool interval, nothing more;
//   * a request that follows a different account waits the pool interval;
//   * a request that follows the same account waits the account interval;
//   * the caps count the whole run, not one lane of it;
//   * a flood wait is a promise to the one account that hit it, and it is
//     longer than any interval, so it wins while it lasts.
//
// Nothing here sleeps. The caller asks how long to wait and waits; that keeps
// the rules testable without a clock and lets a page show the pause.
struct PacerConfig {
	int requestIntervalMs = 1000; // between any two requests of the run
	int accountIntervalMs = 0; // per account, never below the request interval
	int successCap = 0; // 0 = no cap
	int execCap = 0; // 0 = no cap
	int floodCap = 0; // 0 = no cap
};

enum class PacerVerdict {
	Go,
	RunOver, // a cap is spent: no further request belongs to this run
};

class Pacer {
public:
	explicit Pacer(const PacerConfig &config);

	// Asks whether the next request for that account may go out, and reserves
	// the attempt when it may. The account is the one the request is sent as:
	// two accounts never share the account interval.
	[[nodiscard]] PacerVerdict claim(const QString &account);

	// The number of milliseconds to wait before a claim for that account would
	// be in time. Zero when it may go now.
	[[nodiscard]] int waitFor(const QString &account) const;

	void recordSuccess();
	void recordFlood(const QString &account, int seconds);

	[[nodiscard]] int attempts() const;
	[[nodiscard]] int successes() const;
	[[nodiscard]] int floods() const;

private:
	[[nodiscard]] bool capped() const;

	PacerConfig _config;
	int _attempts = 0;
	int _successes = 0;
	int _floods = 0;

	// The last request of every account, in milliseconds since the run began.
	std::map<QString, qint64> _lastOfAccount;

	// The moment a flood wait expires, per account. It is a deadline, not a
	// last request: the platform told the account when it may speak again, and
	// that instant is in the future.
	std::map<QString, qint64> _floodUntil;

	// The last request of the run whatever account it was, so the pool
	// interval is kept across a switch.
	qint64 _lastOfRun = 0;
	bool _started = false;
};

// The interval one account keeps between its own requests: the account
// interval, but never below the pool interval, because a page that sets only
// one of the two must still keep the stricter of them.
[[nodiscard]] int EffectiveAccountInterval(const PacerConfig &config);

// Prints the rules above against a fixed sequence. Not called from the client:
// it is the driver the acceptance run builds.
void RunPacerSelfCheck();

} // namespace OG
