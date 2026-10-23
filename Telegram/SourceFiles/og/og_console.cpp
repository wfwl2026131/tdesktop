/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_console.h"

#include "base/object_ptr.h"
#include "og/og_portrait_page.h"
#include "og/og_seat_grid.h"
#include "og/og_todo_page.h"
#include "og/og_tr.h"
#include <rpl/producer.h>
#include "settings/settings_common.h"
#include "ui/qt_object_factory.h"
#include "ui/rp_widget.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "ui/wrap/table_layout.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_window.h"

#include <QtCore/QPointer>
#include <QtWidgets/QVBoxLayout>

namespace Deep {
namespace {

constexpr auto kToastDuration = 1500;

// A toast opened through the controller is parented to the main window body,
// where it stays invisible while this top-level window is the active one.
// Show it on the console window itself: same API, explicit parent, default
// placement, so it is centred in this window. Pages that toast follow the same
// rule and pass their own instance as the parent. The weak pointer keeps only
// one toast alive: the next one takes the previous one down first, the way the
// window-level helper does, instead of stacking two identical rectangles.
void ShowOwnToast(
		not_null<QWidget*> window,
		base::weak_ptr<Ui::Toast::Instance> &last,
		const QString &text) {
	if (const auto strong = last.get()) {
		strong->hideAnimated();
	}
	last = Ui::Toast::Show(window, Ui::Toast::Config{
		.text = TextWithEntities{ text },
		.duration = kToastDuration,
	});
}

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
, _controller(controller)
, _scrollStyle([] {
	auto result = std::make_unique<style::ScrollArea>(st::defaultScrollArea);
	// The reference console keeps its bar on screen and never fades it out,
	// while st::defaultScrollArea starts a hide timer after every scroll. A
	// zero interval turns that timer off for this window only; the bar still
	// hides itself whenever the content fits, which is what updateBar() does
	// independently of the interval.
	result->hiding = 0;
	return result;
}()) {
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

ConsoleWindowQt::~ConsoleWindowQt() = default;

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
	// Ui::VerticalLayout has no size hint at all: its height comes out of
	// resizeGetHeight(), which only runs when a parent passes a width in. A
	// bare QScrollArea with setWidgetResizable(true) sizes its content from
	// sizeHint() instead, so the layout was squashed to the viewport, the rows
	// past its bottom edge were clipped and the scroll range stayed zero.
	// Ui::ScrollArea keeps the widget at the size the widget itself sets.
	const auto scroll = Ui::CreateChild<Ui::ScrollArea>(this, *_scrollStyle);

	auto content = object_ptr<Ui::VerticalLayout>(scroll);
	const auto contentRaw = content.data();
	buildContent(contentRaw);

	// The width is applied before the ownership moves into the scroll area:
	// setWidget() resizes a widget that was never resized to its size hint,
	// and this one only has a size once resizeToWidth() has run.
	contentRaw->resizeToWidth(kWidth);
	scroll->setOwnedWidget(std::move(content));

	const auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(scroll);
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
		const auto tech = QString::fromUtf8(feature.tech);
		// Two rows already own a window of their own on the reference side,
		// so their callbacks open it instead of reporting a placeholder.
		// TODO: the final entry point of both pages (a right toolbar tab or a
		// menu item) still waits for evidence from the reference client; this
		// only makes the pages reachable. The other rows and the console
		// layout stay exactly as they are.
		if (tech == QString::fromLatin1("portrait")) {
			button->setClickedCallback([=] {
				OG::PortraitPageWindow::Show(_controller);
			});
		} else if (tech == QString::fromLatin1("todo_manager")) {
			button->setClickedCallback([=] {
				OG::TodoPageWindow::Show(_controller);
			});
		} else {
			button->setClickedCallback([=, text = FeatureToast(feature)] {
				ShowOwnToast(this, _lastToast, text);
			});
		}
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
		ShowOwnToast(this, _lastToast, OG::Tr::Lookup(kStartKey));
	});
	const auto stop = rightRaw->add(
		object_ptr<Ui::RoundButton>(
			rightRaw,
			rpl::single(OG::Tr::Lookup(kStopKey)),
			st::defaultLightButton),
		st::boxRowPadding);
	stop->setClickedCallback([=] {
		ShowOwnToast(this, _lastToast, OG::Tr::Lookup(kStopKey));
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
