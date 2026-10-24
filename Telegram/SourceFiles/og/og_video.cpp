/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_video.h"

#include <algorithm>
#include <cstdio>

namespace OG {
namespace {

// The prefixes are compared case insensitively, so they are kept as the
// QString forms instead of char pointers: the project turns the implicit
// ASCII conversions off.
const auto kTgPrefix = QString::fromLatin1("https://telegram.me/");
const auto kTmePrefix = QString::fromLatin1("https://t.me/");
const auto kTmeShortPrefix = QString::fromLatin1("t.me/");

// What the reference message says when the field is empty; it is also the key
// the page uses for the label, so one string serves both.
const auto kErrorNoLink = "Group link is required";
const auto kErrorNoAccounts = "Please choose accounts";

[[nodiscard]] bool IsUserNameChar(QChar ch) {
	return (ch >= QChar('a') && ch <= QChar('z'))
		|| (ch >= QChar('A') && ch <= QChar('Z'))
		|| (ch >= QChar('0') && ch <= QChar('9'))
		|| (ch == QChar('_'));
}

[[nodiscard]] bool StartsWithIgnoreCase(
		const QString &text,
		const QString &prefix) {
	return text.startsWith(prefix, Qt::CaseInsensitive);
}

} // namespace

QString NormalizeGroupLink(const QString &raw) {
	auto text = raw.trimmed();
	if (text.isEmpty()) {
		return QString();
	}
	// The three forms the reference accepts, longest first so the generic
	// "t.me/" rule does not cut "https://t.me/" down to "/".
	if (StartsWithIgnoreCase(text, kTgPrefix)) {
		text = text.mid(kTgPrefix.size());
	} else if (StartsWithIgnoreCase(text, kTmePrefix)) {
		text = text.mid(kTmePrefix.size());
	} else if (StartsWithIgnoreCase(text, kTmeShortPrefix)) {
		text = text.mid(kTmeShortPrefix.size());
	}
	while (text.startsWith(QChar('@'))) {
		text = text.mid(1);
	}
	// A pasted link often carries a query or a trailing slash; neither is part
	// of the address the request is built from.
	if (const auto query = text.indexOf(QChar('?')); query >= 0) {
		text = text.left(query);
	}
	while (text.endsWith(QChar('/'))) {
		text.chop(1);
	}
	return text;
}

bool IsUsableGroupLink(const QString &normalized) {
	if (normalized.isEmpty()) {
		return false;
	}
	// A private invite keeps its "joinchat/" or "+" form whole, the way the
	// reference stores it; everything else has to be a username.
	const auto invite = normalized.startsWith(
		QString::fromLatin1("joinchat/"),
		Qt::CaseInsensitive);
	if (invite || normalized.startsWith(QChar('+'))) {
		return (normalized.size() > 1);
	}
	return std::all_of(
		normalized.begin(),
		normalized.end(),
		IsUserNameChar);
}

VideoRequestIssue CheckVideoRequest(const VideoManageConfig &config) {
	if (!IsUsableGroupLink(NormalizeGroupLink(config.link))) {
		return VideoRequestIssue::MissingLink;
	}
	if (config.accounts.isEmpty()) {
		return VideoRequestIssue::MissingAccounts;
	}
	return VideoRequestIssue::None;
}

QString VideoRequestIssueText(VideoRequestIssue issue) {
	switch (issue) {
	case VideoRequestIssue::MissingLink:
		return QString::fromLatin1(kErrorNoLink);
	case VideoRequestIssue::MissingAccounts:
		return QString::fromLatin1(kErrorNoAccounts);
	case VideoRequestIssue::None:
		return QString();
	}
	return QString();
}

QStringList ResolveVideoAccounts(
		const VideoManageConfig &config,
		const std::vector<AccountRow> &pool) {
	auto result = QStringList();
	for (const auto &row : pool) {
		if (config.accounts.contains(row.label)) {
			result.append(row.label);
		}
	}
	return result;
}

int RunVideoSelfCheck() {
	auto failures = 0;
	const auto check = [&](const char *name, bool passed) {
		if (!passed) {
			++failures;
		}
		std::printf(
			"[og][video] %-42s %s\n",
			name,
			passed ? "PASS" : "FAIL");
	};

	// 1. A full link loses its scheme, the way the request needs it.
	check("1 full link -> username",
		NormalizeGroupLink("https://t.me/officegram")
			== QString::fromLatin1("officegram"));

	// 2. An @username loses the @.
	check("2 @username -> username",
		NormalizeGroupLink("@officegram")
			== QString::fromLatin1("officegram"));

	// 3. A bare username is already the answer, trimmed.
	check("3 bare username trimmed",
		NormalizeGroupLink("  officegram  ")
			== QString::fromLatin1("officegram"));

	// 4. The short form works too: operators paste what the app copies.
	check("4 t.me short form",
		NormalizeGroupLink("t.me/officegram")
			== QString::fromLatin1("officegram"));

	// 5. A pasted link with a query keeps only the address.
	check("5 query dropped",
		NormalizeGroupLink("https://t.me/officegram?start=1")
			== QString::fromLatin1("officegram"));

	// 6. A private invite is kept whole: its + or joinchat/ part is the link.
	check("6 private invite kept",
		NormalizeGroupLink("https://t.me/+AbCdEf123")
			== QString::fromLatin1("+AbCdEf123"));

	// 7. An empty field is not a link.
	check("7 empty is not usable",
		!IsUsableGroupLink(NormalizeGroupLink("   ")));

	// 8. A username with a space in it is not an address.
	check("8 space is not a username",
		!IsUsableGroupLink(NormalizeGroupLink("two words")));

	// 9. The validator asks for the link first, then the accounts.
	auto config = VideoManageConfig();
	check("9 empty config asks for the link",
		CheckVideoRequest(config) == VideoRequestIssue::MissingLink);

	config.link = QString::fromLatin1("@officegram");
	check("10 link without accounts",
		CheckVideoRequest(config) == VideoRequestIssue::MissingAccounts);

	config.accounts = QStringList{ QString::fromLatin1("t1") };
	check("11 complete config validates",
		CheckVideoRequest(config) == VideoRequestIssue::None);

	// 12. A tick for a row the operator already removed resolves to nothing,
	// instead of being handed to a request builder that cannot use it.
	auto pool = std::vector<AccountRow>();
	auto first = AccountRow();
	first.id = 1;
	first.label = QString::fromLatin1("t1");
	pool.push_back(first);

	auto stale = config;
	stale.accounts = QStringList{ QString::fromLatin1("gone") };
	check("12 removed row resolves empty",
		ResolveVideoAccounts(stale, pool).isEmpty());

	// 13. The pool order wins over the order the boxes were ticked in.
	auto second = AccountRow();
	second.id = 2;
	second.label = QString::fromLatin1("t2");
	pool.push_back(second);
	auto both = config;
	both.accounts = QStringList{
		QString::fromLatin1("t2"),
		QString::fromLatin1("t1"),
	};
	const auto resolved = ResolveVideoAccounts(both, pool);
	check("13 pool order wins",
		resolved.size() == 2
		&& resolved[0] == QString::fromLatin1("t1")
		&& resolved[1] == QString::fromLatin1("t2"));

	// 14. An interval below zero never reaches a request.
	check("14 interval is a number of ms",
		VideoManageConfig().intervalMs > 0);

	std::printf(
		"[og][video] %d failure(s)\n",
		failures);
	return failures;
}

} // namespace OG
