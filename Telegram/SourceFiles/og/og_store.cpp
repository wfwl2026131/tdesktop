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
const auto kPortraitsKey = "portraits";
const auto kClientsKey = "clients";
const auto kPortraitLogsKey = "portrait_logs";
const auto kTagsKey = "tags";
const auto kTagCategoriesKey = "tag_categories";
const auto kNextPortraitIdKey = "next_portrait_id";
const auto kNextClientIdKey = "next_client_id";
const auto kNextPortraitLogIdKey = "next_portrait_log_id";

// load() reuses the same row mutators the callers use, and those mark the
// document dirty. A read must not schedule a write, so the flag below tells
// touch() to stay quiet for the whole load.
class LoadingGuard {
public:
	explicit LoadingGuard(bool &loading) : _loading(loading) {
		_loading = true;
	}
	~LoadingGuard() {
		_loading = false;
	}

private:
	bool &_loading;

};

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

[[nodiscard]] QJsonObject Write(const Portrait &entry) {
	auto result = QJsonObject();
	Write(result, "inner_id", entry.inner_id);
	Write(result, "platform", entry.platform);
	Write(result, "key", entry.key);
	Write(result, "worker_id", entry.worker_id);
	Write(result, "stage", entry.stage);
	Write(result, "source", entry.source);
	Write(result, "id", entry.id);
	Write(result, "type", entry.type);
	Write(result, "username", entry.username);
	Write(result, "nickname", entry.nickname);
	Write(result, "phone", entry.phone);
	Write(result, "name", entry.name);
	Write(result, "thumbnail", entry.thumbnail);
	Write(result, "gender", entry.gender);
	Write(result, "birthday", entry.birthday);
	Write(result, "address", entry.address);
	Write(result, "email", entry.email);
	Write(result, "position", entry.position);
	Write(result, "salary", entry.salary);
	Write(result, "company", entry.company);
	Write(result, "company_size", entry.company_size);
	Write(result, "company_industry", entry.company_industry);
	Write(result, "mark", entry.mark);
	Write(result, "tags", entry.tags);
	Write(result, "country", entry.country);
	Write(result, "lang", entry.lang);
	Write(result, "user_ids", entry.user_ids);
	Write(result, "chat_ids", entry.chat_ids);
	Write(result, "total_msgs", entry.total_msgs);
	Write(result, "total_chars", entry.total_chars);
	Write(result, "team_id", entry.team_id);
	Write(result, "first_msg_at", entry.first_msg_at);
	Write(result, "last_msg_at", entry.last_msg_at);
	Write(result, "last_msg_id", entry.last_msg_id);
	Write(result, "created_at", entry.created_at);
	Write(result, "sys_id", entry.sys_id);
	Write(result, "reserve1", entry.reserve1);
	Write(result, "reserve2", entry.reserve2);
	Write(result, "reserve3", entry.reserve3);
	Write(result, "reserve4", entry.reserve4);
	Write(result, "reserve5", entry.reserve5);
	Write(result, "reserve6", entry.reserve6);
	Write(result, "reserve7", entry.reserve7);
	Write(result, "reserve8", entry.reserve8);
	return result;
}

[[nodiscard]] Portrait PortraitFromJson(const QJsonObject &object) {
	auto result = Portrait();
	result.inner_id = ReadInt64(object, "inner_id");
	result.platform = ReadInt(object, "platform");
	result.key = ReadString(object, "key");
	result.worker_id = ReadString(object, "worker_id");
	result.stage = ReadInt(object, "stage");
	result.source = ReadInt(object, "source");
	result.id = ReadInt64(object, "id");
	result.type = ReadInt(object, "type");
	result.username = ReadString(object, "username");
	result.nickname = ReadString(object, "nickname");
	result.phone = ReadString(object, "phone");
	result.name = ReadString(object, "name");
	result.thumbnail = ReadString(object, "thumbnail");
	result.gender = ReadInt(object, "gender");
	result.birthday = ReadInt(object, "birthday");
	result.address = ReadString(object, "address");
	result.email = ReadString(object, "email");
	result.position = ReadString(object, "position");
	result.salary = ReadString(object, "salary");
	result.company = ReadString(object, "company");
	result.company_size = ReadString(object, "company_size");
	result.company_industry = ReadString(object, "company_industry");
	result.mark = ReadString(object, "mark");
	result.tags = ReadString(object, "tags");
	result.country = ReadString(object, "country");
	result.lang = ReadString(object, "lang");
	result.user_ids = ReadString(object, "user_ids");
	result.chat_ids = ReadString(object, "chat_ids");
	result.total_msgs = ReadInt(object, "total_msgs");
	result.total_chars = ReadInt(object, "total_chars");
	result.team_id = ReadInt64(object, "team_id");
	result.first_msg_at = ReadInt64(object, "first_msg_at");
	result.last_msg_at = ReadInt64(object, "last_msg_at");
	result.last_msg_id = ReadInt64(object, "last_msg_id");
	result.created_at = ReadInt64(object, "created_at");
	result.sys_id = ReadInt64(object, "sys_id");
	result.reserve1 = ReadString(object, "reserve1");
	result.reserve2 = ReadString(object, "reserve2");
	result.reserve3 = ReadString(object, "reserve3");
	result.reserve4 = ReadString(object, "reserve4");
	result.reserve5 = ReadString(object, "reserve5");
	result.reserve6 = ReadString(object, "reserve6");
	result.reserve7 = ReadString(object, "reserve7");
	result.reserve8 = ReadString(object, "reserve8");
	return result;
}

[[nodiscard]] QJsonObject Write(const Client &entry) {
	auto result = QJsonObject();
	Write(result, "inner_id", entry.inner_id);
	Write(result, "platform", entry.platform);
	Write(result, "key", entry.key);
	Write(result, "worker_id", entry.worker_id);
	Write(result, "username", entry.username);
	Write(result, "phone", entry.phone);
	Write(result, "id", entry.id);
	Write(result, "nickname", entry.nickname);
	Write(result, "state", entry.state);
	Write(result, "import_user_id", entry.import_user_id);
	Write(result, "imported_at", entry.imported_at);
	Write(result, "followed_at", entry.followed_at);
	Write(result, "created_at", entry.created_at);
	Write(result, "add_count", entry.add_count);
	Write(result, "team_id", entry.team_id);
	Write(result, "sent_count", entry.sent_count);
	Write(result, "recv_count", entry.recv_count);
	Write(result, "sys_id", entry.sys_id);
	Write(result, "reserve1", entry.reserve1);
	Write(result, "reserve2", entry.reserve2);
	Write(result, "reserve3", entry.reserve3);
	Write(result, "reserve4", entry.reserve4);
	Write(result, "reserve6", entry.reserve6);
	Write(result, "reserve7", entry.reserve7);
	Write(result, "reserve8", entry.reserve8);
	return result;
}

[[nodiscard]] Client ClientFromJson(const QJsonObject &object) {
	auto result = Client();
	result.inner_id = ReadInt64(object, "inner_id");
	result.platform = ReadInt(object, "platform");
	result.key = ReadString(object, "key");
	result.worker_id = ReadString(object, "worker_id");
	result.username = ReadString(object, "username");
	result.phone = ReadString(object, "phone");
	result.id = ReadString(object, "id");
	result.nickname = ReadString(object, "nickname");
	result.state = ReadInt(object, "state");
	result.import_user_id = ReadInt64(object, "import_user_id");
	result.imported_at = ReadInt64(object, "imported_at");
	result.followed_at = ReadInt64(object, "followed_at");
	result.created_at = ReadInt64(object, "created_at");
	result.add_count = ReadInt(object, "add_count");
	result.team_id = ReadInt64(object, "team_id");
	result.sent_count = ReadInt(object, "sent_count");
	result.recv_count = ReadInt(object, "recv_count");
	result.sys_id = ReadInt64(object, "sys_id");
	result.reserve1 = ReadString(object, "reserve1");
	result.reserve2 = ReadString(object, "reserve2");
	result.reserve3 = ReadString(object, "reserve3");
	result.reserve4 = ReadString(object, "reserve4");
	result.reserve6 = ReadString(object, "reserve6");
	result.reserve7 = ReadString(object, "reserve7");
	result.reserve8 = ReadString(object, "reserve8");
	return result;
}

[[nodiscard]] QJsonObject Write(const PortraitLog &entry) {
	auto result = QJsonObject();
	Write(result, "log_id", entry.log_id);
	Write(result, "platform", entry.platform);
	Write(result, "key", entry.key);
	Write(result, "key2", entry.key2);
	Write(result, "inner_id", entry.inner_id);
	Write(result, "change", entry.change);
	Write(result, "user_id", entry.user_id);
	Write(result, "team_id", entry.team_id);
	Write(result, "worker_id", entry.worker_id);
	Write(result, "sys_id", entry.sys_id);
	Write(result, "created_at", entry.created_at);
	return result;
}

[[nodiscard]] PortraitLog PortraitLogFromJson(const QJsonObject &object) {
	auto result = PortraitLog();
	result.log_id = ReadInt64(object, "log_id");
	result.platform = ReadInt(object, "platform");
	result.key = ReadString(object, "key");
	result.key2 = ReadString(object, "key2");
	result.inner_id = ReadInt64(object, "inner_id");
	result.change = ReadString(object, "change");
	result.user_id = ReadInt64(object, "user_id");
	result.team_id = ReadInt64(object, "team_id");
	result.worker_id = ReadString(object, "worker_id");
	result.sys_id = ReadInt64(object, "sys_id");
	result.created_at = ReadInt64(object, "created_at");
	return result;
}

[[nodiscard]] QJsonObject Write(const Tag &entry) {
	auto result = QJsonObject();
	Write(result, "label", entry.label);
	Write(result, "type", entry.type);
	Write(result, "num", entry.num);
	Write(result, "category", entry.category);
	Write(result, "worker_id", entry.worker_id);
	Write(result, "user_id", entry.user_id);
	Write(result, "team_id", entry.team_id);
	Write(result, "created_at", entry.created_at);
	return result;
}

[[nodiscard]] Tag TagFromJson(const QJsonObject &object) {
	auto result = Tag();
	result.label = ReadString(object, "label");
	result.type = ReadInt(object, "type");
	result.num = ReadInt(object, "num");
	result.category = ReadString(object, "category");
	result.worker_id = ReadString(object, "worker_id");
	result.user_id = ReadInt64(object, "user_id");
	result.team_id = ReadInt64(object, "team_id");
	result.created_at = ReadInt64(object, "created_at");
	return result;
}

[[nodiscard]] QJsonObject Write(const TagCategory &entry) {
	auto result = QJsonObject();
	Write(result, "label", entry.label);
	Write(result, "type", entry.type);
	Write(result, "num", entry.num);
	Write(result, "worker_id", entry.worker_id);
	Write(result, "user_id", entry.user_id);
	Write(result, "team_id", entry.team_id);
	Write(result, "created_at", entry.created_at);
	return result;
}

[[nodiscard]] TagCategory TagCategoryFromJson(const QJsonObject &object) {
	auto result = TagCategory();
	result.label = ReadString(object, "label");
	result.type = ReadInt(object, "type");
	result.num = ReadInt(object, "num");
	result.worker_id = ReadString(object, "worker_id");
	result.user_id = ReadInt64(object, "user_id");
	result.team_id = ReadInt64(object, "team_id");
	result.created_at = ReadInt64(object, "created_at");
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
	const auto loading = LoadingGuard(_loading);
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
	_portraits.clear();
	_portraitByInnerId.clear();
	_portraitByKey.clear();
	_clients.clear();
	_clientByInnerId.clear();
	_clientByKey.clear();
	_portraitLogs.clear();
	_portraitLogById.clear();
	_tags.clear();
	_tagByLabel.clear();
	_tagCategories.clear();
	_tagCategoryByLabel.clear();
	_dirty = false;
	_nextTodoId = 1;
	_nextArchiveId = 1;
	_nextPortraitId = 1;
	_nextClientId = 1;
	_nextPortraitLogId = 1;

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
	_nextPortraitId = std::max(ReadInt64(root, kNextPortraitIdKey), qint64(1));
	_nextClientId = std::max(ReadInt64(root, kNextClientIdKey), qint64(1));
	_nextPortraitLogId = std::max(
		ReadInt64(root, kNextPortraitLogIdKey),
		qint64(1));

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

	const auto portraits = root.value(Key(kPortraitsKey)).toArray();
	for (const auto &value : portraits) {
		const auto entry = PortraitFromJson(value.toObject());
		if (entry.key.isEmpty() && entry.worker_id.isEmpty()) {
			continue; // A profile without its identity key is not a row.
		}
		rememberPortraitRow(entry);
	}

	const auto clients = root.value(Key(kClientsKey)).toArray();
	for (const auto &value : clients) {
		const auto entry = ClientFromJson(value.toObject());
		if (entry.username.isEmpty() && entry.phone.isEmpty()) {
			continue;
		}
		rememberClientRow(entry);
	}

	const auto logs = root.value(Key(kPortraitLogsKey)).toArray();
	for (const auto &value : logs) {
		const auto entry = PortraitLogFromJson(value.toObject());
		if (!entry.inner_id && entry.change.isEmpty()) {
			continue; // A log with neither a subject nor a change is empty.
		}
		if (!entry.log_id) {
			addPortraitLog(entry);
			continue;
		}
		_nextPortraitLogId = std::max(_nextPortraitLogId, entry.log_id + 1);
		if (const auto i = _portraitLogById.find(entry.log_id);
			i != _portraitLogById.end()) {
			_portraitLogs[i->second] = entry;
		} else {
			_portraitLogById.emplace(
				entry.log_id,
				_portraitLogs.size());
			_portraitLogs.push_back(entry);
		}
	}

	const auto tags = root.value(Key(kTagsKey)).toArray();
	for (const auto &value : tags) {
		const auto entry = TagFromJson(value.toObject());
		if (entry.label.isEmpty()) {
			continue;
		}
		rememberTag(entry);
	}

	const auto categories = root.value(Key(kTagCategoriesKey)).toArray();
	for (const auto &value : categories) {
		const auto entry = TagCategoryFromJson(value.toObject());
		if (entry.label.isEmpty()) {
			continue;
		}
		rememberTagCategory(entry);
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
	if (_loading) {
		return;
	}
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

	auto portraits = QJsonArray();
	for (const auto &entry : _portraits) {
		portraits.append(Write(entry));
	}
	root.insert(Key(kPortraitsKey), portraits);

	auto clients = QJsonArray();
	for (const auto &entry : _clients) {
		clients.append(Write(entry));
	}
	root.insert(Key(kClientsKey), clients);

	auto logs = QJsonArray();
	for (const auto &entry : _portraitLogs) {
		logs.append(Write(entry));
	}
	root.insert(Key(kPortraitLogsKey), logs);

	auto tags = QJsonArray();
	for (const auto &entry : _tags) {
		tags.append(Write(entry));
	}
	root.insert(Key(kTagsKey), tags);

	auto categories = QJsonArray();
	for (const auto &entry : _tagCategories) {
		categories.append(Write(entry));
	}
	root.insert(Key(kTagCategoriesKey), categories);

	// The watermarks are part of the document, not of the tables: they keep
	// an id that a removed row used to occupy from being allocated again.
	Write(root, kNextTodoIdKey, _nextTodoId);
	Write(root, kNextArchiveIdKey, _nextArchiveId);
	Write(root, kNextPortraitIdKey, _nextPortraitId);
	Write(root, kNextClientIdKey, _nextClientId);
	Write(root, kNextPortraitLogIdKey, _nextPortraitLogId);

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

void Store::rememberTodoEvent(const TodoEntry &entry) {
	auto row = entry;
	const auto now = QDateTime::currentSecsSinceEpoch();
	const auto i = _todoByKey.find(todoKey(row));
	if (i == _todoByKey.end()) {
		row.is_new = (row.unread_count == row.from_count) ? 1 : 0;
		rememberTodo(row);
		return;
	}
	const auto index = i->second;
	const auto &existing = _todos[index];
	row.id = existing.id;
	row.created_at = existing.created_at ? existing.created_at : now;
	// The reference adds both counters instead of replacing them, so two
	// events of the same todo add up, and is_new drops to 0 as soon as the
	// accumulated unread count no longer matches the accumulated total.
	row.unread_count = existing.unread_count + entry.unread_count;
	row.from_count = existing.from_count + entry.from_count;
	row.is_new = (row.unread_count == row.from_count) ? 1 : 0;
	row.updated_at = row.updated_at ? row.updated_at : now;
	_todos[index] = row;
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

// Known deviation: the reference inserts an archive row with a plain INSERT
// and does not state ON CONFLICT, so a row that hits the unique key fails
// there instead of being replaced. We replace it, which is the looser of the
// two behaviours: a duplicate archive does not lose the call, and the row that
// is kept is the newest one.
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

void Store::upsertTodoEvent(const TodoEntry &entry) {
	rememberTodoEvent(entry);
	touch();
}

bool Store::setTodoRead(
		const QString &fromId,
		const QString &peerId,
		const QString &receiverId,
		int unreadCount) {
	const auto i = _todoByKey.find(TodoKey{ fromId, peerId, receiverId });
	if (i == _todoByKey.end()) {
		return false;
	}
	auto &row = _todos[i->second];
	// "Was read" is a separate UPDATE in the reference: the counter is
	// overwritten by the value the caller computed, not accumulated.
	row.unread_count = unreadCount;
	row.is_new = (row.unread_count == row.from_count) ? 1 : 0;
	row.updated_at = QDateTime::currentSecsSinceEpoch();
	touch();
	return true;
}

bool Store::applyTodoUpdate(
		const QString &fromId,
		const QString &peerId,
		const QString &receiverId,
		TodoStatus status,
		int unreadCount) {
	const auto i = _todoByKey.find(TodoKey{ fromId, peerId, receiverId });
	if (i == _todoByKey.end()) {
		return false;
	}
	auto &row = _todos[i->second];
	// One statement writes four columns, exactly like the reference UPDATE,
	// so a caller cannot leave the row between two writes.
	row.status = static_cast<int>(status);
	row.unread_count = unreadCount;
	// Inferred: the reference writes is_new inside that same statement, but
	// the value it passes for the "done" case is not covered by the evidence
	// we have, so the formula of the insert and accumulation paths is used
	// here again: new while nothing of this client was read.
	row.is_new = (row.unread_count == row.from_count) ? 1 : 0;
	row.updated_at = QDateTime::currentSecsSinceEpoch();
	touch();
	return true;
}

std::vector<TodoEntry> Store::todosByStatus(TodoStatus status) const {
	const auto code = static_cast<int>(status);
	auto result = std::vector<TodoEntry>();
	for (const auto &row : _todos) {
		if (row.status == code) {
			result.push_back(row);
		}
	}
	// The reference query orders by last_msg_at then updated_at, both
	// descending; created_at is never part of the list order.
	std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
		if (a.last_msg_at != b.last_msg_at) {
			return a.last_msg_at > b.last_msg_at;
		}
		return a.updated_at > b.updated_at;
	});
	return result;
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

bool Store::setArchiveTopicMsgId(qint64 archiveId, qint64 topicMsgId) {
	const auto i = _archiveById.find(archiveId);
	if (i == _archiveById.end()) {
		return false;
	}
	_archives[i->second].topic_msg_id = topicMsgId;
	touch();
	return true;
}

const TodoArchive *Store::findArchiveByTopicMsgId(qint64 topicMsgId) const {
	if (!topicMsgId) {
		return nullptr;
	}
	for (const auto &row : _archives) {
		if (row.topic_msg_id == topicMsgId) {
			return &row;
		}
	}
	return nullptr;
}

qint64 Store::archiveTopicMsgId(
		const QString &fromId,
		const QString &peerId,
		const QString &receiverId,
		qint64 msgId) const {
	const auto i = _archiveByKey.find(ArchiveKey{ peerId, msgId, receiverId });
	if (i == _archiveByKey.end()) {
		return 0;
	}
	const auto &row = _archives[i->second];
	// The reference looks the row up by all four columns, so an archive of a
	// different client under the same conversation and message is not it.
	return (row.from_id == fromId) ? row.topic_msg_id : 0;
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

Store::PortraitKey Store::portraitKey(const Portrait &entry) const {
	return PortraitKey{ entry.platform, entry.key, entry.worker_id };
}

Store::ClientKey Store::clientKey(const Client &entry) const {
	return ClientKey{ entry.platform, entry.username, entry.phone };
}

void Store::rebuildPortraitIndexes() {
	_portraitByInnerId.clear();
	_portraitByKey.clear();
	for (auto i = std::size_t(0); i != _portraits.size(); ++i) {
		_portraitByInnerId.emplace(_portraits[i].inner_id, i);
		_portraitByKey.emplace(portraitKey(_portraits[i]), i);
	}
}

void Store::rebuildClientIndexes() {
	_clientByInnerId.clear();
	_clientByKey.clear();
	for (auto i = std::size_t(0); i != _clients.size(); ++i) {
		_clientByInnerId.emplace(_clients[i].inner_id, i);
		_clientByKey.emplace(clientKey(_clients[i]), i);
	}
}

void Store::rememberPortraitRow(const Portrait &entry) {
	auto row = entry;
	const auto now = QDateTime::currentSecsSinceEpoch();

	if (const auto i = _portraitByKey.find(portraitKey(row));
		i != _portraitByKey.end()) {
		// UNIQUE(platform, key, worker_id): updated in place, the primary key
		// and the creation time the row already has are kept when unsent.
		const auto index = i->second;
		const auto existing = _portraits[index];
		if (!row.inner_id) {
			row.inner_id = existing.inner_id;
		}
		if (!row.created_at) {
			row.created_at = existing.created_at;
		}
		_portraits[index] = row;
		rebuildPortraitIndexes();
		return;
	}

	if (!row.inner_id) {
		row.inner_id = _nextPortraitId++;
	} else {
		_nextPortraitId = std::max(_nextPortraitId, row.inner_id + 1);
		if (const auto j = _portraitByInnerId.find(row.inner_id);
			j != _portraitByInnerId.end()) {
			// INSERT OR REPLACE: this id already belongs to another key.
			_portraits.erase(_portraits.begin() + std::ptrdiff_t(j->second));
		}
	}
	if (!row.created_at) {
		row.created_at = now;
	}
	_portraits.push_back(row);
	rebuildPortraitIndexes();
}

void Store::rememberClientRow(const Client &entry) {
	auto row = entry;
	const auto now = QDateTime::currentSecsSinceEpoch();

	if (const auto i = _clientByKey.find(clientKey(row));
		i != _clientByKey.end()) {
		// UNIQUE(platform, username, phone).
		const auto index = i->second;
		const auto existing = _clients[index];
		if (!row.inner_id) {
			row.inner_id = existing.inner_id;
		}
		if (!row.created_at) {
			row.created_at = existing.created_at;
		}
		_clients[index] = row;
		rebuildClientIndexes();
		return;
	}

	if (!row.inner_id) {
		row.inner_id = _nextClientId++;
	} else {
		_nextClientId = std::max(_nextClientId, row.inner_id + 1);
		if (const auto j = _clientByInnerId.find(row.inner_id);
			j != _clientByInnerId.end()) {
			_clients.erase(_clients.begin() + std::ptrdiff_t(j->second));
		}
	}
	if (!row.created_at) {
		row.created_at = now;
	}
	_clients.push_back(row);
	rebuildClientIndexes();
}

const std::vector<Portrait> &Store::portraits() const {
	return _portraits;
}

std::optional<Portrait> Store::portrait(qint64 innerId) const {
	const auto i = _portraitByInnerId.find(innerId);
	if (i == _portraitByInnerId.end()) {
		return std::nullopt;
	}
	return _portraits[i->second];
}

void Store::rememberPortrait(const Portrait &entry) {
	rememberPortraitRow(entry);
	touch();
}

bool Store::removePortrait(qint64 innerId) {
	const auto i = _portraitByInnerId.find(innerId);
	if (i == _portraitByInnerId.end()) {
		return false;
	}
	_portraits.erase(_portraits.begin() + std::ptrdiff_t(i->second));
	rebuildPortraitIndexes();
	touch();
	return true;
}

const std::vector<Client> &Store::clients() const {
	return _clients;
}

std::optional<Client> Store::client(qint64 innerId) const {
	const auto i = _clientByInnerId.find(innerId);
	if (i == _clientByInnerId.end()) {
		return std::nullopt;
	}
	return _clients[i->second];
}

void Store::rememberClient(const Client &entry) {
	rememberClientRow(entry);
	touch();
}

bool Store::removeClient(qint64 innerId) {
	const auto i = _clientByInnerId.find(innerId);
	if (i == _clientByInnerId.end()) {
		return false;
	}
	_clients.erase(_clients.begin() + std::ptrdiff_t(i->second));
	rebuildClientIndexes();
	touch();
	return true;
}

const std::vector<PortraitLog> &Store::portraitLogs() const {
	return _portraitLogs;
}

std::optional<PortraitLog> Store::portraitLog(qint64 logId) const {
	const auto i = _portraitLogById.find(logId);
	if (i == _portraitLogById.end()) {
		return std::nullopt;
	}
	return _portraitLogs[i->second];
}

qint64 Store::addPortraitLog(const PortraitLog &entry) {
	auto row = entry;
	if (!row.log_id) {
		row.log_id = _nextPortraitLogId++;
	} else {
		_nextPortraitLogId = std::max(_nextPortraitLogId, row.log_id + 1);
		// The log has no unique key of its own: the same id is a replace.
		if (const auto i = _portraitLogById.find(row.log_id);
			i != _portraitLogById.end()) {
			row.created_at = row.created_at
				? row.created_at
				: _portraitLogs[i->second].created_at;
			_portraitLogs[i->second] = row;
			touch();
			return row.log_id;
		}
	}
	if (!row.created_at) {
		row.created_at = QDateTime::currentSecsSinceEpoch();
	}
	_portraitLogById.emplace(row.log_id, _portraitLogs.size());
	_portraitLogs.push_back(row);
	touch();
	return row.log_id;
}

bool Store::removePortraitLog(qint64 logId) {
	const auto i = _portraitLogById.find(logId);
	if (i == _portraitLogById.end()) {
		return false;
	}
	_portraitLogs.erase(_portraitLogs.begin() + std::ptrdiff_t(i->second));
	_portraitLogById.clear();
	for (auto k = std::size_t(0); k != _portraitLogs.size(); ++k) {
		_portraitLogById.emplace(_portraitLogs[k].log_id, k);
	}
	touch();
	return true;
}

const std::vector<Tag> &Store::tags() const {
	return _tags;
}

const Tag *Store::tag(const QString &label) const {
	const auto i = _tagByLabel.find(label);
	return (i == _tagByLabel.end()) ? nullptr : &_tags[i->second];
}

void Store::rememberTag(const Tag &entry) {
	if (entry.label.isEmpty()) {
		return; // label is the primary key of this table.
	}
	auto row = entry;
	if (!row.created_at) {
		row.created_at = QDateTime::currentSecsSinceEpoch();
	}
	const auto i = _tagByLabel.find(row.label);
	if (i != _tagByLabel.end()) {
		_tags[i->second] = row;
		touch();
		return;
	}
	_tagByLabel.emplace(row.label, _tags.size());
	_tags.push_back(row);
	touch();
}

bool Store::removeTag(const QString &label) {
	const auto i = _tagByLabel.find(label);
	if (i == _tagByLabel.end()) {
		return false;
	}
	_tags.erase(_tags.begin() + std::ptrdiff_t(i->second));
	_tagByLabel.clear();
	for (auto k = std::size_t(0); k != _tags.size(); ++k) {
		_tagByLabel.emplace(_tags[k].label, k);
	}
	touch();
	return true;
}

const std::vector<TagCategory> &Store::tagCategories() const {
	return _tagCategories;
}

const TagCategory *Store::tagCategory(const QString &label) const {
	const auto i = _tagCategoryByLabel.find(label);
	return (i == _tagCategoryByLabel.end())
		? nullptr
		: &_tagCategories[i->second];
}

void Store::rememberTagCategory(const TagCategory &entry) {
	if (entry.label.isEmpty()) {
		return;
	}
	auto row = entry;
	if (!row.created_at) {
		row.created_at = QDateTime::currentSecsSinceEpoch();
	}
	const auto i = _tagCategoryByLabel.find(row.label);
	if (i != _tagCategoryByLabel.end()) {
		_tagCategories[i->second] = row;
		touch();
		return;
	}
	_tagCategoryByLabel.emplace(row.label, _tagCategories.size());
	_tagCategories.push_back(row);
	touch();
}

bool Store::removeTagCategory(const QString &label) {
	const auto i = _tagCategoryByLabel.find(label);
	if (i == _tagCategoryByLabel.end()) {
		return false;
	}
	_tagCategories.erase(
		_tagCategories.begin() + std::ptrdiff_t(i->second));
	_tagCategoryByLabel.clear();
	for (auto k = std::size_t(0); k != _tagCategories.size(); ++k) {
		_tagCategoryByLabel.emplace(_tagCategories[k].label, k);
	}
	touch();
	return true;
}

Store &StoreInstance() {
	// Constructed on first use: the working directory is only known once
	// Core::App() is up. Never destroyed on purpose - a static object would
	// run its destructor after Qt and Core::App() are gone, and the write it
	// does there would reach for a dead working directory. The 200ms save
	// timer flushes every burst of a running session; a quit inside that
	// window can lose the very last writes, which is the price of not
	// touching the shutdown sequence in this milestone.
	static auto instance = new Store();
	static auto loaded = false;
	if (!loaded) {
		loaded = true;
		// A missing or unreadable file is not an error here: the store simply
		// starts empty, which is also what a first run looks like.
		(void)instance->load();
	}
	return *instance;
}

} // namespace OG

