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

// The same three values as a type, so a list query cannot be handed a count
// by mistake. 1 = pending, 2 = done, 3 = archived, as the DDL says.
enum class TodoStatus : int {
	Pending = kTodoStatusPending,
	Done = kTodoStatusDone,
	Archived = kTodoStatusArchived,
};

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
	// 1 or 0, not 1/2: the reference sets 1 while the accumulated unread
	// count still equals the accumulated total, and 0 once something was read.
	int is_new = 1;
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

// Table "portrait", 44 columns, UNIQUE(platform, key, worker_id).
struct Portrait {
	qint64 inner_id = 0; // INTEGER PRIMARY KEY AUTOINCREMENT
	int platform = 0;
	QString key;
	QString worker_id; // TEXT, the seat the profile belongs to
	int stage = 0;
	int source = 0;
	qint64 id = 0; // integer here, unlike client.id which is TEXT
	int type = 0;
	QString username;
	QString nickname;
	QString phone;
	QString name;
	QString thumbnail;
	int gender = 0;
	int birthday = 0; // an integer date, e.g. 19900131
	QString address;
	QString email;
	QString position;
	QString salary;
	QString company;
	QString company_size;
	QString company_industry;
	QString mark;
	QString tags; // one TEXT column, not a JSON array
	QString country;
	QString lang;
	QString user_ids;
	QString chat_ids;
	int total_msgs = 0;
	int total_chars = 0;
	qint64 team_id = 0;
	qint64 first_msg_at = 0;
	qint64 last_msg_at = 0;
	qint64 last_msg_id = 0;
	qint64 created_at = 0;
	qint64 sys_id = 0;
	QString reserve1;
	QString reserve2;
	QString reserve3;
	QString reserve4;
	QString reserve5;
	QString reserve6;
	QString reserve7;
	QString reserve8;
};

// Table "client", 25 columns, UNIQUE(platform, username, phone).
// The DDL skips reserve5 here on purpose: there is no such column, and the
// gap is not a transcription mistake to be "fixed".
struct Client {
	qint64 inner_id = 0; // INTEGER PRIMARY KEY AUTOINCREMENT
	int platform = 0;
	QString key;
	QString worker_id;
	QString username;
	QString phone;
	QString id; // TEXT here, unlike portrait.id which is integer
	QString nickname;
	int state = 1;
	qint64 import_user_id = 0;
	qint64 imported_at = 0;
	qint64 followed_at = 0;
	qint64 created_at = 0;
	int add_count = 0;
	qint64 team_id = 0;
	int sent_count = 0;
	int recv_count = 0;
	qint64 sys_id = 0;
	QString reserve1;
	QString reserve2;
	QString reserve3;
	QString reserve4;
	QString reserve6;
	QString reserve7;
	QString reserve8;
};

// Table "portrait_log", 11 columns, PRIMARY KEY (log_id), no other key.
struct PortraitLog {
	qint64 log_id = 0; // INTEGER PRIMARY KEY AUTOINCREMENT
	int platform = 0;
	QString key;
	QString key2;
	qint64 inner_id = 0; // the portrait.inner_id the change belongs to
	QString change; // a text snapshot, not a structured value
	qint64 user_id = 0;
	qint64 team_id = 0;
	QString worker_id;
	qint64 sys_id = 0;
	qint64 created_at = 0;
};

// Table "tag", 8 columns, PRIMARY KEY (label) - not a generated id.
struct Tag {
	QString label;
	int type = 0;
	int num = 0;
	QString category;
	QString worker_id;
	qint64 user_id = 0;
	qint64 team_id = 0;
	qint64 created_at = 0;
};

// Table "tag_category", 7 columns, PRIMARY KEY (label).
// One column less than "tag": there is no category column here.
struct TagCategory {
	QString label;
	int type = 0;
	int num = 0;
	QString worker_id;
	qint64 user_id = 0;
	qint64 team_id = 0;
	qint64 created_at = 0;
};

// Owns the tables, their in-memory indexes and the JSON file.
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

	// Adds one incoming event for a todo row: unread_count and from_count
	// accumulate, is_new follows the reference CASE, the rest of the row is
	// replaced by the incoming one. Inserts the row when its key is new.
	void upsertTodoEvent(const TodoEntry &entry);

	// The independent "was read" update of the reference: overwrites the
	// unread counter, recomputes is_new and refreshes updated_at.
	bool setTodoRead(
		const QString &fromId,
		const QString &peerId,
		const QString &receiverId,
		int unreadCount);

	// The list of one status, ordered like the reference query orders it:
	// last_msg_at first, updated_at second, newest first both times.
	[[nodiscard]] std::vector<TodoEntry> todosByStatus(TodoStatus status) const;

	bool upsertTopic(const TodoTopic &topic);
	[[nodiscard]] std::optional<TodoTopic> topic(
		qint64 todoId,
		qint64 peerId) const;
	bool setTopicBlacklist(qint64 todoId, qint64 peerId, bool blacklist);
	bool setTopicTagGroup(qint64 todoId, qint64 peerId, const QString &tag);

	bool addArchive(const TodoArchive &entry);
	[[nodiscard]] const std::vector<TodoArchive> &archives() const;

	// topic_msg_id is filled in after the insert by the reference, not at
	// insert time.
	bool setArchiveTopicMsgId(qint64 archiveId, qint64 topicMsgId);
	[[nodiscard]] const TodoArchive *findArchiveByTopicMsgId(
		qint64 topicMsgId) const;

	// The four-part "is this message archived" lookup, 0 when it is not.
	[[nodiscard]] qint64 archiveTopicMsgId(
		const QString &fromId,
		const QString &peerId,
		const QString &receiverId,
		qint64 msgId) const;

	[[nodiscard]] std::optional<QString> translation(
		const QString &input,
		const QString &lgTo) const;
	void putTranslation(const TranslationEntry &entry);
	int pruneTranslation(qint64 olderThanSeconds);

	[[nodiscard]] const std::vector<Portrait> &portraits() const;
	[[nodiscard]] std::optional<Portrait> portrait(qint64 innerId) const;
	void rememberPortrait(const Portrait &entry);
	bool removePortrait(qint64 innerId);

	[[nodiscard]] const std::vector<Client> &clients() const;
	[[nodiscard]] std::optional<Client> client(qint64 innerId) const;
	void rememberClient(const Client &entry);
	bool removeClient(qint64 innerId);

	[[nodiscard]] const std::vector<PortraitLog> &portraitLogs() const;
	[[nodiscard]] std::optional<PortraitLog> portraitLog(qint64 logId) const;
	qint64 addPortraitLog(const PortraitLog &entry);
	bool removePortraitLog(qint64 logId);

	[[nodiscard]] const std::vector<Tag> &tags() const;
	[[nodiscard]] const Tag *tag(const QString &label) const;
	void rememberTag(const Tag &entry);
	bool removeTag(const QString &label);

	[[nodiscard]] const std::vector<TagCategory> &tagCategories() const;
	[[nodiscard]] const TagCategory *tagCategory(const QString &label) const;
	void rememberTagCategory(const TagCategory &entry);
	bool removeTagCategory(const QString &label);

private:
	// The unique keys of the DDL, in column order.
	using TodoKey = std::tuple<QString, QString, QString>; // from, peer, recv
	using TopicKey = std::pair<qint64, QString>; // todo, chat_peer
	using ArchiveKey = std::tuple<QString, qint64, QString>; // peer, msg, recv
	using TranslationKey = std::pair<QString, QString>; // input, lg_to
	using PortraitKey = std::tuple<int, QString, QString>; // platform, key, seat
	using ClientKey = std::tuple<int, QString, QString>; // platform, user, phone

	void scheduleSave();
	void writeToDisk();
	void touch();

	void rememberTodo(const TodoEntry &entry);
	void rememberTodoEvent(const TodoEntry &entry);
	void rememberTopic(const TodoTopic &entry);
	void rememberArchive(const TodoArchive &entry);
	void rememberTranslation(const TranslationEntry &entry);
	void rememberPortraitRow(const Portrait &entry);
	void rememberClientRow(const Client &entry);

	void rebuildTodoIndexes();
	void rebuildArchiveIndexes();
	void rebuildPortraitIndexes();
	void rebuildClientIndexes();

	[[nodiscard]] TodoKey todoKey(const TodoEntry &entry) const;
	[[nodiscard]] ArchiveKey archiveKey(const TodoArchive &entry) const;
	[[nodiscard]] PortraitKey portraitKey(const Portrait &entry) const;
	[[nodiscard]] ClientKey clientKey(const Client &entry) const;

	[[nodiscard]] QString path() const;

	QString _path;

	// True while the document differs from what is on disk. A store that was
	// never modified never writes, so a read-only run leaves no file behind.
	bool _dirty = false;

	// True while load() fills the tables through the same mutators that the
	// callers use; touch() stays quiet so a read does not schedule a write.
	bool _loading = false;

	// The id watermarks, our equivalent of sqlite_sequence for AUTOINCREMENT:
	// they only ever grow, so the id of a removed row is never handed out a
	// second time. from_todo_topic.todo_id points at from_todo_list.id, and a
	// reused id would attach old topics to a brand new row.
	qint64 _nextTodoId = 1;
	qint64 _nextArchiveId = 1;
	qint64 _nextPortraitId = 1;
	qint64 _nextClientId = 1;
	qint64 _nextPortraitLogId = 1;

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

	std::vector<Portrait> _portraits;
	std::map<qint64, std::size_t> _portraitByInnerId;
	std::map<PortraitKey, std::size_t> _portraitByKey;

	std::vector<Client> _clients;
	std::map<qint64, std::size_t> _clientByInnerId;
	std::map<ClientKey, std::size_t> _clientByKey;

	std::vector<PortraitLog> _portraitLogs;
	std::map<qint64, std::size_t> _portraitLogById;

	std::vector<Tag> _tags;
	std::map<QString, std::size_t> _tagByLabel;

	std::vector<TagCategory> _tagCategories;
	std::map<QString, std::size_t> _tagCategoryByLabel;
};

} // namespace OG
