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
class GenericBox;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace OG {

// Static mock of the Officegram console window (M1.5a): license table,
// 300-seat dot grid, 22-item feature list and the action area placeholder.
// M2 replaces the mock rows with live controls wired to ogweb routes.
void FillConsoleBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller);

} // namespace OG
