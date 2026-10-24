/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace OG {

// Every slice of this fork keeps its rules in a self-check: a fixed set of
// inputs and the result each one must produce, printed as PASS or FAIL. The
// checks are not part of the runtime, they are the driver an acceptance run
// builds, which is why nothing calls them at startup by default.
//
// RunAllSelfChecks() runs all of them in one go and returns the number of
// failures, so a script can call it and read the exit code. It is meant for a
// build with the checks in it, not for a normal launch.
int RunAllSelfChecks();

// True when the environment asks for the checks at startup (OG_SELFCHECK=1).
// main() consults it once, before any window exists, and prints the result.
[[nodiscard]] bool SelfChecksRequested();

} // namespace OG
