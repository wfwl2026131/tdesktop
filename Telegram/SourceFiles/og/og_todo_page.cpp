/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_todo_page.h"

#include "og/og_tr.h"

#include <QtCore/QDateTime>
#include <QtCore/QPointer>

#include <QtGui/QFontMetrics>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>

namespace OG {
namespace {

constexpr auto kAvatarSize = 32;
constexpr auto kDescriptionWidth = 380;
constexpr auto kRowMargin = 10;

[[nodiscard]] QString TimeText(qint64 seconds) {
	if (seconds <= 0) {
		return QString::fromLatin1("-");
	}
	return QDateTime::fromSecsSinceEpoch(seconds).toString(
		QString::fromLatin1("yyyy-MM-dd HH:mm"));
}

[[nodiscard]] QString NameText(const TodoEntry &entry) {
	return entry.from_name.isEmpty() ? entry.from_id : entry.from_name;
}

[[nodiscard]] QString DescriptionText(const TodoEntry &entry) {
	return entry.title.isEmpty() ? entry.msg : entry.title;
}

// The row builder below is called from the click handler of a button that
// lives inside the row being dropped, so the widgets are hidden first and
// destroyed once the event loop returns.
void ClearLayout(QVBoxLayout *layout) {
	while (auto *item = layout->takeAt(0)) {
		if (auto *widget = item->widget()) {
			widget->hide();
			widget->deleteLater();
		}
		delete item;
	}
}

} // namespace

TodoPageWindow::TodoPageWindow(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: QDialog(parent) {
	// The controller is part of the signature for parity with the console
	// window and for the entry point that is still being decided; the page
	// itself talks to the store only.
	Q_UNUSED(controller);

	setWindowTitle(Tr::Lookup("TodoPage::Title"));
	setWindowFlags(
		Qt::Dialog
		| Qt::WindowTitleHint
		| Qt::WindowSystemMenuHint
		| Qt::WindowCloseButtonHint);
	resize(kWidth, kHeight);
	setupContent();
	rebuild();
}

void TodoPageWindow::Show(
		not_null<Window::SessionController*> controller) {
	static auto instance = QPointer<TodoPageWindow>();
	if (instance) {
		if (instance->windowState() & Qt::WindowMinimized) {
			instance->setWindowState(
				(instance->windowState() & ~Qt::WindowMinimized)
				| Qt::WindowActive);
		}
		instance->show();
		instance->raise();
		instance->activateWindow();
		return;
	}
	const auto window = new TodoPageWindow(nullptr, controller);
	window->setAttribute(Qt::WA_DeleteOnClose);
	instance = window;
	window->show();
	window->raise();
	window->activateWindow();
}

void TodoPageWindow::reject() {
	// Escape hides the window instead of destroying it, so the next Show()
	// brings back the same instance and the selection survives.
	hide();
}

void TodoPageWindow::setupContent() {
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	auto *tabs = new QHBoxLayout();
	tabs->setContentsMargins(kRowMargin, kRowMargin, kRowMargin, 0);
	_tabPending = new QPushButton(Tr::Lookup("TodoPage::Pending"), this);
	_tabDone = new QPushButton(Tr::Lookup("TodoPage::Done"), this);
	_tabArchived = new QPushButton(
		Tr::Lookup("TodoPage::Archived"),
		this);
	for (const auto button : { _tabPending, _tabDone, _tabArchived }) {
		button->setCheckable(true);
		button->setAutoExclusive(true);
		tabs->addWidget(button);
	}
	tabs->addStretch(1);
	layout->addLayout(tabs);

	connect(_tabPending, &QPushButton::clicked, this, [this] {
		_status = TodoStatus::Pending;
		rebuild();
	});
	connect(_tabDone, &QPushButton::clicked, this, [this] {
		_status = TodoStatus::Done;
		rebuild();
	});
	connect(_tabArchived, &QPushButton::clicked, this, [this] {
		_status = TodoStatus::Archived;
		rebuild();
	});

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	layout->addWidget(scroll, 1);

	_rowsHost = new QWidget();
	_rows = new QVBoxLayout(_rowsHost);
	_rows->setContentsMargins(kRowMargin, 0, kRowMargin, kRowMargin);
	_rows->setSpacing(2);
	_rows->setAlignment(Qt::AlignTop);
	scroll->setWidget(_rowsHost);
}

void TodoPageWindow::rebuild() {
	ClearLayout(_rows);

	_tabPending->setChecked(_status == TodoStatus::Pending);
	_tabDone->setChecked(_status == TodoStatus::Done);
	_tabArchived->setChecked(_status == TodoStatus::Archived);

	auto empty = false;
	if (_status == TodoStatus::Archived) {
		const auto &rows = StoreInstance().archives();
		empty = rows.empty();
		for (const auto &row : rows) {
			addArchiveRow(row);
		}
	} else {
		const auto rows = StoreInstance().todosByStatus(_status);
		empty = rows.empty();
		for (const auto &row : rows) {
			addTodoRow(row);
		}
	}

	if (empty) {
		auto *label = new QLabel(Tr::Lookup("TodoPage::Empty"), _rowsHost);
		label->setAlignment(Qt::AlignCenter);
		_rows->addWidget(label);
	}
	// The trailing stretch keeps the rows at the top: without it the labels
	// would share out the free height and every row would look oversized.
	_rows->addStretch(1);
}

void TodoPageWindow::addTodoRow(const TodoEntry &entry) {
	auto *row = new QWidget(_rowsHost);
	auto *line = new QHBoxLayout(row);
	line->setContentsMargins(0, 6, 0, 6);
	line->setSpacing(8);

	if (entry.is_new == 1) {
		auto *flag = new QLabel(Tr::Lookup("TodoPage::New Client"), row);
		flag->setStyleSheet(QString::fromLatin1("color:#d32f2f;"));
		line->addWidget(flag);
	}

	const auto name = NameText(entry);
	auto *avatar = new QLabel(
		name.isEmpty() ? QString::fromLatin1("-") : name.left(1).toUpper(),
		row);
	avatar->setFixedSize(kAvatarSize, kAvatarSize);
	avatar->setAlignment(Qt::AlignCenter);
	avatar->setStyleSheet(QString::fromLatin1(
		"background:#e0e0e0;border-radius:16px;"));
	if (!entry.from_avatar.isEmpty()) {
		// The column holds the reference of the avatar, not the bitmap;
		// resolving it needs the avatar cache lane, so the reference is
		// exposed as a tooltip instead of being dropped.
		avatar->setToolTip(entry.from_avatar);
	}
	line->addWidget(avatar);

	auto *texts = new QVBoxLayout();
	line->addLayout(texts, 1);

	auto *main = new QLabel(row);
	auto mainText = name;
	if (!entry.peer_name.isEmpty()) {
		mainText += QString::fromLatin1(" / ") + entry.peer_name;
	}
	main->setText(mainText);
	texts->addWidget(main);

	auto *description = new QLabel(row);
	const auto descriptionText = DescriptionText(entry);
	if (!descriptionText.isEmpty()) {
		const auto metrics = QFontMetrics(description->font());
		description->setText(metrics.elidedText(
			descriptionText,
			Qt::ElideRight,
			kDescriptionWidth));
	}
	texts->addWidget(description);

	auto peerOk = false;
	const auto peer = entry.peer_id.toLongLong(&peerOk);
	if (peerOk) {
		if (const auto topic = StoreInstance().topic(entry.id, peer)) {
			auto *topics = new QLabel(row);
			topics->setText(
				Tr::Lookup("TodoPage::Topic")
				+ QString::fromLatin1(": ")
				+ topic->topic_id
				+ QString::fromLatin1(" / ")
				+ Tr::Lookup("TodoPage::Dispatcher")
				+ QString::fromLatin1(": ")
				+ topic->dispatcher_id);
			topics->setStyleSheet(QString::fromLatin1("color:#757575;"));
			texts->addWidget(topics);
		}
	}

	if (entry.unread_count > 0) {
		auto *unread = new QLabel(
			QString::number(entry.unread_count),
			row);
		unread->setStyleSheet(QString::fromLatin1("color:#d32f2f;"));
		line->addWidget(unread);
	}

	auto *total = new QLabel(
		Tr::Lookup("TodoPage::Total")
		+ QString::fromLatin1(" ")
		+ QString::number(entry.from_count),
		row);
	line->addWidget(total);

	auto *time = new QLabel(TimeText(entry.last_msg_at), row);
	line->addWidget(time);

	// The buttons work on the row as it is stored now, not on the snapshot
	// this row was built from, so a write can never carry a stale status or
	// counter back into the store.
	const auto todoId = entry.id;
	auto *read = new QPushButton(Tr::Lookup("TodoPage::Mark Read"), row);
	line->addWidget(read);
	connect(read, &QPushButton::clicked, this, [this, todoId] {
		const auto current = StoreInstance().todo(todoId);
		if (current) {
			StoreInstance().applyTodoUpdate(
				current->from_id,
				current->peer_id,
				current->receiver_id,
				static_cast<TodoStatus>(current->status),
				0);
		}
		rebuild();
	});

	auto *done = new QPushButton(Tr::Lookup("TodoPage::Complete"), row);
	line->addWidget(done);
	connect(done, &QPushButton::clicked, this, [this, todoId] {
		// The unread counter is handed over unchanged: marking a todo done
		// must not pretend that its messages were read.
		const auto current = StoreInstance().todo(todoId);
		if (current) {
			StoreInstance().applyTodoUpdate(
				current->from_id,
				current->peer_id,
				current->receiver_id,
				TodoStatus::Done,
				current->unread_count);
		}
		rebuild();
	});

	auto *archive = new QPushButton(Tr::Lookup("TodoPage::Archive"), row);
	line->addWidget(archive);
	connect(archive, &QPushButton::clicked, this, [this, todoId] {
		const auto current = StoreInstance().todo(todoId);
		if (current) {
			StoreInstance().applyTodoUpdate(
				current->from_id,
				current->peer_id,
				current->receiver_id,
				TodoStatus::Archived,
				current->unread_count);
		}
		rebuild();
	});

	_rows->addWidget(row);
}

void TodoPageWindow::addArchiveRow(const TodoArchive &entry) {
	auto *row = new QWidget(_rowsHost);
	auto *line = new QHBoxLayout(row);
	line->setContentsMargins(0, 6, 0, 6);
	line->setSpacing(8);

	auto *texts = new QVBoxLayout();
	line->addLayout(texts, 1);

	auto *main = new QLabel(row);
	main->setText(
		Tr::Lookup("TodoPage::Peer")
		+ QString::fromLatin1(": ")
		+ entry.peer_id
		+ QString::fromLatin1(" / ")
		+ Tr::Lookup("TodoPage::Message")
		+ QString::fromLatin1(": ")
		+ QString::number(entry.msg_id));
	texts->addWidget(main);

	auto *description = new QLabel(row);
	const auto descriptionText = entry.title.isEmpty()
		? entry.msg
		: entry.title;
	if (!descriptionText.isEmpty()) {
		const auto metrics = QFontMetrics(description->font());
		description->setText(metrics.elidedText(
			descriptionText,
			Qt::ElideRight,
			kDescriptionWidth));
	}
	texts->addWidget(description);

	auto *topic = new QLabel(
		Tr::Lookup("TodoPage::Topic Message")
		+ QString::fromLatin1(": ")
		+ QString::number(entry.topic_msg_id),
		row);
	line->addWidget(topic);

	auto *time = new QLabel(TimeText(entry.created_at), row);
	line->addWidget(time);

	_rows->addWidget(row);
}

} // namespace OG
