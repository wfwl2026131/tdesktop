/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_accounts.h"

#include <cstdio>

namespace OG {
namespace {

const auto kKindFrozen = "frozen";
const auto kKindOneWay = "one_way";
const auto kKindTwoWay = "two_way";

const auto kStatusNew = "new";
const auto kStatusOk = "ok";
const auto kStatusLoggedOut = "logged-out";
const auto kStatusLimited = "limited";

[[nodiscard]] bool IsAsciiDigit(QChar ch) {
	return (ch >= QChar('0')) && (ch <= QChar('9'));
}

// A phone number the MTP layer accepts is a plus and then digits, or bare
// digits; everything a human types around them is separators.
[[nodiscard]] QString DigitsOf(const QString &raw) {
	auto digits = QString();
	digits.reserve(raw.size());
	for (const auto ch : raw) {
		if (IsAsciiDigit(ch)) {
			digits.append(ch);
		}
	}
	return digits;
}

} // namespace

QString AccountKindToken(AccountKind kind) {
	switch (kind) {
	case AccountKind::Frozen: return kKindFrozen;
	case AccountKind::OneWay: return kKindOneWay;
	case AccountKind::TwoWay: return kKindTwoWay;
	case AccountKind::Unknown: return QString();
	}
	return QString();
}

AccountKind AccountKindFromToken(const QString &token) {
	if (token == kKindFrozen) {
		return AccountKind::Frozen;
	} else if (token == kKindOneWay) {
		return AccountKind::OneWay;
	} else if (token == kKindTwoWay) {
		return AccountKind::TwoWay;
	}
	return AccountKind::Unknown;
}

bool IsKnownAccountKind(AccountKind kind) {
	return (kind != AccountKind::Unknown);
}

QString AccountStatusToken(const QString &status) {
	if (status == kStatusNew
		|| status == kStatusOk
		|| status == kStatusLoggedOut
		|| status == kStatusLimited) {
		return status;
	}
	return kStatusNew;
}

bool IsKnownAccountStatus(const QString &status) {
	return (status == kStatusNew)
		|| (status == kStatusOk)
		|| (status == kStatusLoggedOut)
		|| (status == kStatusLimited);
}

QString NormalizePhone(const QString &raw) {
	const auto digits = DigitsOf(raw);
	if (digits.isEmpty()) {
		return QString();
	}
	// A number short enough to be a local prefix is kept as typed minus the
	// separators, without the plus: the pool shows what the operator entered
	// and only a fully qualified number gets the plus the client needs.
	if (digits.size() < 7) {
		return digits;
	}
	return QString(QChar('+')) + digits;
}

QString NormalizeAccountLabel(const QString &raw) {
	return raw.trimmed();
}

bool PoolAccepts(
		const AccountRow &candidate,
		const AccountRow *byLabel,
		const AccountRow *byPhone) {
	const auto label = NormalizeAccountLabel(candidate.label);
	const auto phone = NormalizePhone(candidate.phone);
	if (label.isEmpty() || phone.isEmpty()) {
		return false;
	}
	if (byLabel && (byLabel->id != candidate.id)) {
		return false;
	}
	if (byPhone && (byPhone->id != candidate.id)) {
		return false;
	}
	return true;
}

void RunAccountsSelfCheck() {
	auto failures = 0;
	const auto check = [&](const char *name, bool passed) {
		if (!passed) {
			++failures;
		}
		std::printf(
			"[og][accounts] %-38s %s\n",
			name,
			passed ? "PASS" : "FAIL");
	};

	// 1. The separators a person types around a number are not part of it,
	// and a fully qualified number keeps the plus the client needs.
	check("1 +66 66-11 06 045 normalises",
		NormalizePhone("+66 66-11 06 045") == QString::fromLatin1("+66661106045"));

	// 2. A bare number with spaces gets the plus too.
	check("2 66661106045 gets a plus",
		NormalizePhone("66661106045") == QString::fromLatin1("+66661106045"));

	// 3. A short local prefix stays as digits: no plus is invented.
	check("3 short prefix keeps no plus",
		NormalizePhone("0 611 06") == QString::fromLatin1("061106"));

	// 4. Letters are dropped, not guessed at. What is left is short, so no
	// plus is added: the plus belongs to a number that can be dialled.
	check("4 letters are dropped",
		NormalizePhone("+66abc611") == QString::fromLatin1("66611"));

	// 5. Nothing left means the row is not a number at all.
	check("5 nothing left is empty", NormalizePhone("(())").isEmpty());

	// 6. A label is trimmed, and an empty one is not a label.
	check("6 label trimmed",
		NormalizeAccountLabel("  t1 ") == QString::fromLatin1("t1"));
	check("7 empty label rejected", NormalizeAccountLabel("   ").isEmpty());

	// 8. An empty candidate never enters the pool.
	const auto empty = AccountRow();
	check("8 empty row rejected", !PoolAccepts(empty, nullptr, nullptr));

	// 9. A row that is already in the pool under the same label is refused.
	auto existing = AccountRow();
	existing.id = 7;
	existing.label = QString::fromLatin1("t1");
	existing.phone = QString::fromLatin1("+66661106045");
	auto sameLabel = AccountRow();
	sameLabel.id = 0;
	sameLabel.label = QString::fromLatin1("t1");
	sameLabel.phone = QString::fromLatin1("+61400000000");
	check("9 duplicate label rejected",
		!PoolAccepts(sameLabel, &existing, nullptr));

	// 10. The same number under another label is refused as well: two
	// sessions for one number is how both of them get limited.
	auto samePhone = AccountRow();
	samePhone.id = 0;
	samePhone.label = QString::fromLatin1("t2");
	samePhone.phone = QString::fromLatin1("+66 66 11 06 045");
	check("10 duplicate number rejected",
		!PoolAccepts(samePhone, nullptr, &existing));

	// 11. Editing the row it already is stays allowed.
	auto editing = existing;
	editing.phone = QString::fromLatin1("+66661106045");
	check("11 editing itself allowed",
		PoolAccepts(editing, &existing, &existing));

	// 12. The kind tokens round trip, and an unknown one is not folded into
	// the three real buckets.
	check("12 kind tokens round trip",
		AccountKindFromToken(AccountKindToken(AccountKind::TwoWay))
			== AccountKind::TwoWay);
	check("13 unknown token stays unknown",
		AccountKindFromToken(QString::fromLatin1("whatever"))
			== AccountKind::Unknown);
	check("14 unknown kind is not a bucket",
		!IsKnownAccountKind(AccountKind::Unknown));

	// 15. A status the pool does not record reads as new instead of being
	// shown to the operator as something nobody wrote.
	check("15 foreign status reads as new",
		AccountStatusToken(QString::fromLatin1("bogus"))
			== QString::fromLatin1("new"));
	check("16 known status kept",
		IsKnownAccountStatus(QString::fromLatin1("logged-out")));

	std::printf(
		"[og][accounts] %d failure(s)\n",
		failures);
}

} // namespace OG
