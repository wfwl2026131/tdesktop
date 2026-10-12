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

namespace OG {

// M1 skeleton of the marketing workbench panel (opened from the main menu).
// M2 replaces the static rows with live ogweb data (serverBase / _ff
// heartbeat / get-account-info seats).
void FillWorkbenchBox(not_null<Ui::GenericBox*> box);

} // namespace OG
