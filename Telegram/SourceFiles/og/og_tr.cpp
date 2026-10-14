/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_tr.h"

#include "og/og_core.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QTranslator>

namespace OG {
namespace Tr {
namespace {

// Evidence (reference client): the only Deep translations path in .rdata.
constexpr auto kResourceTemplate = ":/deep/translations/deep_%1.qm";

QTranslator *gDeep = nullptr;
QString gLanguage;

[[nodiscard]] QString ResourceName(const QString &language) {
	return QString::fromLatin1(kResourceTemplate).arg(language);
}

bool LoadInto(QTranslator *translator, const QString &language) {
	return translator->load(ResourceName(language));
}

} // namespace

void Start() {
	if (gDeep) {
		return;
	}
	gDeep = new QTranslator();

	const auto requested = Config::languageCode();
	const auto primary = ResourceName(requested);
	qDebug("Attempting to load Deep translations from: %s", qPrintable(primary));
	if (LoadInto(gDeep, requested)) {
		QCoreApplication::installTranslator(gDeep);
		gLanguage = requested;
		qDebug("Deep translations loaded from: %s", qPrintable(primary));
		return;
	}

	qDebug(
		"Failed to load Deep translations for language: %s - trying English fallback",
		qPrintable(requested));
	const auto fallback = ResourceName(QString::fromLatin1("en"));
	qDebug("Attempting fallback load from: %s", qPrintable(fallback));
	if (LoadInto(gDeep, QString::fromLatin1("en"))) {
		QCoreApplication::installTranslator(gDeep);
		gLanguage = QString::fromLatin1("en");
		return;
	}

	qDebug("CRITICAL: Failed to load Deep translations");
	// The English source strings in Lookup() call sites stay in place, so the
	// interface still renders - the same last step the reference logs before
	// it keeps running.
	gLanguage = QString::fromLatin1("en");
}

QString Lookup(const char *key) {
	return key ? Lookup(QString::fromUtf8(key)) : QString();
}

QString Lookup(const QString &key) {
	const auto separator = key.indexOf(QString::fromLatin1("::"));
	if (separator <= 0) {
		return key;
	}
	const auto context = key.left(separator).toUtf8();
	const auto source = key.mid(separator + 2).toUtf8();
	return QCoreApplication::translate(
		context.constData(),
		source.constData());
}

QString LanguageCode() {
	return gLanguage.isEmpty() ? Config::languageCode() : gLanguage;
}

} // namespace Tr
} // namespace OG
