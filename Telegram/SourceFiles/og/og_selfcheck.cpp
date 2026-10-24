/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_selfcheck.h"

#include "og/og_accounts.h"
#include "og/og_pacer.h"
#include "og/og_phone.h"
#include "og/og_video.h"

#include <QtCore/QString>

#include <cstdio>

namespace OG {

int RunAllSelfChecks() {
	// Each slice prints its own lines and returns the number it failed, so one
	// run of this function is the whole evidence for the pure logic of the
	// fork. The auto-reply matcher is not here: its cases are behind
	// OG_AUTOREPLY_SELF_CHECK and this build never defines it.
	std::printf("=== og self checks ===\n");

	auto failures = 0;
	failures += RunAccountsSelfCheck();
	failures += RunVideoSelfCheck();
	failures += RunPacerSelfCheck();
	failures += RunPhoneSelfCheck();

	std::printf(
		"=== og self checks: %d failure(s) ===\n",
		failures);
	return failures;
}

bool SelfChecksRequested() {
	// The acceptance run sets it; a normal launch never does. Anything but an
	// explicit off counts as on, so OG_SELFCHECK=1, =yes and =true all work and
	// a typo cannot silently skip the checks.
	const auto value = qEnvironmentVariable("OG_SELFCHECK")
		.trimmed()
		.toLower();
	return !value.isEmpty()
		&& (value != QString::fromLatin1("0"))
		&& (value != QString::fromLatin1("false"));
}

} // namespace OG
