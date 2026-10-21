/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>

#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

namespace OG {

// Auto-reply slice of the fork: the rule model, the matcher and a service that
// keeps the delivered-message cache.
//
// The order of the checks below is not ours: the reference client prints the
// decision tokens and they are contiguous in its binary, so the sequence is
// its priority order and every branch here follows it one to one.
//
//   Receive [%1] from %2: %3
//   Skip channel message from %1
//   Private chat from %1, auto-reply allowed
//   Group message mentions me from %1
//   Skip unknown peer type from %1
//   Auto-reply triggered for keyword '%1'
//   Skip auto-reply for cached message from %1 with keyword '%2'
//   Detected public message link reply %1, forwarding instead of plain text
//   SavedMessages directive detected for keyword '%1'

// The rule table in memory. The reference keeps the same fields in
// tdata/auto_reply_config.json rather than in its database, so the column set
// is ours; the two names that config publishes are kept verbatim (keyword, and
// the reply it calls replyContent). enabled exists for the per-rule switch the
// reference does not show yet, so the page does not expose it.
struct AutoReplyRule {
	qint64 id = 0;
	QString keyword; // the config key "keyword"
	QString reply; // the config key "replyContent"
	int enabled = 1;
	qint64 created_at = 0;
	qint64 updated_at = 0;
};

// The global switches of the page: one for the whole feature and one for the
// duplicate window. They are settings of the store, not a table, because
// there is one row of them for the whole client.
struct AutoReplySettings {
	bool enabled = true; // "Enabled Reply Automatically"
	int duplicateSeconds = 60; // "Duplicate Cache(Seconds):"
};

// Who the message came from, as far as the decision cares.
enum class PeerKind {
	Private,
	Group,
	Channel,
	Other,
};

// How a rule keyword is compared with the incoming text. The reference only
// prints "Auto-reply triggered for keyword '%1'", which does not tell a
// substring rule from a regular expression, so substring is what this
// milestone ships; the enum exists so a later mode can be added without
// touching any caller.
enum class MatchMode {
	Substring,
};

struct AutoReplyInput {
	PeerKind kind = PeerKind::Private;
	bool mentionsMe = false;
	QString text;
	quint64 peerId = 0;
};

struct AutoReplyDecision {
	enum class Action {
		None,
		SendText,
		ForwardLink,
		SavedMessages,
	};
	Action action = Action::None;
	QString keyword; // the keyword of the rule that matched
	QString payload; // the text to send, or the link to forward

	[[nodiscard]] bool isNone() const {
		return action == Action::None;
	}
};

// The delivered-message cache of the reference ("cached message"). It is
// intentionally memory only: the reference names a cache and nothing shows it
// being written to disk, so losing it on restart is acceptable.
//
// Keyed by peer and keyword; an entry older than the configured window is not
// a hit any more. The oldest entry is dropped once kCapacity is exceeded.
class DuplicateCache {
public:
	// seconds <= 0 means the window is off: nothing is reported as seen.
	[[nodiscard]] bool Seen(
		quint64 peerId,
		const QString &keyword,
		int seconds) const;
	void Remember(quint64 peerId, const QString &keyword);

	static constexpr auto kCapacity = 512;

private:
	struct Entry {
		quint64 peerId = 0;
		QString keyword;
		std::int64_t at = 0;
	};

	[[nodiscard]] static std::int64_t NowMs();

	std::deque<Entry> _entries;
};

// Decides what one incoming message should trigger. A decision other than
// None is remembered in the cache, because the caller is expected to act on
// it; the reference treats a triggered keyword as delivered at that point.
[[nodiscard]] AutoReplyDecision MatchAutoReply(
	const AutoReplyInput &input,
	const std::vector<AutoReplyRule> &rules,
	DuplicateCache &cache,
	int duplicateSeconds,
	MatchMode mode = MatchMode::Substring);

// What the sender does with a SendText decision. The real MTP path is a later
// milestone: this build never constructs a session, so the hook is the only
// way out of the slice.
using SendTextHook = std::function<void(quint64 peerId, const QString &text)>;

class AutoReplyService final {
public:
	[[nodiscard]] static AutoReplyService &Instance();

	// Replaces the sender. The default one writes a log line, which is what a
	// build without a message pipeline can honestly do.
	void setSendHook(SendTextHook hook);

	// Reads the rules and the global settings from the store, decides, and
	// hands a SendText decision to the hook. Every other action is returned
	// untouched: forwarding a public link and searching SavedMessages are
	// later milestones.
	[[nodiscard]] AutoReplyDecision handle(const AutoReplyInput &input);

private:
	AutoReplyService() = default;

	DuplicateCache _cache;
	SendTextHook _hook;
};

} // namespace OG
