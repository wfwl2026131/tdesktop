/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_menu.h"

#include "base/object_ptr.h"
#include "og/og_console.h"
#include "og/og_right_toolbar.h"
#include "og/og_tr.h"
#include <rpl/producer.h>
#include "settings/settings_common.h"
#include "ui/layers/box_content.h"
#include "ui/layers/generic_box.h"
#include "ui/layers/show.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/shadow.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_window.h"
#include "styles/style_window_main_menu.h"

namespace OG {
namespace {

constexpr auto kM2ToastDuration = 1500;

void AddM2Row(
		not_null<Ui::VerticalLayout*> menu,
		const char *key,
		const style::icon *icon,
		not_null<Window::SessionController*> controller) {
	const auto row = Settings::AddButtonWithIcon(
		menu,
		rpl::single(Tr::Lookup(key)),
		st::mainMenuButton,
		Settings::IconDescriptor{ .icon = icon });
	row->setClickedCallback([=] {
		controller->window().showToast(
			Tr::Lookup("MainMenu::M2 placeholder"),
			kM2ToastDuration);
	});
}

void ShowToolBarBox(not_null<Window::SessionController*> controller) {
	controller->uiShow()->showBox(Box([=](not_null<Ui::GenericBox*> box) {
		box->setWidth(
			Deep::RightToolBar::kWidth + 2 * st::boxRowPadding.left());
		box->setTitle(rpl::single(Tr::Lookup("RightToolBar::Title")));
		box->verticalLayout()->add(
			object_ptr<Deep::RightToolBar>(box, controller),
			st::boxRowPadding);
		box->addButton(
			rpl::single(Tr::Lookup("Common::Close")),
			[=] { box->closeBox(); });
	}));
}

} // namespace

void SetupMenuRows(
		not_null<Ui::VerticalLayout*> menu,
		not_null<Window::SessionController*> controller) {
	// Deep translation data is loaded once, before the first Lookup().
	Tr::Start();

	menu->add(
		object_ptr<Ui::PlainShadow>(menu),
		{ 0, st::mainMenuSkip, 0, st::mainMenuSkip });

	const auto console = Settings::AddButtonWithIcon(
		menu,
		rpl::single(Tr::Lookup("MainMenu::Console")),
		st::mainMenuButton,
		Settings::IconDescriptor{ .icon = &st::menuIconGroups });
	console->setClickedCallback([=] {
		Deep::ConsoleWindowQt::Show(controller);
	});

	const auto toolbar = Settings::AddButtonWithIcon(
		menu,
		rpl::single(Tr::Lookup("MainMenu::Toolbar")),
		st::mainMenuButton,
		Settings::IconDescriptor{ .icon = &st::menuIconSavedMessages });
	toolbar->setClickedCallback([=] {
		ShowToolBarBox(controller);
	});

	AddM2Row(menu, "MainMenu::Batch Login", &st::menuIconUserShow, controller);
	AddM2Row(menu, "MainMenu::AI Tools", &st::menuIconSavedMessages, controller);
	AddM2Row(menu, "MainMenu::Seats And License", &st::menuIconChannel, controller);
}

} // namespace OG
