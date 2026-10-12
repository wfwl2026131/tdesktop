/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QString>

namespace OG {

struct Config {
	// ogweb REST base. Env OG_CRM_URL overrides the LAN dev default.
	static QString serverBase();

	// Seat limit mirror for the UI skeleton; og_gate (M2) replaces this
	// with the live /get-account-info value from the server.
	static constexpr int kSeatLimit = 200;
};

} // namespace OG
