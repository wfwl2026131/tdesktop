/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace OG {

// The phone list page of the reference takes a pasted block of numbers, keeps
// the ones that can be a phone number, and drops the ones it has already seen.
// The rules are small enough to write down, and every page that carries a
// number list needs the same ones, so they live here and not in a page.
//
// A number is stored in one shape only: a plus sign and the digits, no
// spaces, no dashes, no brackets. Two ways of writing the same number must
// collapse to the same string, or a duplicate survives and the run messages
// the same person twice. The reference prints the count of both kinds
// ("%1 numbers, %2 duplicates"), which is why the two are counted apart.

// A phone number in the one shape this fork stores: '+' followed by 7 to 15
// digits. The empty string when the text cannot be one.
//
// Known limit, written down because a wrong guess here messages a stranger: a
// number pasted in its national form with a trunk zero (0138... instead of
// 138...) is NOT repaired. Repairing it needs the country table, and a wrong
// country code is worse than a rejected number, so the leading zero stays and
// the number is what the operator pasted.
[[nodiscard]] QString NormalizePhoneNumber(const QString &text);

// The pasted block split into numbers. A block is separated by commas,
// semicolons, tabs, pipes or any kind of line break. A space is NOT a
// separator: it is how a number is written ("+86 138 0000 0000"), and treating
// it as one turns a single number into four rejected fragments. Every entry is
// normalized, and an entry that cannot be one is reported rather than dropped
// silently: a filter that hides its rejects looks like it worked while it did
// not.
struct PhoneList {
	QStringList numbers; // normalized, in the order they first appeared
	QStringList duplicates; // normalized, seen again after the first
	QStringList rejected; // the entries as they were typed, trimmed
};

[[nodiscard]] PhoneList SplitPhoneList(const QString &text);

// The page's two intervals, from the reference's own configuration file
// filter_phone_config.json: one gap between accounts, one gap between two
// numbers of the same account.
struct PhoneFilterConfig {
	int globalIntervalMs = 1000; // the reference default of the page
	int singleIntervalMs = 0; // 0 = no extra gap per number
};

// Reads the two keys of that file. Anything it cannot read keeps its default,
// so a half written configuration cannot turn the pacing into zero.
[[nodiscard]] PhoneFilterConfig ParsePhoneFilterConfig(const QString &json);

[[nodiscard]] QString WritePhoneFilterConfig(const PhoneFilterConfig &config);

int RunPhoneSelfCheck();

} // namespace OG
