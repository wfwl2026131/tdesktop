/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/object_ptr.h"
#include "base/required.h"
#include "ui/rp_widget.h"

#include <vector>

namespace Ui {
class RoundButton;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Deep {

// Right-side toolbar of the reference client, reproduced as a real widget
// class for our fork.
//
// Evidence: `Deep::RightToolBar` inherits `Ui::RpWidget` in the reference
// binary, so our class does the same instead of reusing a plain box. Its four
// tabs are Unread Messages / Quick Reply / Translation / Client List, and the
// client list owns a disabled state.
//
// M1 keeps the tabs static: clicking one only reports what M2 will wire to
// live session data. The first filter row under them only switches its own
// highlight. The second row maps "All"/"Private"/"Group" onto the folders the
// account already has - it never creates one - and "New clients" is a
// placeholder that moves nothing but the highlight. The read actions and
// docking the panel into the main window arrive with M2 / M1.5b.
class RightToolBar final : public Ui::RpWidget {
public:
	RightToolBar(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	// M1 skeleton size; the dock layout of M1.5b replaces it.
	static constexpr auto kWidth = 300;
	static constexpr auto kHeight = 420;

protected:
	void resizeEvent(QResizeEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void paintEvent(QPaintEvent *e) override;

private:
	void setupTabs();
	void setupActions();
	void setupUnreadCount();
	void setupFilter();

	// Which segment of the second filter row is active, read from the native
	// chats filter: 0..2 for "All"/"Private"/"Group", 3 for the "New clients"
	// placeholder, -1 when the native filter is a folder this row does not
	// stand for. Nothing about those segments is stored (see the .cpp).
	[[nodiscard]] int row3Selected() const;
	void applyRow3Filter(int segment);

	const not_null<Window::SessionController*> _controller;
	std::vector<not_null<Ui::RoundButton*>> _tabs;
	std::vector<not_null<Ui::RoundButton*>> _actions;

	// Selection of the first hand painted filter row. Index 0 is "Unread", as
	// in the reference: this row only switches its own highlight.
	int _row2Selected = 0;

	// The one piece of state the second row keeps: the "New clients"
	// placeholder is the only segment that does not move the native filter, so
	// it is the only one that cannot be derived from it. setupFilter() clears
	// it on any native filter emission, which is what keeps the two in step.
	bool _row3NewSelected = false;

	// Unread total of the current account, painted as a badge after the
	// "Unread" label of the first filter row. Read-only: it comes from the
	// session's own account badge and is refreshed by that badge's change
	// signal.
	int _unreadCount = 0;
	rpl::lifetime _lifetime;
};

} // namespace Deep
