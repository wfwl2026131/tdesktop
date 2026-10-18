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
// live session data. The two filter rows under them are painted by hand and
// only switch their own selection; wiring them to live data and the read
// actions arrive with M2, and docking the panel into the main window equals
// M1.5b.
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

	const not_null<Window::SessionController*> _controller;
	std::vector<not_null<Ui::RoundButton*>> _tabs;
	std::vector<not_null<Ui::RoundButton*>> _actions;

	// Selection of the two hand painted filter rows. Index 0 is "Unread" on
	// the first row and "All" on the second one, as in the reference.
	int _row2Selected = 0;
	int _row3Selected = 0;

	// Unread total of the current account, painted as a badge after the
	// "Unread" label of the first filter row. Read-only: it comes from the
	// session's own account badge and is refreshed by that badge's change
	// signal.
	int _unreadCount = 0;
	rpl::lifetime _lifetime;
};

} // namespace Deep
