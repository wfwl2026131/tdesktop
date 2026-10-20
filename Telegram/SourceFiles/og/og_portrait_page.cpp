/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_portrait_page.h"

#include "og/og_tr.h"

#include <QtCore/QDateTime>
#include <QtCore/QPointer>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>

#include <utility>
#include <vector>

namespace OG {
namespace {

constexpr auto kRowMargin = 10;

using Field = std::pair<const char *, QString>;

[[nodiscard]] QString TimeText(qint64 seconds) {
	if (seconds <= 0) {
		return QString::fromLatin1("-");
	}
	return QDateTime::fromSecsSinceEpoch(seconds).toString(
		QString::fromLatin1("yyyy-MM-dd HH:mm"));
}

[[nodiscard]] QString ValueText(const QString &value) {
	return value.isEmpty() ? QString::fromLatin1("-") : value;
}

void ClearLayout(QVBoxLayout *layout) {
	while (auto *item = layout->takeAt(0)) {
		if (auto *widget = item->widget()) {
			widget->hide();
			widget->deleteLater();
		}
		delete item;
	}
}

void AddTitle(QVBoxLayout *layout, QWidget *parent, const QString &text) {
	auto *label = new QLabel(text, parent);
	auto font = label->font();
	font.setBold(true);
	label->setFont(font);
	layout->addWidget(label);
}

void AddLine(QVBoxLayout *layout, QWidget *parent, const QString &text) {
	auto *label = new QLabel(text, parent);
	label->setWordWrap(true);
	layout->addWidget(label);
}

void AddSection(
		QVBoxLayout *layout,
		QWidget *parent,
		const char *titleKey,
		const std::vector<Field> &fields) {
	AddTitle(layout, parent, OG::Tr::Lookup(titleKey));
	for (const auto &field : fields) {
		AddLine(
			layout,
			parent,
			OG::Tr::Lookup(field.first)
				+ QString::fromLatin1(": ")
				+ ValueText(field.second));
	}
}

// The reference matches a portrait to a client by platform and key, and the
// seat separates two rows that agree on both, so it wins when both sides
// carry one.
[[nodiscard]] const Portrait *FindPortrait(const Client &client) {
	const auto &all = OG::StoreInstance().portraits();
	const Portrait *fallback = nullptr;
	for (const auto &portrait : all) {
		if (portrait.platform != client.platform
			|| portrait.key != client.key) {
			continue;
		}
		if (!client.worker_id.isEmpty()
			&& portrait.worker_id == client.worker_id) {
			return &portrait;
		}
		if (!fallback) {
			fallback = &portrait;
		}
	}
	return fallback;
}

} // namespace

PortraitPageWindow::PortraitPageWindow(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: QDialog(parent) {
	// The controller is part of the signature for parity with the console
	// window and for the entry point that is still being decided; the page
	// itself talks to the store only.
	Q_UNUSED(controller);

	setWindowTitle(Tr::Lookup("PortraitPage::Title"));
	setWindowFlags(
		Qt::Dialog
		| Qt::WindowTitleHint
		| Qt::WindowSystemMenuHint
		| Qt::WindowCloseButtonHint);
	resize(kWidth, kHeight);
	setupContent();
	rebuildClients();
	showPortrait();
}

void PortraitPageWindow::Show(
		not_null<Window::SessionController*> controller) {
	static auto instance = QPointer<PortraitPageWindow>();
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
	const auto window = new PortraitPageWindow(nullptr, controller);
	window->setAttribute(Qt::WA_DeleteOnClose);
	instance = window;
	window->show();
	window->raise();
	window->activateWindow();
}

void PortraitPageWindow::reject() {
	// Escape hides the window instead of destroying it, so the next Show()
	// brings back the same instance and the selection survives.
	hide();
}

void PortraitPageWindow::setupContent() {
	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(kRowMargin, kRowMargin, kRowMargin, kRowMargin);

	auto *clientsScroll = new QScrollArea(this);
	clientsScroll->setWidgetResizable(true);
	clientsScroll->setFrameShape(QFrame::NoFrame);
	clientsScroll->setFixedWidth(kClientColumnWidth);
	layout->addWidget(clientsScroll);

	_clientHost = new QWidget();
	_clientRows = new QVBoxLayout(_clientHost);
	_clientRows->setContentsMargins(0, 0, 0, 0);
	_clientRows->setSpacing(2);
	_clientRows->setAlignment(Qt::AlignTop);
	clientsScroll->setWidget(_clientHost);

	auto *detailsScroll = new QScrollArea(this);
	detailsScroll->setWidgetResizable(true);
	detailsScroll->setFrameShape(QFrame::NoFrame);
	layout->addWidget(detailsScroll, 1);

	_detailsHost = new QWidget();
	_details = new QVBoxLayout(_detailsHost);
	_details->setContentsMargins(kRowMargin, 0, kRowMargin, 0);
	_details->setSpacing(2);
	_details->setAlignment(Qt::AlignTop);
	detailsScroll->setWidget(_detailsHost);
}

void PortraitPageWindow::rebuildClients() {
	ClearLayout(_clientRows);

	const auto &clients = StoreInstance().clients();
	if (clients.empty()) {
		auto *label = new QLabel(
			Tr::Lookup("PortraitPage::No Clients"),
			_clientHost);
		label->setAlignment(Qt::AlignCenter);
		_clientRows->addWidget(label);
		return;
	}

	for (const auto &client : clients) {
		auto text = QString::number(client.platform)
			+ QString::fromLatin1(" / ")
			+ ValueText(client.username)
			+ QString::fromLatin1(" / ")
			+ ValueText(client.phone);
		if (!client.nickname.isEmpty()) {
			text += QString::fromLatin1(" / ") + client.nickname;
		}
		text += QString::fromLatin1("\n")
			+ Tr::Lookup("PortraitPage::State")
			+ QString::fromLatin1(" ")
			+ QString::number(client.state)
			+ QString::fromLatin1(" / ")
			+ Tr::Lookup("PortraitPage::Add Count")
			+ QString::fromLatin1(" ")
			+ QString::number(client.add_count)
			+ QString::fromLatin1(" / ")
			+ Tr::Lookup("PortraitPage::Sent Count")
			+ QString::fromLatin1(" ")
			+ QString::number(client.sent_count)
			+ QString::fromLatin1(" / ")
			+ Tr::Lookup("PortraitPage::Recv Count")
			+ QString::fromLatin1(" ")
			+ QString::number(client.recv_count);

		auto *button = new QPushButton(text, _clientHost);
		button->setCheckable(true);
		button->setChecked(client.inner_id == _selected);
		const auto innerId = client.inner_id;
		connect(button, &QPushButton::clicked, this, [this, innerId] {
			_selected = innerId;
			rebuildClients();
			showPortrait();
		});
		_clientRows->addWidget(button);
	}
}

void PortraitPageWindow::showPortrait() {
	ClearLayout(_details);

	if (!_selected) {
		auto *label = new QLabel(
			Tr::Lookup("PortraitPage::Choose Client"),
			_detailsHost);
		label->setAlignment(Qt::AlignCenter);
		_details->addWidget(label);
		return;
	}

	const auto client = StoreInstance().client(_selected);
	if (!client) {
		auto *label = new QLabel(
			Tr::Lookup("PortraitPage::Choose Client"),
			_detailsHost);
		label->setAlignment(Qt::AlignCenter);
		_details->addWidget(label);
		return;
	}

	const auto *portrait = FindPortrait(*client);
	if (!portrait) {
		AddTitle(
			_details,
			_detailsHost,
			ValueText(client->username));
		AddLine(
			_details,
			_detailsHost,
			Tr::Lookup("PortraitPage::No Portrait"));
		return;
	}

	const auto &p = *portrait;

	AddSection(_details, _detailsHost, "PortraitPage::Group Meta", {
		{ "PortraitPage::Platform", QString::number(p.platform) },
		{ "PortraitPage::Key", p.key },
		{ "PortraitPage::Inner Id", QString::number(p.inner_id) },
		{ "PortraitPage::Thumbnail", p.thumbnail },
	});

	AddSection(_details, _detailsHost, "PortraitPage::Group Identity", {
		{ "PortraitPage::Username", p.username },
		{ "PortraitPage::Nickname", p.nickname },
		{ "PortraitPage::Name", p.name },
		{ "PortraitPage::Gender", QString::number(p.gender) },
		{ "PortraitPage::Birthday", QString::number(p.birthday) },
		{ "PortraitPage::Country", p.country },
		{ "PortraitPage::Language", p.lang },
		{ "PortraitPage::Phone", p.phone },
		{ "PortraitPage::Email", p.email },
		{ "PortraitPage::Address", p.address },
	});

	AddSection(_details, _detailsHost, "PortraitPage::Group Career", {
		{ "PortraitPage::Company", p.company },
		{ "PortraitPage::Position", p.position },
		{ "PortraitPage::Salary", p.salary },
		{ "PortraitPage::Company Size", p.company_size },
		{ "PortraitPage::Company Industry", p.company_industry },
	});

	AddSection(
		_details,
		_detailsHost,
		"PortraitPage::Group Communication",
		{
			{
				"PortraitPage::Total Messages",
				QString::number(p.total_msgs),
			},
			{
				"PortraitPage::Total Chars",
				QString::number(p.total_chars),
			},
			{
				"PortraitPage::First Message At",
				TimeText(p.first_msg_at),
			},
			{
				"PortraitPage::Last Message At",
				TimeText(p.last_msg_at),
			},
			{
				"PortraitPage::Last Message Id",
				QString::number(p.last_msg_id),
			},
			{ "PortraitPage::Created At", TimeText(p.created_at) },
		});

	AddSection(_details, _detailsHost, "PortraitPage::Group Relation", {
		{ "PortraitPage::Id", QString::number(p.id) },
		{ "PortraitPage::Type", QString::number(p.type) },
		{ "PortraitPage::Stage", QString::number(p.stage) },
		{ "PortraitPage::Source", QString::number(p.source) },
		{ "PortraitPage::Team Id", QString::number(p.team_id) },
		{ "PortraitPage::Sys Id", QString::number(p.sys_id) },
		{ "PortraitPage::Worker Id", p.worker_id },
		{ "PortraitPage::User Ids", p.user_ids },
		{ "PortraitPage::Chat Ids", p.chat_ids },
	});

	AddSection(_details, _detailsHost, "PortraitPage::Group Remark", {
		{ "PortraitPage::Mark", p.mark },
		// One TEXT column of the reference, not a JSON array: it is shown
		// exactly as it is stored and never parsed here.
		{ "PortraitPage::Tags", p.tags },
	});

	const auto &logs = StoreInstance().portraitLogs();
	auto changes = 0;
	for (const auto &log : logs) {
		if (log.inner_id == p.inner_id
			|| (!p.key.isEmpty() && log.key == p.key)) {
			++changes;
		}
	}
	AddTitle(_details, _detailsHost, Tr::Lookup("PortraitPage::Timeline"));
	if (!changes) {
		AddLine(
			_details,
			_detailsHost,
			Tr::Lookup("PortraitPage::No Changes"));
	} else {
		for (const auto &log : logs) {
			if (log.inner_id != p.inner_id
				&& (p.key.isEmpty() || log.key != p.key)) {
				continue;
			}
			AddLine(
				_details,
				_detailsHost,
				TimeText(log.created_at)
					+ QString::fromLatin1("  ")
					+ ValueText(log.change));
		}
	}

	const auto &categories = StoreInstance().tagCategories();
	const auto &tags = StoreInstance().tags();
	if (!categories.empty() || !tags.empty()) {
		AddTitle(_details, _detailsHost, Tr::Lookup("PortraitPage::Tags"));
		for (const auto &category : categories) {
			AddTitle(
				_details,
				_detailsHost,
				ValueText(category.label)
					+ QString::fromLatin1(" (")
					+ QString::number(category.num)
					+ QString::fromLatin1(")"));
			for (const auto &tag : tags) {
				if (tag.category != category.label) {
					continue;
				}
				AddLine(
					_details,
					_detailsHost,
					ValueText(tag.label)
						+ QString::fromLatin1(" (")
						+ QString::number(tag.num)
						+ QString::fromLatin1(")"));
			}
		}
		for (const auto &tag : tags) {
			if (!tag.category.isEmpty()
				&& StoreInstance().tagCategory(tag.category)) {
				continue;
			}
			AddLine(
				_details,
				_detailsHost,
				ValueText(tag.label)
					+ QString::fromLatin1(" (")
					+ QString::number(tag.num)
					+ QString::fromLatin1(")"));
		}
	}
}

} // namespace OG
