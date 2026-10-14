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

#include <vector>

namespace Ui {
class FlatLabel;
class RoundButton;
class RpWidget;
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
// tabs are Unread Messages / Translation / Client List / QuickReply, and the
// client list owns a disabled state.
//
// M1 keeps the tabs static: clicking one only reports what M2 will wire to
// live session data. Tab pages, the filter row and the real read actions
// arrive with M2; docking it into the main window equals M1.5b.
class RightToolBar final : public Ui::RpWidget {
public:
	RightToolBar(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	// M1 skeleton size; the dock layout of M1.5b replaces it.
	static constexpr auto kWidth = 380;
	static constexpr auto kHeight = 420;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	void setupTabs();
	void setupHint();
	void setupActions();

	const not_null<Window::SessionController*> _controller;
	std::vector<not_null<Ui::RoundButton*>> _tabs;
	Ui::FlatLabel *_hint = nullptr;
	std::vector<not_null<Ui::RoundButton*>> _actions;
};

} // namespace Deep
