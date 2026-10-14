/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_core.h"

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
	if (const auto overrideLanguage = qEnvironmentVariable("OG_DEEP_LANG");
		!overrideLanguage.isEmpty()) {
		return overrideLanguage;
	}
	// M2 replaces this with the settings item the reference reads.
	return QString::fromLatin1("en");
}

} // namespace OG
