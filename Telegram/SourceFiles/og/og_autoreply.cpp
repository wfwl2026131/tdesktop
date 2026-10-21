/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_autoreply.h"

#if !defined(OG_AUTOREPLY_SELF_CHECK)
// The diagnostic build below only needs the matcher and the cache, so it is
// compiled without the store and without the service that reads it.
#include "og/og_store.h"

#include <QtCore/QDebug>
#endif // !OG_AUTOREPLY_SELF_CHECK

#include <chrono>
#include <cstddef>

namespace OG {
namespace {

constexpr auto kLinkPrefix = "https://t.me/";

// A public message link is <prefix><name>/<message id>. The real parser
// (PublicMessageLinkParts) belongs to the forwarding milestone; this only
// answers "is the reply a link rather than plain text", which is all the
// decision needs.
[[nodiscard]] bool IsPublicMessageLink(const QString &reply) {
	const auto prefix = QString::fromLatin1(kLinkPrefix);
	if (!reply.startsWith(prefix)) {
		return false;
	}
	const auto rest = reply.mid(prefix.size());
	const auto slash = rest.indexOf(QLatin1Char('/'));
	return (slash > 0) && (slash + 1 < rest.size());
}

// The reference logs "SavedMessages directive detected for keyword '%1'",
// i.e. a keyword that is a command rather than a phrase. The list of commands
// is not visible in the binary, so only the two names the feature itself uses
// are understood here.
[[nodiscard]] bool IsSavedMessagesDirective(const QString &keyword) {
	const auto lowered = keyword.toLower();
	return (lowered == QString::fromLatin1("/saved"))
		|| (lowered == QString::fromLatin1("/savedmessages"));
}

[[nodiscard]] bool Matches(
		const QString &text,
		const QString &keyword,
		MatchMode mode) {
	if (mode == MatchMode::Substring) {
		return text.contains(keyword, Qt::CaseInsensitive);
	}
	return false;
}

} // namespace

std::int64_t DuplicateCache::NowMs() {
	using namespace std::chrono;
	return duration_cast<milliseconds>(
		steady_clock::now().time_since_epoch()).count();
}

bool DuplicateCache::Seen(
		quint64 peerId,
		const QString &keyword,
		int seconds) const {
	if (seconds <= 0) {
		return false;
	}
	const auto now = NowMs();
	const auto window = std::int64_t(seconds) * 1000;
	for (const auto &entry : _entries) {
		if (entry.peerId == peerId
			&& entry.keyword == keyword
			&& (now - entry.at) <= window) {
			return true;
		}
	}
	return false;
}

void DuplicateCache::Remember(quint64 peerId, const QString &keyword) {
	_entries.push_back(Entry{ peerId, keyword, NowMs() });
	while (_entries.size() > std::size_t(kCapacity)) {
		_entries.pop_front();
	}
}

AutoReplyDecision MatchAutoReply(
		const AutoReplyInput &input,
		const std::vector<AutoReplyRule> &rules,
		DuplicateCache &cache,
		int duplicateSeconds,
		MatchMode mode) {
	auto result = AutoReplyDecision();

	// The three skip branches come first and in this order: a channel message
	// never reaches the rule table, an unknown peer type neither, and a group
	// message has to mention the account.
	if (input.kind == PeerKind::Channel) {
		return result;
	} else if (input.kind == PeerKind::Other) {
		return result;
	} else if (input.kind == PeerKind::Group && !input.mentionsMe) {
		return result;
	}

	// The first matching rule wins, so the order of the table is the priority
	// of the rules. A disabled rule never matches; a rule the page would not
	// have created (no keyword or no reply) is skipped as well.
	const AutoReplyRule *hit = nullptr;
	for (const auto &rule : rules) {
		if (!rule.enabled || rule.keyword.isEmpty() || rule.reply.isEmpty()) {
			continue;
		}
		if (Matches(input.text, rule.keyword, mode)) {
			hit = &rule;
			break;
		}
	}
	if (!hit) {
		return result;
	}

	// Only after a keyword matched does the delivered-message cache get a
	// say: the same peer and the same keyword inside the window means the
	// reply has already gone out.
	if (cache.Seen(input.peerId, hit->keyword, duplicateSeconds)) {
		return result;
	}

	result.keyword = hit->keyword;
	result.payload = hit->reply;

	// Link first, directive second, plain text last: the reference prints the
	// link branch before the directive branch, so a link reply is a forward
	// even when the keyword looks like a command.
	if (IsPublicMessageLink(hit->reply)) {
		result.action = AutoReplyDecision::Action::ForwardLink;
	} else if (IsSavedMessagesDirective(hit->keyword)) {
		result.action = AutoReplyDecision::Action::SavedMessages;
	} else {
		result.action = AutoReplyDecision::Action::SendText;
	}

	cache.Remember(input.peerId, hit->keyword);
	return result;
}

#if !defined(OG_AUTOREPLY_SELF_CHECK)
AutoReplyService &AutoReplyService::Instance() {
	static auto instance = AutoReplyService();
	return instance;
}

void AutoReplyService::setSendHook(SendTextHook hook) {
	_hook = std::move(hook);
}

AutoReplyDecision AutoReplyService::handle(const AutoReplyInput &input) {
	const auto &store = StoreInstance();
	const auto settings = store.autoReplySettings();
	if (!settings.enabled) {
		return AutoReplyDecision();
	}

	auto decision = MatchAutoReply(
		input,
		store.autoReplyRules(),
		_cache,
		settings.duplicateSeconds);
	if (decision.action != AutoReplyDecision::Action::SendText) {
		// Everything else is only reported to the caller: this milestone does
		// not forward links and does not search SavedMessages.
		return decision;
	}

	if (_hook) {
		_hook(input.peerId, decision.payload);
	} else {
		qDebug() << "[og][autoreply] send text to" << input.peerId
			<< decision.payload;
	}
	return decision;
}

#endif // !OG_AUTOREPLY_SELF_CHECK

#if defined(OG_AUTOREPLY_SELF_CHECK)

#include <cstdio>

namespace {

[[nodiscard]] const char *ActionToken(AutoReplyDecision::Action action) {
	switch (action) {
	case AutoReplyDecision::Action::None: return "None";
	case AutoReplyDecision::Action::SendText: return "SendText";
	case AutoReplyDecision::Action::ForwardLink: return "ForwardLink";
	case AutoReplyDecision::Action::SavedMessages: return "SavedMessages";
	}
	return "?";
}

[[nodiscard]] AutoReplyInput MakeInput(
		PeerKind kind,
		bool mentionsMe,
		const char *text,
		quint64 peerId) {
	auto input = AutoReplyInput();
	input.kind = kind;
	input.mentionsMe = mentionsMe;
	input.text = QString::fromUtf8(text);
	input.peerId = peerId;
	return input;
}

} // namespace

// The six acceptance cases of the task book, compiled only when the macro is
// defined by hand: the build never defines it, so nothing of this reaches the
// product. To run them, compile this file with -DOG_AUTOREPLY_SELF_CHECK
// together with a driver that calls RunAutoReplySelfCheck().
void RunAutoReplySelfCheck() {
	auto rules = std::vector<AutoReplyRule>();

	auto price = AutoReplyRule();
	price.keyword = QString::fromLatin1("price");
	price.reply = QString::fromLatin1("Our price list is in the pinned post.");
	price.enabled = 1;
	rules.push_back(price);

	auto link = AutoReplyRule();
	link.keyword = QString::fromLatin1("catalog");
	link.reply = QString::fromLatin1("https://t.me/officegram/42");
	link.enabled = 1;
	rules.push_back(link);

	auto saved = AutoReplyRule();
	saved.keyword = QString::fromLatin1("/saved");
	saved.reply = QString::fromLatin1("search");
	saved.enabled = 1;
	rules.push_back(saved);

	auto cache = DuplicateCache();
	auto failures = 0;
	const auto check = [&](
			const char *name,
			const AutoReplyDecision &decision,
			AutoReplyDecision::Action expected) {
		const auto passed = (decision.action == expected);
		if (!passed) {
			++failures;
		}
		std::printf(
			"[og][autoreply] %-34s action=%-13s expected=%-13s %s\n",
			name,
			ActionToken(decision.action),
			ActionToken(expected),
			passed ? "PASS" : "FAIL");
	};

	// 1. Private chat, keyword inside a longer text: the reply goes out.
	const auto first = MakeInput(
		PeerKind::Private,
		false,
		"Hello, what is the price?",
		5001);
	check("1 private hit", MatchAutoReply(first, rules, cache, 60),
		AutoReplyDecision::Action::SendText);

	// 2. Private chat without any keyword: nothing happens.
	check("2 private miss", MatchAutoReply(MakeInput(
		PeerKind::Private,
		false,
		"Good morning",
		5002), rules, cache, 60),
		AutoReplyDecision::Action::None);

	// 3. A channel message is skipped whatever it says.
	check("3 channel skipped", MatchAutoReply(MakeInput(
		PeerKind::Channel,
		false,
		"what is the price?",
		5003), rules, cache, 60),
		AutoReplyDecision::Action::None);

	// 4. A group message that does not mention the account is skipped.
	check("4 group without mention", MatchAutoReply(MakeInput(
		PeerKind::Group,
		false,
		"what is the price?",
		5004), rules, cache, 60),
		AutoReplyDecision::Action::None);

	// 5. The same text in a group that does mention the account matches.
	check("5 group with mention", MatchAutoReply(MakeInput(
		PeerKind::Group,
		true,
		"what is the price?",
		5004), rules, cache, 60),
		AutoReplyDecision::Action::SendText);

	// 6. The very first message again, inside the duplicate window: the
	// cache holds peer 5001 with the keyword, so the second one is skipped.
	check("6 duplicate inside window", MatchAutoReply(first, rules, cache, 60),
		AutoReplyDecision::Action::None);

	std::printf(
		"[og][autoreply] self-check done: %d case(s) failed\n",
		failures);
}

#endif // OG_AUTOREPLY_SELF_CHECK

} // namespace OG
