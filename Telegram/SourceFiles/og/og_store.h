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
#include <tuple>
#include <utility>
#include <vector>

namespace OG {

// Local data layer of the fork.
//
// The reference client keeps this data in SQLite (tdata/deep.db); our build
// has no SQL module linked, so the same tables are stored as one JSON document
// under <working dir>/og/store.json and indexed in memory. Storage may be
// swapped later without touching the callers.
//
// Every struct below mirrors one table of the reference DDL, column by column,
// in the order the columns are declared, with the column names verbatim and in
// snake_case. A field can therefore be matched between the DDL, the JSON keys
// and this header without any translation step. Types follow the DDL too:
//
//   INTEGER PRIMARY KEY AUTOINCREMENT -> qint64, zero means "assign on insert"
//   TEXT                              -> QString, including the id columns
//   INTEGER (seconds, message id)      -> qint64
//   INTEGER (small enum, count)        -> int
//   integer default 1/2               -> int, the DDL uses plain integers even
//                                        where the value is a flag
//   bool                              -> bool (only `translation.is_out`)
//
// Defaults are the DDL defaults, so a default constructed entry is the same row
// the database would create from a statement that names no columns.

// Todo status values of from_todo_list.status.
inline constexpr auto kTodoStatusPending = 1;
inline constexpr auto kTodoStatusDone = 2;
inline constexpr auto kTodoStatusArchived = 3;

// Todo priority values of from_todo_list.priority.
inline constexpr auto kTodoPriorityNormal = 1;
inline constexpr auto kTodoPriorityImportant = 2;
inline constexpr auto kTodoPriorityUrgent = 3;

// Table "from_todo_list", 28 columns, UNIQUE(from_id, peer_id, receiver_id).
struct TodoEntry {
	qint64 id = 0; // INTEGER PRIMARY KEY AUTOINCREMENT
	QString from_id; // TEXT, the client
	QString from_name; // the client's nickname
	QString from_avatar; // the client's avatar
	QString peer_id; // TEXT, the conversation
	QString peer_name; // the conversation title
	QString peer_avatar; // the conversation avatar
	QString receiver_id = QString::fromLatin1("0");
	int chat_type = 1; // 1 = private, 2 = group
	int msg_type = 1; // 1 = message, 2 = mention
	int unread_count = 0;
	int from_count = 0; // how much this client has sent in total
	int is_new = 1; // 1 = new client, 2 = not new (the DDL says so)
	QString title;
	qint64 msg_id = 0;
	QString msg; // the description
	QString media; // the media file
	int priority = 1; // 1 = normal, 2 = important, 3 = urgent
	int status = 1; // 1 = pending, 2 = done, 3 = archived
	QString reserve1;
	QString reserve2;
	QString reserve3;
	QString reserve4; // the DDL notes this one as fromUsername
	QString reserve5;
	QString reserve6;
	qint64 last_msg_at = 0;
	qint64 created_at = 0;
	qint64 updated_at = 0;
};

// Table "from_todo_topic", 14 columns, UNIQUE(todo_id, chat_peer_id).
struct TodoTopic {
	qint64 todo_id = 0; // INTEGER, the from_todo_list.id it belongs to
	QString chat_peer_id;
	QString topic_id;
	QString dispatcher_id;
	int is_blacklist = 0;
	int tag_group = 0; // which client group; unused in the reference so far
	QString reserve1;
	QString reserve2;
	QString reserve3;
	QString reserve4;
	QString reserve5;
	QString reserve6;
	qint64 created_at = 0;
	qint64 updated_at = 0;
};

// Table "from_todo_archive", 15 columns, UNIQUE(peer_id, msg_id, receiver_id).
struct TodoArchive {
	qint64 archive_id = 0; // INTEGER PRIMARY KEY AUTOINCREMENT
	QString from_id;
	QString peer_id;
	QString receiver_id; // private chats need it: peer_id is not unique alone
	int chat_type = 1;
	int msg_type = 1;
	QString title;
	qint64 msg_id = 0;
	QString msg;
	QString media;
	QString reserve1;
	QString reserve2;
	QString reserve3;
	qint64 created_at = 0;
	qint64 topic_msg_id = 0; // the message id inside the topic group
};

// Table "translation", 10 columns, PRIMARY KEY (input, lg_to).
struct TranslationEntry {
	QString input;
	QString output;
	QString lg_from;
	QString lg_to;
	QString chat_id; // TEXT in the DDL, not an integer
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
	// The unique keys of the DDL, in column order.
	using TodoKey = std::tuple<QString, QString, QString>; // from, peer, recv
	using TopicKey = std::pair<qint64, QString>; // todo, chat_peer
	using ArchiveKey = std::tuple<QString, qint64, QString>; // peer, msg, recv
	using TranslationKey = std::pair<QString, QString>; // input, lg_to

	void scheduleSave();
	void writeToDisk();
	void touch();

	void rememberTodo(const TodoEntry &entry);
	void rememberTopic(const TodoTopic &entry);
	void rememberArchive(const TodoArchive &entry);
	void rememberTranslation(const TranslationEntry &entry);

	void rebuildTodoIndexes();
	void rebuildArchiveIndexes();

	[[nodiscard]] TodoKey todoKey(const TodoEntry &entry) const;
	[[nodiscard]] ArchiveKey archiveKey(const TodoArchive &entry) const;

	[[nodiscard]] QString path() const;

	QString _path;

	// True while the document differs from what is on disk. A store that was
	// never modified never writes, so a read-only run leaves no file behind.
	bool _dirty = false;

	// The id watermarks, our equivalent of sqlite_sequence for AUTOINCREMENT:
	// they only ever grow, so the id of a removed row is never handed out a
	// second time. from_todo_topic.todo_id points at from_todo_list.id, and a
	// reused id would attach old topics to a brand new row.
	qint64 _nextTodoId = 1;
	qint64 _nextArchiveId = 1;

	base::Timer _saveTimer;

	std::vector<TodoEntry> _todos;
	std::map<qint64, std::size_t> _todoById;
	std::map<TodoKey, std::size_t> _todoByKey;

	std::vector<TodoTopic> _topics;
	std::map<TopicKey, std::size_t> _topicByKey;

	std::vector<TodoArchive> _archives;
	std::map<qint64, std::size_t> _archiveById;
	std::map<ArchiveKey, std::size_t> _archiveByKey;

	std::vector<TranslationEntry> _translations;
	std::map<TranslationKey, std::size_t> _translationByKey;
};

} // namespace OG
