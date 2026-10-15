/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_core.h"

#include "lang/lang_keys.h"

#include <cstdlib>

namespace OG {

QString Config::serverBase() {
	if (const auto overrideUrl = qEnvironmentVariable("OG_CRM_URL");
		!overrideUrl.isEmpty()) {
		return overrideUrl;
	}
	// LAN dev relay on the Mac host (see 12-STATE-SNAPSHOT.md, ogweb 8080).
	return QString::fromUtf8("http://192.168.217.1:8080");
}

QString Config::languageCode() {
	if (const auto overrideLanguage = QString::fromLocal8Bit(qgetenv("OG_DEEP_LANG"));
		!overrideLanguage.isEmpty()) {
		return overrideLanguage;
	}
	// The reference takes it from a settings item; in this fork the live value
	// is the UI language pack the window is running in.
	const auto current = Lang::Id();
	if (current.isEmpty()) {
		return QString::fromLatin1("en");
	}
	// One Chinese pack ships with the build, so every Chinese variant uses it.
	if (current.startsWith(QString::fromLatin1("zh"), Qt::CaseInsensitive)) {
		return QString::fromLatin1("zh-hans");
	}
	return current;
}

} // namespace OG
