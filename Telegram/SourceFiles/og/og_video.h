/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "og/og_accounts.h"

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <vector>

namespace OG {

// The group video page: one group link, one interval, a tick per account and
// the two actions the reference offers (join and leave a group call).
//
// The strings below are the ones the reference prints, taken from its binary
// and kept verbatim, so the page can be read against it line by line:
//
//   Active Accounts
//   Group link or @username
//   Interval (ms)
//   Join
//   Leave
//   Task is running
//   Group link is required
//   Please choose accounts
//   Account %1 joined
//   Account %1 failed to join
//   Account %1 left
//   Account %1 failed to leave
//   %1 success / %2 failed   %3
//
// Its configuration file is video_manage_config.json and it publishes the
// interval under the key `intervalMs`; that spelling is kept for the store
// keys so a moved configuration still reads.
//
// Nothing in this file talks to Telegram. The window decides what one request
// would be; sending it belongs to the milestone that owns the session.

enum class GroupCallAction {
	Join,
	Leave,
};

struct VideoManageConfig {
	QString link; // "Group link or @username"
	int intervalMs = 1000; // "Interval (ms)", the config key "intervalMs"
	QStringList accounts; // labels of the pool rows that take part
};

// The reference accepts a full t.me link, an @username, or a bare username,
// and never keeps the scheme or the @ once the request is built.
[[nodiscard]] QString NormalizeGroupLink(const QString &raw);

// True when what is left after normalising can be a group address: a
// username shaped token (letters, digits, underscore) or a t.me/joinchat
// invite that the reference stores whole.
[[nodiscard]] bool IsUsableGroupLink(const QString &normalized);

// What stops the request from being built. The order is the reference's:
// the link first, then the accounts.
enum class VideoRequestIssue {
	None,
	MissingLink,
	MissingAccounts,
};

[[nodiscard]] VideoRequestIssue CheckVideoRequest(
	const VideoManageConfig &config);

// The message the reference shows for that issue, empty when there is none.
[[nodiscard]] QString VideoRequestIssueText(VideoRequestIssue issue);

// The account rows that take part, in pool order, skipping labels that are no
// longer in the pool: a page that kept a stale tick would otherwise work on a
// row the operator already removed.
[[nodiscard]] QStringList ResolveVideoAccounts(
	const VideoManageConfig &config,
	const std::vector<AccountRow> &pool);

// Prints the rules above against a fixed set of inputs. Not called from the
// client: it is the driver the acceptance run builds.
int RunVideoSelfCheck();

} // namespace OG
