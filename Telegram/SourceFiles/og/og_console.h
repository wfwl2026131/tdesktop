/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/required.h"
#include "base/weak_ptr.h"

#include <QtWidgets/QDialog>

#include <memory>

namespace style {
struct ScrollArea;
} // namespace style

namespace Ui {

class VerticalLayout;

namespace Toast {
class Instance;
} // namespace Toast

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

	// Defined in the source file, where the scroll area style is complete.
	~ConsoleWindowQt() override;

	// Shows the window and brings it to the front. Escape only hides it and the
	// next call reuses the same instance; closing it (the title bar button or
	// the Close button inside) destroys it and the call after that opens a
	// fresh one.
	static void Show(not_null<Window::SessionController*> controller);

	// Client area of the reference window (Windows: 1210x770 measured with
	// GetClientRect, while the whole window is 1223x806 because the 29 logical
	// px title bar sits outside of it).
	static constexpr auto kWidth = 1210;
	static constexpr auto kHeight = 770;

private:
	void setupContent();
	void buildContent(not_null<Ui::VerticalLayout*> content);

	// Escape hides the window instead of closing it, so that the instance
	// survives for the next Show() to reuse.
	void reject() override;

	const not_null<Window::SessionController*> _controller;

	// The scroll area keeps a reference to the style it was built with, so the
	// window owns the copy instead of handing it a temporary.
	const std::unique_ptr<style::ScrollArea> _scrollStyle;

	// Only one toast is meant to be on screen at a time, so the next one takes
	// the previous one down first, the way the window-level helper does.
	base::weak_ptr<Ui::Toast::Instance> _lastToast;
};

} // namespace Deep
