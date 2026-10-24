/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_video_page.h"

#include "og/og_store.h"
#include "og/og_tr.h"

#include <QtCore/QPointer>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

namespace OG {
namespace {

constexpr auto kMargin = 16;

// The rows are rebuilt from the pool on every refresh, and the refresh may be
// triggered from a button that lives inside the list: hide first, delete once
// the event loop is back.
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

VideoPageWindow::VideoPageWindow(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	bool embedded)
: QDialog(parent) {
	Q_UNUSED(controller);

	// Inside the console page stack the window decorations would be those of
	// a top level page, so they are dropped; see the auto-reply page for the
	// same reason.
	if (!embedded) {
		setWindowTitle(Tr::Lookup("VideoManagePage::Title"));
		setWindowFlags(
			Qt::Dialog
			| Qt::WindowTitleHint
			| Qt::WindowSystemMenuHint
			| Qt::WindowCloseButtonHint);
		resize(kWidth, kHeight);
	} else {
		setWindowFlags(Qt::Widget);
	}
	setupContent();
	rebuild();
}

void VideoPageWindow::Show(
		not_null<Window::SessionController*> controller) {
	static auto instance = QPointer<VideoPageWindow>();
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
	const auto window = new VideoPageWindow(nullptr, controller);
	window->setAttribute(Qt::WA_DeleteOnClose);
	instance = window;
	window->show();
	window->raise();
	window->activateWindow();
}

void VideoPageWindow::reject() {
	// Escape hides, so a half typed address survives the next Show(), the
	// same contract the other pages keep.
	hide();
}

void VideoPageWindow::setupContent() {
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
	layout->setSpacing(10);

	auto *linkLine = new QHBoxLayout();
	linkLine->setSpacing(8);
	linkLine->addWidget(new QLabel(Tr::Lookup(
		"VideoManagePage::Group link or @username"), this));
	_link = new QLineEdit(this);
	_link->setPlaceholderText(Tr::Lookup(
		"VideoManagePage::Group link or @username"));
	linkLine->addWidget(_link, 1);
	layout->addLayout(linkLine);

	auto *intervalLine = new QHBoxLayout();
	intervalLine->setSpacing(8);
	intervalLine->addWidget(new QLabel(Tr::Lookup(
		"VideoManagePage::Interval (ms)"), this));
	_interval = new QSpinBox(this);
	_interval->setRange(0, 600000);
	_interval->setSingleStep(100);
	intervalLine->addWidget(_interval);
	intervalLine->addStretch(1);
	layout->addLayout(intervalLine);

	layout->addWidget(new QLabel(Tr::Lookup(
		"VideoManagePage::Active Accounts"), this));

	// The pool is small enough that a plain column of check boxes is both
	// simpler and more readable than a table widget, and it matches the hand
	// built rows of the other pages.
	_accountsHost = new QWidget(this);
	_accounts = new QVBoxLayout(_accountsHost);
	_accounts->setContentsMargins(0, 0, 0, 0);
	_accounts->setSpacing(4);
	_accounts->setAlignment(Qt::AlignTop);
	layout->addWidget(_accountsHost, 1);

	auto *buttons = new QHBoxLayout();
	buttons->setSpacing(8);
	_join = new QPushButton(Tr::Lookup("VideoManagePage::Join"), this);
	_leave = new QPushButton(Tr::Lookup("VideoManagePage::Leave"), this);
	buttons->addWidget(_join);
	buttons->addWidget(_leave);
	buttons->addStretch(1);
	layout->addLayout(buttons);

	_status = new QLabel(this);
	_status->setWordWrap(true);
	layout->addWidget(_status);

	connect(_link, &QLineEdit::textChanged, this, [this] {
		save();
	});
	connect(
		_interval,
		QOverload<int>::of(&QSpinBox::valueChanged),
		this,
		[this] {
			save();
		});
	connect(_join, &QPushButton::clicked, this, [this] {
		act(GroupCallAction::Join);
	});
	connect(_leave, &QPushButton::clicked, this, [this] {
		act(GroupCallAction::Leave);
	});
}

void VideoPageWindow::rebuild() {
	const auto config = StoreInstance().videoManageSettings();
	// The setters emit their change signals, and those handlers save: block
	// them while the stored values go in, so opening the page cannot rewrite
	// what it just read.
	_link->blockSignals(true);
	_interval->blockSignals(true);
	_link->setText(config.link);
	_interval->setValue(config.intervalMs);
	_interval->blockSignals(false);
	_link->blockSignals(false);

	reloadAccounts();
}

void VideoPageWindow::reloadAccounts() {
	ClearLayout(_accounts);

	const auto &pool = StoreInstance().accounts();
	if (pool.empty()) {
		auto *empty = new QLabel(Tr::Lookup(
			"VideoManagePage::No accounts in the pool"), _accountsHost);
		empty->setWordWrap(true);
		_accounts->addWidget(empty);
		_accounts->addStretch(1);
		return;
	}
	const auto config = StoreInstance().videoManageSettings();
	for (const auto &row : pool) {
		auto *box = new QCheckBox(row.label, _accountsHost);
		box->setChecked(config.accounts.contains(row.label));
		const auto label = row.label;
		connect(box, &QCheckBox::toggled, this, [this, label](bool on) {
			auto config = StoreInstance().videoManageSettings();
			if (on) {
				if (!config.accounts.contains(label)) {
					config.accounts.append(label);
				}
			} else {
				config.accounts.removeAll(label);
			}
			StoreInstance().setVideoManageSettings(config);
			save();
		});
		_accounts->addWidget(box);
	}
	_accounts->addStretch(1);
}

void VideoPageWindow::save() {
	auto config = StoreInstance().videoManageSettings();
	config.link = NormalizeGroupLink(_link->text());
	config.intervalMs = _interval->value();
	StoreInstance().setVideoManageSettings(config);
}

void VideoPageWindow::act(GroupCallAction action) {
	save();

	auto config = StoreInstance().videoManageSettings();
	// An account that left the pool is dropped from the request and from the
	// stored selection, so the next open shows the same set that would run.
	config.accounts = ResolveVideoAccounts(config, StoreInstance().accounts());
	StoreInstance().setVideoManageSettings(config);

	const auto issue = CheckVideoRequest(config);
	if (issue != VideoRequestIssue::None) {
		const auto key = (issue == VideoRequestIssue::MissingLink)
			? "VideoManagePage::Group link is required"
			: "VideoManagePage::Please choose accounts";
		_status->setText(Tr::Lookup(key));
		return;
	}
	// The rule of this milestone: the page decides what the request would be,
	// the session that sends it belongs to the next one. The line names the
	// request, so the acceptance run reads back what the page understood from
	// the address the operator typed.
	const auto verb = (action == GroupCallAction::Join)
		? "VideoManagePage::Join"
		: "VideoManagePage::Leave";
	_status->setText(
		Tr::Lookup("VideoManagePage::Task is running")
		+ QString::fromLatin1(": ")
		+ Tr::Lookup(verb)
		+ QString::fromLatin1(" ")
		+ NormalizeGroupLink(config.link)
		+ QString::fromLatin1(" (")
		+ QString::number(config.accounts.size())
		+ QString::fromLatin1(")"));
}

} // namespace OG
