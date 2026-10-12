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

// Static mock of the Officegram side toolbar (M1.5a): the four tabs
// (unread / quick reply / translation / clients), the filter row and the
// read-action buttons. M2 wires each tab to live session data.
void FillToolbarBox(not_null<Ui::GenericBox*> box);

} // namespace OG
