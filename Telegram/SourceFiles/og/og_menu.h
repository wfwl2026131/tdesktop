/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/required.h"

namespace Ui {
class VerticalLayout;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace OG {

// Appends the Officegram CRM section to the slide-out main menu
// (Window::MainMenu). Keep this the only entry point from official files:
// one call + one include in window_main_menu.cpp, nothing else.
void SetupMenuRows(
	not_null<Ui::VerticalLayout*> menu,
	not_null<Window::SessionController*> controller);

} // namespace OG
