/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_phone.h"

#include "og/og_pacer.h"

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <algorithm>
#include <cstdio>

namespace OG {
namespace {

constexpr auto kMinDigits = 7;
constexpr auto kMaxDigits = 15;

// A separator is anything that is not a digit: spaces, dashes, brackets,
// dots. The reference accepts a number written the way a phone book writes it,
// and the shape is decided by the digits alone.
[[nodiscard]] bool IsDigit(QChar ch) {
	return (ch >= QChar('0')) && (ch <= QChar('9'));
}

[[nodiscard]] QString DigitsOf(const QString &text) {
	auto digits = QString();
	digits.reserve(text.size());
	for (const auto ch : text) {
		if (IsDigit(ch)) {
			digits.append(ch);
		}
	}
	return digits;
}

[[nodiscard]] bool IsSeparator(QChar ch) {
	return (ch == QChar(','))
		|| (ch == QChar(';'))
		|| (ch == QChar('\n'))
		|| (ch == QChar('\r'))
		|| (ch == QChar('\t'))
		|| (ch == QChar('|'));
}

[[nodiscard]] int ReadInt(
		const QJsonObject &object,
		const char *key,
		int fallback) {
	const auto value = object.value(QString::fromLatin1(key));
	return value.isDouble() ? int(value.toDouble()) : fallback;
}

} // namespace

QString NormalizePhoneNumber(const QString &text) {
	auto digits = DigitsOf(text);
	if (digits.isEmpty()) {
		return QString();
	}
	// The international prefix is written three ways and means the same
	// number: "+", "00", or nothing at all when the number already starts with
	// the country code. "00" is the one form that has to go, because the plus
	// is what the stored shape carries.
	while (digits.startsWith(QString::fromLatin1("00"))) {
		digits.remove(0, 2);
	}
	if ((digits.size() < kMinDigits) || (digits.size() > kMaxDigits)) {
		return QString();
	}
	return QString(QChar('+')) + digits;
}

PhoneList SplitPhoneList(const QString &text) {
	auto result = PhoneList();
	auto seen = QStringList();
	auto entry = QString();
	const auto flush = [&] {
		const auto trimmed = entry.trimmed();
		entry.clear();
		if (trimmed.isEmpty()) {
			return;
		}
		const auto number = NormalizePhoneNumber(trimmed);
		if (number.isEmpty()) {
			result.rejected.append(trimmed);
		} else if (seen.contains(number)) {
			result.duplicates.append(number);
		} else {
			seen.append(number);
			result.numbers.append(number);
		}
	};
	for (const auto ch : text) {
		if (IsSeparator(ch)) {
			flush();
		} else {
			entry.append(ch);
		}
	}
	flush();
	return result;
}

PhoneFilterConfig ParsePhoneFilterConfig(const QString &json) {
	auto result = PhoneFilterConfig();
	const auto document = QJsonDocument::fromJson(json.toUtf8());
	if (!document.isObject()) {
		return result;
	}
	const auto object = document.object();
	// The reference keeps both keys in milliseconds and both are gaps, so a
	// negative one is a typo and reads as the default rather than as zero.
	result.globalIntervalMs = std::max(
		ReadInt(object, "globalInterval", result.globalIntervalMs),
		0);
	result.singleIntervalMs = std::max(
		ReadInt(object, "singleInterval", result.singleIntervalMs),
		0);
	return result;
}

QString WritePhoneFilterConfig(const PhoneFilterConfig &config) {
	auto object = QJsonObject();
	object.insert(
		QString::fromLatin1("globalInterval"),
		std::max(config.globalIntervalMs, 0));
	object.insert(
		QString::fromLatin1("singleInterval"),
		std::max(config.singleIntervalMs, 0));
	return QString::fromUtf8(
		QJsonDocument(object).toJson(QJsonDocument::Indented));
}

void RunPhoneSelfCheck() {
	auto failures = 0;
	const auto check = [&](const char *name, bool passed) {
		if (!passed) {
			++failures;
		}
		std::printf("[og][phone] %-46s %s\n", name, passed ? "PASS" : "FAIL");
	};
	const auto phone = [](const char *text) {
		return NormalizePhoneNumber(QString::fromUtf8(text));
	};
	const auto same = [](const QString &actual, const char *expected) {
		return actual == QString::fromUtf8(expected);
	};

	// 1-4. The four ways the same number is written collapse to one string.
	check("1 plus and country code",
		same(phone("+86 138 0000 0000"), "+8613800000000"));
	check("2 double zero prefix",
		same(phone("0086 138 0000 0000"), "+8613800000000"));
	check("3 dashes and brackets",
		same(phone("+1 (555) 010-9999"), "+15550109999"));
	// A national number with its trunk zero is not repaired: the country table
	// would be needed, and a wrong country code messages a stranger. The zero
	// stays, and this assertion is the limit written down.
	check("4 trunk zero is kept",
		same(phone("+86 0138 0000 0000"), "+86013800000000"));

	// 5-8. What cannot be a number is refused, and the reason is the digits,
	// not the punctuation around them.
	check("5 too short", phone("+12 345").isEmpty());
	check("6 too long", phone("+1234567890123456").isEmpty());
	check("7 letters only", phone("call me").isEmpty());
	check("8 empty", phone("   ").isEmpty());

	// 9. A number with no plus still reads: the pasted column often has none.
	check("9 no plus",
		same(phone("13800000000"), "+13800000000"));

	// 10. A list keeps the order of first appearance and counts the repeats.
	const auto list = SplitPhoneList(QString::fromUtf8(
		"+86 138 0000 0000\n"
		"0086 138 0000 0000\n"
		"+1 (555) 010-9999\n"
		"nonsense\n"
		"+1 555 010 9999\n"));
	check("10 list order", list.numbers.size() == 2
		&& same(list.numbers[0], "+8613800000000")
		&& same(list.numbers[1], "+15550109999"));
	check("11 list duplicates", list.duplicates.size() == 2
		&& same(list.duplicates[0], "+8613800000000")
		&& same(list.duplicates[1], "+15550109999"));
	check("12 list rejects keep their text",
		list.rejected.size() == 1
		&& same(list.rejected[0], "nonsense"));

	// 13. A comma separated paste is separated by commas, not only by lines.
	const auto commas = SplitPhoneList(QString::fromUtf8(
		"+8613800000000, +8613900000000; +8613700000000"));
	check("13 commas and semicolons separate",
		commas.numbers.size() == 3);

	// 14. The configuration reads the reference's two keys and keeps the
	// defaults for anything missing.
	const auto parsed = ParsePhoneFilterConfig(QString::fromUtf8(
		"{ \"globalInterval\": 2500, \"singleInterval\": 400 }"));
	check("14 config keys", parsed.globalIntervalMs == 2500
		&& parsed.singleIntervalMs == 400);
	const auto partial = ParsePhoneFilterConfig(QString::fromUtf8("{}"));
	check("15 empty config keeps the defaults",
		partial.globalIntervalMs == PhoneFilterConfig().globalIntervalMs);
	const auto broken = ParsePhoneFilterConfig(QString::fromUtf8("not json"));
	check("16 broken config keeps the defaults",
		broken.globalIntervalMs == PhoneFilterConfig().globalIntervalMs);

	// 17. What the page writes reads back the same.
	auto written = PhoneFilterConfig();
	written.globalIntervalMs = 1200;
	written.singleIntervalMs = 300;
	const auto roundTrip = ParsePhoneFilterConfig(
		WritePhoneFilterConfig(written));
	check("17 config round trip", roundTrip.globalIntervalMs == 1200
		&& roundTrip.singleIntervalMs == 300);

	// 18. The two intervals feed the shared pacer unchanged: the number gap is
	// the account gap, and the global one is the pool gap.
	auto pacerConfig = PacerConfig();
	pacerConfig.requestIntervalMs = parsed.globalIntervalMs;
	pacerConfig.accountIntervalMs = parsed.singleIntervalMs;
	check("18 phone gaps feed the pacer",
		EffectiveAccountInterval(pacerConfig) == 2500);

	std::printf("[og][phone] %d failure(s)\n", failures);
	return failures;
}

} // namespace OG
