/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/required.h"

#include <QtWidgets/QDialog>

namespace Ui {
class VerticalLayout;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace Deep {

// Console window of the reference client, reproduced for our fork.
//
// Evidence: `Deep::ConsoleWindowQt` inherits QDialog in the reference binary,
// so this is a real top-level window and not a box layer. M1 keeps its seat
// grid, licence table and feature column static; only the opening/closing
// behaviour and the window identity are part of the milestone.
class ConsoleWindowQt final : public QDialog {
public:
	ConsoleWindowQt(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	// Creates the window, shows it and brings it to the front. The window
	// deletes itself on close, so a second call always opens a fresh one.
	static void Show(not_null<Window::SessionController*> controller);

	static constexpr auto kWidth = 700;
	static constexpr auto kHeight = 620;

private:
	void setupContent();
	void buildContent(not_null<Ui::VerticalLayout*> content);

	const not_null<Window::SessionController*> _controller;
};

} // namespace Deep
