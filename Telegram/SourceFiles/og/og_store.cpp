/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_store.h"

#include "settings.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonParseError>
#include <QtCore/QSaveFile>

#include <algorithm>
#include <cstddef>

namespace OG {
namespace {

// Coalescing window for save(). The first call of a burst schedules the
// write, the rest only mark the document dirty, so the file is at most this
// many milliseconds behind the memory.
constexpr auto kSaveCoalesceDelay = crl::time(200);

const auto kTodosKey = "todos";
const auto kTopicsKey = "topics";
const auto kArchivesKey = "archives";
const auto kTranslationsKey = "translations";
const auto kNextTodoIdKey = "next_todo_id";
const auto kNextArchiveIdKey = "next_archive_id";

[[nodiscard]] QString Key(const char *name) {
	return QString::fromLatin1(name);
}

// Readers are deliberately forgiving: a value that arrives as a number or as
// a bool where the DDL says TEXT is still accepted, and a value that is
// missing or of an unexpected type falls back to the default instead of
// dropping the whole row.
[[nodiscard]] qint64 ReadInt64(const QJsonObject &object, const char *name) {
	const auto value = object.value(Key(name));
	if (value.isDouble()) {
		return static_cast<qint64>(value.toDouble());
	} else if (value.isString()) {
		auto ok = false;
		const auto result = value.toString().toLongLong(&ok);
		return ok ? result : qint64(0);
	} else if (value.isBool()) {
		return value.toBool() ? qint64(1) : qint64(0);
	}
	return 0;
}

[[nodiscard]] int ReadInt(const QJsonObject &object, const char *name) {
	return static_cast<int>(ReadInt64(object, name));
}

[[nodiscard]] bool ReadBool(const QJsonObject &object, const char *name) {
	const auto value = object.value(Key(name));
	if (value.isBool()) {
		return value.toBool();
	} else if (value.isDouble()) {
		return value.toDouble() != 0.;
	} else if (value.isString()) {
		const auto text = value.toString();
		return (text == QString::fromLatin1("1"))
			|| (text.compare(
				QString::fromLatin1("true"),
				Qt::CaseInsensitive) == 0);
	}
	return false;
}

[[nodiscard]] QString ReadString(const QJsonObject &object, const char *name) {
	const auto value = object.value(Key(name));
	if (value.isString()) {
		return value.toString();
	} else if (value.isDouble()) {
		// The id columns are TEXT in the DDL and hold numbers in practice, so
		// a number read back as text is the same value the reference stores.
		const auto number = value.toDouble();
		const auto asInteger = static_cast<qint64>(number);
		if (static_cast<double>(asInteger) == number) {
			return QString::number(asInteger);
		}
		return QString::number(number);
	} else if (value.isBool()) {
		return value.toBool()
			? QString::fromLatin1("true")
			: QString::fromLatin1("false");
	}
	return QString();
}

void Write(QJsonObject &object, const char *name, qint64 value) {
	object.insert(Key(name), value);
}

void Write(QJsonObject &object, const char *name, int value) {
	object.insert(Key(name), value);
}

void Write(QJsonObject &object, const char *name, bool value) {
	object.insert(Key(name), value);
}

void Write(QJsonObject &object, const char *name, const QString &value) {
	object.insert(Key(name), value);
}

[[nodiscard]] QJsonObject Write(const TodoEntry &entry) {
	auto result = QJsonObject();
	Write(result, "id", entry.id);
	Write(result, "from_id", entry.from_id);
	Write(result, "from_name", entry.from_name);
	Write(result, "from_avatar", entry.from_avatar);
	Write(result, "peer_id", entry.peer_id);
	Write(result, "peer_name", entry.peer_name);
	Write(result, "peer_avatar", entry.peer_avatar);
	Write(result, "receiver_id", entry.receiver_id);
	Write(result, "chat_type", entry.chat_type);
	Write(result, "msg_type", entry.msg_type);
	Write(result, "unread_count", entry.unread_count);
	Write(result, "from_count", entry.from_count);
	Write(result, "is_new", entry.is_new);
	Write(result, "title", entry.title);
	Write(result, "msg_id", entry.msg_id);
	Write(result, "msg", entry.msg);
	Write(result, "media", entry.media);
	Write(result, "priority", entry.priority);
	Write(result, "status", entry.status);
	Write(result, "reserve1", entry.reserve1);
	Write(result, "reserve2", entry.reserve2);
	Write(result, "reserve3", entry.reserve3);
	Write(result, "reserve4", entry.reserve4);
	Write(result, "reserve5", entry.reserve5);
	Write(result, "reserve6", entry.reserve6);
	Write(result, "last_msg_at", entry.last_msg_at);
	Write(result, "created_at", entry.created_at);
	Write(result, "updated_at", entry.updated_at);
	return result;
}

[[nodiscard]] TodoEntry TodoFromJson(const QJsonObject &object) {
	auto result = TodoEntry();
	result.id = ReadInt64(object, "id");
	result.from_id = ReadString(object, "from_id");
	result.from_name = ReadString(object, "from_name");
	result.from_avatar = ReadString(object, "from_avatar");
	result.peer_id = ReadString(object, "peer_id");
	result.peer_name = ReadString(object, "peer_name");
	result.peer_avatar = ReadString(object, "peer_avatar");
	result.receiver_id = ReadString(object, "receiver_id");
	result.chat_type = ReadInt(object, "chat_type");
	result.msg_type = ReadInt(object, "msg_type");
	result.unread_count = ReadInt(object, "unread_count");
	result.from_count = ReadInt(object, "from_count");
	result.is_new = ReadInt(object, "is_new");
	result.title = ReadString(object, "title");
	result.msg_id = ReadInt64(object, "msg_id");
	result.msg = ReadString(object, "msg");
	result.media = ReadString(object, "media");
	result.priority = ReadInt(object, "priority");
	result.status = ReadInt(object, "status");
	result.reserve1 = ReadString(object, "reserve1");
	result.reserve2 = ReadString(object, "reserve2");
	result.reserve3 = ReadString(object, "reserve3");
	result.reserve4 = ReadString(object, "reserve4");
	result.reserve5 = ReadString(object, "reserve5");
	result.reserve6 = ReadString(object, "reserve6");
	result.last_msg_at = ReadInt64(object, "last_msg_at");
	result.created_at = ReadInt64(object, "created_at");
	result.updated_at = ReadInt64(object, "updated_at");
	return result;
}

[[nodiscard]] QJsonObject Write(const TodoTopic &entry) {
	auto result = QJsonObject();
	Write(result, "todo_id", entry.todo_id);
	Write(result, "chat_peer_id", entry.chat_peer_id);
	Write(result, "topic_id", entry.topic_id);
	Write(result, "dispatcher_id", entry.dispatcher_id);
	Write(result, "is_blacklist", entry.is_blacklist);
	Write(result, "tag_group", entry.tag_group);
	Write(result, "reserve1", entry.reserve1);
	Write(result, "reserve2", entry.reserve2);
	Write(result, "reserve3", entry.reserve3);
	Write(result, "reserve4", entry.reserve4);
	Write(result, "reserve5", entry.reserve5);
	Write(result, "reserve6", entry.reserve6);
	Write(result, "created_at", entry.created_at);
	Write(result, "updated_at", entry.updated_at);
	return result;
}

[[nodiscard]] TodoTopic TopicFromJson(const QJsonObject &object) {
	auto result = TodoTopic();
	result.todo_id = ReadInt64(object, "todo_id");
	result.chat_peer_id = ReadString(object, "chat_peer_id");
	result.topic_id = ReadString(object, "topic_id");
	result.dispatcher_id = ReadString(object, "dispatcher_id");
	result.is_blacklist = ReadInt(object, "is_blacklist");
	result.tag_group = ReadInt(object, "tag_group");
	result.reserve1 = ReadString(object, "reserve1");
	result.reserve2 = ReadString(object, "reserve2");
	result.reserve3 = ReadString(object, "reserve3");
	result.reserve4 = ReadString(object, "reserve4");
	result.reserve5 = ReadString(object, "reserve5");
	result.reserve6 = ReadString(object, "reserve6");
	result.created_at = ReadInt64(object, "created_at");
	result.updated_at = ReadInt64(object, "updated_at");
	return result;
}

[[nodiscard]] QJsonObject Write(const TodoArchive &entry) {
	auto result = QJsonObject();
	Write(result, "archive_id", entry.archive_id);
	Write(result, "from_id", entry.from_id);
	Write(result, "peer_id", entry.peer_id);
	Write(result, "receiver_id", entry.receiver_id);
	Write(result, "chat_type", entry.chat_type);
	Write(result, "msg_type", entry.msg_type);
	Write(result, "title", entry.title);
	Write(result, "msg_id", entry.msg_id);
	Write(result, "msg", entry.msg);
	Write(result, "media", entry.media);
	Write(result, "reserve1", entry.reserve1);
	Write(result, "reserve2", entry.reserve2);
	Write(result, "reserve3", entry.reserve3);
	Write(result, "created_at", entry.created_at);
	Write(result, "topic_msg_id", entry.topic_msg_id);
	return result;
}

[[nodiscard]] TodoArchive ArchiveFromJson(const QJsonObject &object) {
	auto result = TodoArchive();
	result.archive_id = ReadInt64(object, "archive_id");
	result.from_id = ReadString(object, "from_id");
	result.peer_id = ReadString(object, "peer_id");
	result.receiver_id = ReadString(object, "receiver_id");
	result.chat_type = ReadInt(object, "chat_type");
	result.msg_type = ReadInt(object, "msg_type");
	result.title = ReadString(object, "title");
	result.msg_id = ReadInt64(object, "msg_id");
	result.msg = ReadString(object, "msg");
	result.media = ReadString(object, "media");
	result.reserve1 = ReadString(object, "reserve1");
	result.reserve2 = ReadString(object, "reserve2");
	result.reserve3 = ReadString(object, "reserve3");
	result.created_at = ReadInt64(object, "created_at");
	result.topic_msg_id = ReadInt64(object, "topic_msg_id");
	return result;
}

[[nodiscard]] QJsonObject Write(const TranslationEntry &entry) {
	auto result = QJsonObject();
	Write(result, "input", entry.input);
	Write(result, "output", entry.output);
	Write(result, "lg_from", entry.lg_from);
	Write(result, "lg_to", entry.lg_to);
	Write(result, "chat_id", entry.chat_id);
	Write(result, "msg_id", entry.msg_id);
	Write(result, "is_out", entry.is_out);
	Write(result, "team_id", entry.team_id);
	Write(result, "created_at", entry.created_at);
	Write(result, "sys_id", entry.sys_id);
	return result;
}

[[nodiscard]] TranslationEntry TranslationFromJson(const QJsonObject &object) {
	auto result = TranslationEntry();
	result.input = ReadString(object, "input");
	result.output = ReadString(object, "output");
	result.lg_from = ReadString(object, "lg_from");
	result.lg_to = ReadString(object, "lg_to");
	result.chat_id = ReadString(object, "chat_id");
	result.msg_id = ReadInt64(object, "msg_id");
	result.is_out = ReadBool(object, "is_out");
	result.team_id = ReadInt64(object, "team_id");
	result.created_at = ReadInt64(object, "created_at");
	result.sys_id = ReadInt64(object, "sys_id");
	return result;
}

} // namespace

Store::Store() {
	_path = path();
	_saveTimer.setCallback([this] {
		writeToDisk();
	});
}

Store::~Store() {
	_saveTimer.cancel();
	if (_dirty) {
		// The coalescing window is at most 200ms, but a quit must not lose
		// the tail of a burst: write once, synchronously, and stop.
		writeToDisk();
	}
}

QString Store::path() const {
	return cWorkingDir() + QString::fromLatin1("og/store.json");
}

Store::TodoKey Store::todoKey(const TodoEntry &entry) const {
	return TodoKey{ entry.from_id, entry.peer_id, entry.receiver_id };
}

Store::ArchiveKey Store::archiveKey(const TodoArchive &entry) const {
	return ArchiveKey{ entry.peer_id, entry.msg_id, entry.receiver_id };
}

void Store::rebuildTodoIndexes() {
	_todoById.clear();
	_todoByKey.clear();
	for (auto i = std::size_t(0); i != _todos.size(); ++i) {
		_todoById.emplace(_todos[i].id, i);
		_todoByKey.emplace(todoKey(_todos[i]), i);
	}
}

void Store::rebuildArchiveIndexes() {
	_archiveById.clear();
	_archiveByKey.clear();
	for (auto i = std::size_t(0); i != _archives.size(); ++i) {
		_archiveById.emplace(_archives[i].archive_id, i);
		_archiveByKey.emplace(archiveKey(_archives[i]), i);
	}
}

bool Store::load() {
	_todos.clear();
	_todoById.clear();
	_todoByKey.clear();
	_topics.clear();
	_topicByKey.clear();
	_archives.clear();
	_archiveById.clear();
	_archiveByKey.clear();
	_translations.clear();
	_translationByKey.clear();
	_dirty = false;
	_nextTodoId = 1;
	_nextArchiveId = 1;

	auto file = QFile(_path);
	if (!file.exists()) {
		return false;
	}
	if (!file.open(QIODevice::ReadOnly)) {
		return false;
	}
	const auto data = file.readAll();
	file.close();

	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(data, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		// A document we cannot parse as a whole is the one case the rows
		// cannot be recovered from: report it and stay empty.
		return false;
	}
	const auto root = document.object();

	// A document written before the watermarks existed has none: the loops
	// below raise them to max(id) + 1 while the rows are read back, which is
	// the same value the old max+1 allocation would have used.
	_nextTodoId = std::max(ReadInt64(root, kNextTodoIdKey), qint64(1));
	_nextArchiveId = std::max(ReadInt64(root, kNextArchiveIdKey), qint64(1));

	const auto todos = root.value(Key(kTodosKey)).toArray();
	for (const auto &value : todos) {
		const auto entry = TodoFromJson(value.toObject());
		if (entry.from_id.isEmpty() && entry.peer_id.isEmpty()) {
			continue; // A row with no side of the unique key is not a todo.
		}
		rememberTodo(entry);
	}

	const auto topics = root.value(Key(kTopicsKey)).toArray();
	for (const auto &value : topics) {
		const auto entry = TopicFromJson(value.toObject());
		if (!entry.todo_id) {
			continue;
		}
		rememberTopic(entry);
	}

	const auto archives = root.value(Key(kArchivesKey)).toArray();
	for (const auto &value : archives) {
		const auto entry = ArchiveFromJson(value.toObject());
		if (entry.peer_id.isEmpty() && !entry.msg_id) {
			continue;
		}
		rememberArchive(entry);
	}

	const auto translations = root.value(Key(kTranslationsKey)).toArray();
	for (const auto &value : translations) {
		const auto entry = TranslationFromJson(value.toObject());
		if (entry.input.isEmpty() || entry.lg_to.isEmpty()) {
			continue; // Both halves of the primary key are required.
		}
		rememberTranslation(entry);
	}
	return true;
}

void Store::save() {
	// save() is the one entry point that tolerates a foreign thread: it hops
	// to the main thread before it touches the timer or the document. The
	// weak pointer of base::has_weak_ptr drops the callback if the store is
	// gone by then.
	crl::on_main(this, [this] {
		scheduleSave();
	});
}

void Store::scheduleSave() {
	if (!_dirty) {
		return;
	}
	if (_saveTimer.isActive()) {
		// Already waiting inside the coalescing window: the pending write
		// will pick this change up as well.
		return;
	}
	_saveTimer.callOnce(kSaveCoalesceDelay);
}

void Store::touch() {
	if (!_dirty) {
		_dirty = true;
	}
	save();
}

void Store::writeToDisk() {
	_saveTimer.cancel();

	const auto directory = QFileInfo(_path).absolutePath();
	if (!QDir().mkpath(directory)) {
		return;
	}

	auto root = QJsonObject();
	auto todos = QJsonArray();
	for (const auto &entry : _todos) {
		todos.append(Write(entry));
	}
	root.insert(Key(kTodosKey), todos);

	auto topics = QJsonArray();
	for (const auto &entry : _topics) {
		topics.append(Write(entry));
	}
	root.insert(Key(kTopicsKey), topics);

	auto archives = QJsonArray();
	for (const auto &entry : _archives) {
		archives.append(Write(entry));
	}
	root.insert(Key(kArchivesKey), archives);

	auto translations = QJsonArray();
	for (const auto &entry : _translations) {
		translations.append(Write(entry));
	}
	root.insert(Key(kTranslationsKey), translations);

	// The watermarks are part of the document, not of the tables: they keep
	// an id that a removed row used to occupy from being allocated again.
	Write(root, kNextTodoIdKey, _nextTodoId);
	Write(root, kNextArchiveIdKey, _nextArchiveId);

	const auto data = QJsonDocument(root).toJson(QJsonDocument::Indented);

	// QSaveFile writes a temporary file next to the target and replaces the
	// target on commit(), so a crash leaves either the old document or the new
	// one, never a half written file. It is also the only way to do this
	// without a per platform fsync branch.
	auto file = QSaveFile(_path);
	if (!file.open(QIODevice::WriteOnly)) {
		return;
	}
	if (file.write(data) != data.size()) {
		file.cancelWriting();
		return;
	}
	if (!file.commit()) {
		return;
	}
	_dirty = false;
}

void Store::rememberTodo(const TodoEntry &entry) {
	auto row = entry;
	const auto now = QDateTime::currentSecsSinceEpoch();

	if (const auto i = _todoByKey.find(todoKey(row)); i != _todoByKey.end()) {
		// UNIQUE(from_id, peer_id, receiver_id): the row is updated in place,
		// the primary key it already had is kept when the caller sends none.
		const auto index = i->second;
		const auto existing = _todos[index];
		if (!row.id) {
			row.id = existing.id;
		}
		if (!row.created_at) {
			row.created_at = existing.created_at;
		}
		if (!row.updated_at) {
			row.updated_at = now;
		}
		_todos[index] = row;
		rebuildTodoIndexes();
		return;
	}

	if (!row.id) {
		row.id = _nextTodoId++;
	} else {
		// The watermark only grows, so the id of a removed row is never handed
		// out again: from_todo_topic.todo_id points at it.
		_nextTodoId = std::max(_nextTodoId, row.id + 1);
		if (const auto j = _todoById.find(row.id); j != _todoById.end()) {
			// The primary key is taken by another unique key: INSERT OR
			// REPLACE semantics, the old row is dropped.
			_todos.erase(_todos.begin() + std::ptrdiff_t(j->second));
		}
	}
	if (!row.created_at) {
		row.created_at = now;
	}
	if (!row.updated_at) {
		row.updated_at = now;
	}
	_todos.push_back(row);
	rebuildTodoIndexes();
}

void Store::rememberTopic(const TodoTopic &entry) {
	auto row = entry;
	const auto now = QDateTime::currentSecsSinceEpoch();
	const auto key = TopicKey{ row.todo_id, row.chat_peer_id };
	if (const auto i = _topicByKey.find(key); i != _topicByKey.end()) {
		const auto index = i->second;
		const auto existing = _topics[index];
		if (!row.created_at) {
			row.created_at = existing.created_at;
		}
		if (!row.updated_at) {
			row.updated_at = now;
		}
		_topics[index] = row;
		return;
	}
	if (!row.created_at) {
		row.created_at = now;
	}
	if (!row.updated_at) {
		row.updated_at = now;
	}
	_topicByKey.emplace(key, _topics.size());
	_topics.push_back(row);
}

void Store::rememberArchive(const TodoArchive &entry) {
	auto row = entry;
	const auto now = QDateTime::currentSecsSinceEpoch();

	if (const auto i = _archiveByKey.find(archiveKey(row));
		i != _archiveByKey.end()) {
		// UNIQUE(peer_id, msg_id, receiver_id): updated in place.
		const auto index = i->second;
		const auto existing = _archives[index];
		if (!row.archive_id) {
			row.archive_id = existing.archive_id;
		}
		if (!row.created_at) {
			row.created_at = existing.created_at;
		}
		_archives[index] = row;
		rebuildArchiveIndexes();
		return;
	}

	if (!row.archive_id) {
		row.archive_id = _nextArchiveId++;
	} else {
		_nextArchiveId = std::max(_nextArchiveId, row.archive_id + 1);
		if (const auto j = _archiveById.find(row.archive_id);
			j != _archiveById.end()) {
			_archives.erase(_archives.begin() + std::ptrdiff_t(j->second));
		}
	}
	if (!row.created_at) {
		row.created_at = now;
	}
	_archives.push_back(row);
	rebuildArchiveIndexes();
}

void Store::rememberTranslation(const TranslationEntry &entry) {
	auto row = entry;
	if (!row.created_at) {
		row.created_at = QDateTime::currentSecsSinceEpoch();
	}
	const auto key = TranslationKey{ row.input, row.lg_to };
	if (const auto i = _translationByKey.find(key);
		i != _translationByKey.end()) {
		_translations[i->second] = row;
		return;
	}
	_translationByKey.emplace(key, _translations.size());
	_translations.push_back(row);
}

bool Store::upsertTodo(const TodoEntry &entry) {
	rememberTodo(entry);
	touch();
	return true;
}

bool Store::removeTodo(qint64 id) {
	const auto i = _todoById.find(id);
	if (i == _todoById.end()) {
		return false;
	}
	_todos.erase(_todos.begin() + std::ptrdiff_t(i->second));
	rebuildTodoIndexes();
	touch();
	return true;
}

std::optional<TodoEntry> Store::todo(qint64 id) const {
	const auto i = _todoById.find(id);
	if (i == _todoById.end()) {
		return std::nullopt;
	}
	return _todos[i->second];
}

const std::vector<TodoEntry> &Store::todos() const {
	return _todos;
}

bool Store::setTodoState(qint64 id, int state) {
	if (state != kTodoStatusPending
		&& state != kTodoStatusDone
		&& state != kTodoStatusArchived) {
		return false;
	}
	const auto i = _todoById.find(id);
	if (i == _todoById.end()) {
		return false;
	}
	auto &row = _todos[i->second];
	row.status = state;
	row.updated_at = QDateTime::currentSecsSinceEpoch();
	touch();
	return true;
}

bool Store::upsertTopic(const TodoTopic &entry) {
	if (!entry.todo_id) {
		return false;
	}
	rememberTopic(entry);
	touch();
	return true;
}

std::optional<TodoTopic> Store::topic(qint64 todoId, qint64 peerId) const {
	// The column is TEXT and holds a peer id, so a caller that passes a
	// number has to match the text form the row was written in.
	const auto i = _topicByKey.find(
		TopicKey{ todoId, QString::number(peerId) });
	if (i == _topicByKey.end()) {
		return std::nullopt;
	}
	return _topics[i->second];
}

bool Store::setTopicBlacklist(qint64 todoId, qint64 peerId, bool blacklist) {
	const auto i = _topicByKey.find(
		TopicKey{ todoId, QString::number(peerId) });
	if (i == _topicByKey.end()) {
		return false;
	}
	auto &row = _topics[i->second];
	row.is_blacklist = blacklist ? 1 : 0;
	row.updated_at = QDateTime::currentSecsSinceEpoch();
	touch();
	return true;
}

bool Store::setTopicTagGroup(
		qint64 todoId,
		qint64 peerId,
		const QString &tag) {
	const auto i = _topicByKey.find(
		TopicKey{ todoId, QString::number(peerId) });
	if (i == _topicByKey.end()) {
		return false;
	}
	auto ok = false;
	const auto group = tag.toInt(&ok);
	if (!ok) {
		// tag_group is an INTEGER column; a caller that hands us a label
		// cannot be stored in it, and silently writing zero would lose the
		// difference between "no group" and "unusable value".
		return false;
	}
	auto &row = _topics[i->second];
	row.tag_group = group;
	row.updated_at = QDateTime::currentSecsSinceEpoch();
	touch();
	return true;
}

bool Store::addArchive(const TodoArchive &entry) {
	rememberArchive(entry);
	touch();
	return true;
}

const std::vector<TodoArchive> &Store::archives() const {
	return _archives;
}

std::optional<QString> Store::translation(
		const QString &input,
		const QString &lgTo) const {
	const auto i = _translationByKey.find(TranslationKey{ input, lgTo });
	if (i == _translationByKey.end()) {
		return std::nullopt;
	}
	return _translations[i->second].output;
}

void Store::putTranslation(const TranslationEntry &entry) {
	if (entry.input.isEmpty() || entry.lg_to.isEmpty()) {
		return;
	}
	rememberTranslation(entry);
	touch();
}

int Store::pruneTranslation(qint64 olderThanSeconds) {
	if (olderThanSeconds <= 0) {
		return 0;
	}
	const auto cutoff = QDateTime::currentSecsSinceEpoch() - olderThanSeconds;

	auto kept = std::vector<TranslationEntry>();
	kept.reserve(_translations.size());
	auto removed = 0;
	for (const auto &entry : _translations) {
		// A row without a timestamp has no age to compare with. Keeping it is
		// the cheap side of the guess: dropping it could throw away a fresh
		// cache row that a caller simply did not stamp.
		const auto stale = (entry.created_at > 0)
			&& (entry.created_at < cutoff);
		if (stale) {
			++removed;
		} else {
			kept.push_back(entry);
		}
	}
	if (!removed) {
		return 0;
	}
	_translations = std::move(kept);
	_translationByKey.clear();
	for (auto i = std::size_t(0); i != _translations.size(); ++i) {
		_translationByKey.emplace(
			TranslationKey{ _translations[i].input, _translations[i].lg_to },
			i);
	}
	touch();
	return removed;
}

} // namespace OG
