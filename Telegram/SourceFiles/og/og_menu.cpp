/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_menu.h"

#include "base/object_ptr.h"
#include "og/og_panel.h"
#include "rpl/single.h"
#include "settings/settings_common.h"
#include "ui/layers/box_content.h"
#include "ui/layers/generic_box.h"
#include "ui/layers/show.h"
#include "ui/vertical_list.h"
#include "ui/widgets/shadow.h"
#include "window/window_session_controller.h"
#include "styles/style_menu_icons.h"
#include "styles/style_window_main_menu.h"

namespace OG {
namespace {

constexpr auto kM2ToastDuration = 1500;

void AddM2Row(
		not_null<Ui::VerticalLayout*> menu,
		const char *text,
		const style::icon *icon,
		not_null<Window::SessionController*> controller) {
	const auto row = Settings::AddButtonWithIcon(
		menu,
		rpl::single(QString::fromUtf8(text)),
		st::mainMenuButton,
		Settings::IconDescriptor{ .icon = icon });
	row->setClickedCallback([=] {
		controller->window().showToast(
			QString::fromUtf8("M2 \351\207\214\347\250\213\347\242\221\346\216\245\351\200\232 ogweb \345\220\216\345\256\236\350\243\205"),
			kM2ToastDuration);
	});
}

} // namespace

void SetupMenuRows(
		not_null<Ui::VerticalLayout*> menu,
		not_null<Window::SessionController*> controller) {
	menu->add(
		object_ptr<Ui::PlainShadow>(menu),
		{ 0, st::mainMenuSkip, 0, st::mainMenuSkip });

	const auto workbench = Settings::AddButtonWithIcon(
		menu,
		rpl::single(QString::fromUtf8("\350\220\245\351\224\200\345\267\245\344\275\234\345\217\260")),
		st::mainMenuButton,
		Settings::IconDescriptor{ .icon = &st::menuIconGroups });
	workbench->setClickedCallback([=] {
		controller->uiShow()->showBox(Box(OG::FillWorkbenchBox));
	});

	AddM2Row(menu, "\346\211\271\351\207\217\344\270\212\345\217\267", &st::menuIconUserShow, controller);
	AddM2Row(menu, "AI \345\267\245\345\205\267\351\233\206", &st::menuIconSavedMessages, controller);
	AddM2Row(menu, "\345\270\255\344\275\215\344\270\216\346\216\210\346\235\203", &st::menuIconChannel, controller);
}

} // namespace OG
