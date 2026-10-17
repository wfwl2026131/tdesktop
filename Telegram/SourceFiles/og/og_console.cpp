/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_console.h"

#include "base/object_ptr.h"
#include "og/og_seat_grid.h"
#include "og/og_tr.h"
#include <rpl/producer.h>
#include "settings/settings_common.h"
#include "ui/qt_object_factory.h"
#include "ui/rp_widget.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/table_layout.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_window.h"

#include <QtCore/QPointer>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>

namespace Deep {
namespace {

constexpr auto kToastDuration = 1500;

// Keys only - every label goes through the Deep translation data below, so no
// interface copy is compiled into the fork. English stays as the last
// fallback, exactly like the reference client's resource chain.
constexpr auto kTitleKey = "ConsoleWindowQt::Title";
constexpr auto kSeatHintKey = "ConsoleWindowQt::Seat grid hint";
constexpr auto kFeaturesKey = "ConsoleWindowQt::Features";
constexpr auto kActionsKey = "ConsoleWindowQt::Actions";
constexpr auto kActionFieldsKey = "ConsoleWindowQt::Action fields";
constexpr auto kStartKey = "ConsoleWindowQt::Start Task";
constexpr auto kStopKey = "ConsoleWindowQt::Stop Task";
constexpr auto kCloseKey = "Common::Close";
constexpr auto kM2Key = "ConsoleWindowQt::M2 placeholder";

struct LicenseRow {
	const char *key;
	const char *value;
};

const LicenseRow kLicenseRows[] = {
	{"ConsoleWindowQt::Team", "ConsoleWindowQt::Team value"},
	{"ConsoleWindowQt::User", "ConsoleWindowQt::User value"},
	{"ConsoleWindowQt::Seat", "ConsoleWindowQt::Seat value"},
	{"ConsoleWindowQt::Expire", "ConsoleWindowQt::Expire value"},
	{"ConsoleWindowQt::Pointcard", "ConsoleWindowQt::Pointcard value"},
};

struct FeatureRow {
	const char *name;
	const char *tech;
	const style::icon *icon;
};

// The 22 rows of the console's left column (M1.5a list kept as-is, now keyed).
const FeatureRow kFeatures[] = {
	{"ConsoleWindowQt::Red Packet", "red_packet", &st::menuIconGroups},
	{"ConsoleWindowQt::Cross Trading", "cross_trading", &st::menuIconSavedMessages},
	{"ConsoleWindowQt::Mass Send DM v2", "mass_send_dm_v2", &st::menuIconUserShow},
	{"ConsoleWindowQt::Add Contacts", "add_contacts", &st::menuIconUserShow},
	{"ConsoleWindowQt::Batch Invite", "batch_invite", &st::menuIconUserShow},
	{"ConsoleWindowQt::Puppet Chat v2", "puppet_chat_v2", &st::menuIconSavedMessages},
	{"ConsoleWindowQt::Translation", "translation", &st::menuIconSavedMessages},
	{"ConsoleWindowQt::Batch Pull v2", "batch_pull_v2", &st::menuIconGroups},
	{"ConsoleWindowQt::Group Manager", "group_manager", &st::menuIconGroups},
	{"ConsoleWindowQt::Workflow", "workflow", &st::menuIconChannel},
	{"ConsoleWindowQt::Auto Reply", "auto_reply", &st::menuIconSavedMessages},
	{"ConsoleWindowQt::Client CRM Profile", "portrait", &st::menuIconUserShow},
	{"ConsoleWindowQt::Account Hosting", "account_hosting", &st::menuIconChannel},
	{"ConsoleWindowQt::Multi-Account Manager", "account_manager", &st::menuIconUserShow},
	{"ConsoleWindowQt::Anti-Link Groups", "spam_group_manager", &st::menuIconGroups},
	{"ConsoleWindowQt::Avatar Cache", "avatar_cache", &st::menuIconSavedMessages},
	{"ConsoleWindowQt::Multi-Account Edit State", "multi_edit_state", &st::menuIconChannel},
	{"ConsoleWindowQt::Seat Sequence", "seat_sequence", &st::menuIconChannel},
	{"ConsoleWindowQt::Todo Queue", "todo_manager", &st::menuIconSavedMessages},
	{"ConsoleWindowQt::Pointcard Renewal", "agent_user_activate_pointcard", &st::menuIconChannel},
	{"ConsoleWindowQt::Notice Center", "notice_id", &st::menuIconSavedMessages},
	{"ConsoleWindowQt::Runtime Log", "console_log", &st::menuIconGroups},
};

void AddKeyedLabel(
		not_null<Ui::VerticalLayout*> parent,
		const char *key) {
	parent->add(object_ptr<Ui::FlatLabel>(
		parent.get(),
		rpl::single(OG::Tr::Lookup(key)),
		st::defaultFlatLabel));
}

QString FeatureToast(const FeatureRow &row) {
	return OG::Tr::Lookup(row.name)
		+ QString::fromLatin1(" / ")
		+ QString::fromUtf8(row.tech)
		+ QString::fromLatin1(" / ")
		+ OG::Tr::Lookup(kM2Key);
}

} // namespace

ConsoleWindowQt::ConsoleWindowQt(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: QDialog(parent)
, _controller(controller) {
	setWindowTitle(OG::Tr::Lookup(kTitleKey));
	// The reference window cannot be resized and its title bar carries a single
	// button, so the flags are spelled out: no context help question mark (a
	// QDialog adds it by default), and MSWindowsFixedSizeDialogHint instead of
	// relying on Qt to drop WS_THICKFRAME.
	setWindowFlags(
		Qt::Dialog
		| Qt::WindowTitleHint
		| Qt::WindowSystemMenuHint
		| Qt::WindowCloseButtonHint
		| Qt::MSWindowsFixedSizeDialogHint);
	setFixedSize(kWidth, kHeight);
	setupContent();
}

void ConsoleWindowQt::Show(
		not_null<Window::SessionController*> controller) {
	// One instance is kept alive while the window is only hidden, so that
	// Escape and a second menu click bring back the very same window. Closing
	// it deletes it and clears this pointer, which makes the next call open a
	// fresh window again.
	static auto instance = QPointer<ConsoleWindowQt>();
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
	const auto window = new ConsoleWindowQt(nullptr, controller);
	window->setAttribute(Qt::WA_DeleteOnClose);
	instance = window;
	window->show();
	window->raise();
	window->activateWindow();
}

void ConsoleWindowQt::reject() {
	// Escape hides the window instead of closing it: the reference client keeps
	// the same native window and reuses it, and only its X button destroys it.
	hide();
}

void ConsoleWindowQt::setupContent() {
	const auto scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);

	const auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(scroll);

	const auto content = Ui::CreateChild<Ui::VerticalLayout>(scroll);
	buildContent(content);
	content->resizeToWidth(kWidth);
	scroll->setWidget(content);
}

void ConsoleWindowQt::buildContent(
		not_null<Ui::VerticalLayout*> content) {
	Ui::AddSkip(content);

	auto license = object_ptr<Ui::TableLayout>(content.get());
	const auto licenseRaw = license.data();
	content->add(std::move(license), st::boxRowPadding);
	for (const auto &row : kLicenseRows) {
		licenseRaw->addRow(
			object_ptr<Ui::FlatLabel>(
				licenseRaw,
				rpl::single(OG::Tr::Lookup(row.key)),
				st::defaultFlatLabel),
			object_ptr<Ui::FlatLabel>(
				licenseRaw,
				rpl::single(OG::Tr::Lookup(row.value)),
				st::defaultFlatLabel));
	}

	Ui::AddSkip(content);
	AddKeyedLabel(content, kSeatHintKey);
	Ui::AddSkip(content);
	content->add(object_ptr<SeatGrid>(content.get()), st::boxRowPadding);

	Ui::AddSkip(content);
	Ui::AddDivider(content);
	Ui::AddSkip(content);

	auto main = object_ptr<Ui::TableLayout>(content.get());
	const auto mainRaw = main.data();
	content->add(std::move(main), st::boxRowPadding);
	auto left = object_ptr<Ui::VerticalLayout>(mainRaw);
	const auto leftRaw = left.data();
	auto right = object_ptr<Ui::VerticalLayout>(mainRaw);
	const auto rightRaw = right.data();
	mainRaw->addRow(std::move(left), std::move(right));

	AddKeyedLabel(leftRaw, kFeaturesKey);
	for (const auto &feature : kFeatures) {
		const auto button = Settings::AddButtonWithIcon(
			leftRaw,
			rpl::single(OG::Tr::Lookup(feature.name)),
			st::mainMenuButton,
			Settings::IconDescriptor{ .icon = feature.icon });
		button->setClickedCallback([=, text = FeatureToast(feature)] {
			_controller->window().showToast(text, kToastDuration);
		});
	}

	AddKeyedLabel(rightRaw, kActionsKey);
	Ui::AddSkip(rightRaw);
	AddKeyedLabel(rightRaw, kActionFieldsKey);
	Ui::AddSkip(rightRaw);
	const auto start = rightRaw->add(
		object_ptr<Ui::RoundButton>(
			rightRaw,
			rpl::single(OG::Tr::Lookup(kStartKey)),
			st::defaultLightButton),
		st::boxRowPadding);
	start->setClickedCallback([=] {
		_controller->window().showToast(
			OG::Tr::Lookup(kStartKey),
			kToastDuration);
	});
	const auto stop = rightRaw->add(
		object_ptr<Ui::RoundButton>(
			rightRaw,
			rpl::single(OG::Tr::Lookup(kStopKey)),
			st::defaultLightButton),
		st::boxRowPadding);
	stop->setClickedCallback([=] {
		_controller->window().showToast(
			OG::Tr::Lookup(kStopKey),
			kToastDuration);
	});

	Ui::AddSkip(content);
	const auto close = content->add(
		object_ptr<Ui::RoundButton>(
			content.get(),
			rpl::single(OG::Tr::Lookup(kCloseKey)),
			st::defaultLightButton),
		st::boxRowPadding);
	close->setClickedCallback([=] { this->close(); });
}

} // namespace Deep
