/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/timer.h"
#include "base/weak_ptr.h"

#include <QtCore/QString>

#include <cstddef>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace OG {

// Local data layer of the fork.
//
// The reference client keeps this data in SQLite; our build has no SQL module
// linked, so the same four tables are stored as a single JSON document under
// <working dir>/og/store.json and indexed in memory. Storage may be swapped
// later without touching the callers.
//
// The member names below are the column names of the reference schema,
// verbatim and in snake_case, so a field can be matched between the binary,
// the JSON keys and this header without any translation step. The structs are
// plain data on purpose: no logic, no validation beyond the identity keys.

// Todo states, as in the reference: 1 = pending, 2 = done, 3 = archived.
inline constexpr auto kTodoStatePending = 1;
inline constexpr auto kTodoStateDone = 2;
inline constexpr auto kTodoStateArchived = 3;

// Table "from_todo_list".
struct TodoEntry {
	qint64 id = 0;
	qint64 from_id = 0;
	QString from_name;
	QString from_avatar;
	qint64 peer_id = 0;
	QString peer_name;
	QString peer_avatar;
	qint64 receiver_id = 0;
	int chat_type = 0;
	int msg_type = 0;
	int unread_count = 0;
	int from_count = 0;
	bool is_new = false;

	// Not a column of the reference table. The state has to be stored
	// somewhere for setTodoState() to mean anything, and none of the thirteen
	// columns above is a state: is_new is a flag of its own, and the archive
	// table is a separate list. Zero means "not specified" and never
	// overwrites a state already known. Written as the "state" JSON key.
	int state = 0;
};

// Table "from_todo_topic", UNIQUE(todo_id, chat_peer_id): writing the same
// pair twice updates the row instead of appending another one.
struct TodoTopic {
	qint64 todo_id = 0;
	qint64 chat_peer_id = 0;
	qint64 topic_id = 0;
	qint64 dispatcher_id = 0;
	bool is_blacklist = false;
	QString tag_group;
	QString reserve1;
	QString reserve2;
	QString reserve3;
	QString reserve4;
	QString reserve5;
	QString reserve6;
	qint64 created_at = 0;
	qint64 updated_at = 0;
};

// Table "from_todo_archive".
struct TodoArchive {
	qint64 archive_id = 0;
	qint64 from_id = 0;
	qint64 peer_id = 0;
	qint64 receiver_id = 0;
	int chat_type = 0;
	int msg_type = 0;
	QString title;
	qint64 msg_id = 0;
	QString msg;
	QString media;
	QString reserve1;
	QString reserve2;
	QString reserve3;
};

// Table "translation", PRIMARY KEY (input, lg_to): one row per source text
// and target language, a second write overwrites the first.
struct TranslationEntry {
	QString input;
	QString output;
	QString lg_from;
	QString lg_to;
	qint64 chat_id = 0;
	qint64 msg_id = 0;
	bool is_out = false;
	qint64 team_id = 0;
	qint64 created_at = 0;
	qint64 sys_id = 0;
};

// Owns the four tables, their in-memory indexes and the JSON file.
//
// Threading: every method must be called from the main thread. save() hops to
// the main thread by itself, so a background caller cannot corrupt the
// document; nothing else is guarded.
class Store final : public base::has_weak_ptr {
public:
	Store();
	~Store();

	Store(const Store &) = delete;
	Store &operator=(const Store &) = delete;

	// Reads the document. A missing or unparsable file is not an error the
	// caller has to handle: the store stays usable and empty, false is
	// returned to let the caller log the difference. Rows that are not
	// objects or that lack their identity key are skipped one by one.
	[[nodiscard]] bool load();

	// Marks the document dirty and coalesces writes: the first call in a
	// burst schedules one write 200ms later, the rest of the burst only mark
	// the document dirty. The file is therefore at most 200ms behind.
	void save();

	bool upsertTodo(const TodoEntry &entry);
	bool removeTodo(qint64 id);
	[[nodiscard]] std::optional<TodoEntry> todo(qint64 id) const;
	[[nodiscard]] const std::vector<TodoEntry> &todos() const;
	bool setTodoState(qint64 id, int state);

	bool upsertTopic(const TodoTopic &topic);
	[[nodiscard]] std::optional<TodoTopic> topic(
		qint64 todoId,
		qint64 peerId) const;
	bool setTopicBlacklist(qint64 todoId, qint64 peerId, bool blacklist);
	bool setTopicTagGroup(qint64 todoId, qint64 peerId, const QString &tag);

	bool addArchive(const TodoArchive &entry);
	[[nodiscard]] const std::vector<TodoArchive> &archives() const;

	[[nodiscard]] std::optional<QString> translation(
		const QString &input,
		const QString &lgTo) const;
	void putTranslation(const TranslationEntry &entry);
	int pruneTranslation(qint64 olderThanSeconds);

private:
	using TopicKey = std::pair<qint64, qint64>;
	using TranslationKey = std::pair<QString, QString>;

	void scheduleSave();
	void writeToDisk();
	void touch();

	void rememberTodo(const TodoEntry &entry);
	void rememberTopic(const TodoTopic &topic);
	void rememberArchive(const TodoArchive &entry);
	void rememberTranslation(const TranslationEntry &entry);

	[[nodiscard]] QString path() const;

	QString _path;

	// True while the document differs from what is on disk. A store that was
	// never modified never writes, so a read-only run leaves no file behind.
	bool _dirty = false;

	base::Timer _saveTimer;

	std::vector<TodoEntry> _todos;
	std::map<qint64, std::size_t> _todoIndex;

	std::vector<TodoTopic> _topics;
	std::map<TopicKey, std::size_t> _topicIndex;

	std::vector<TodoArchive> _archives;
	std::map<qint64, std::size_t> _archiveIndex;

	std::vector<TranslationEntry> _translations;
	std::map<TranslationKey, std::size_t> _translationIndex;
};

} // namespace OG
