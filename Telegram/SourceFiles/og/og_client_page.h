/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/required.h"
#include "og/og_store.h"

#include <QtWidgets/QDialog>

class QGridLayout;
class QLabel;
class QScrollArea;
class QWidget;

namespace Window {
class SessionController;
} // namespace Window

namespace OG {

// The client list of the reference client, as a window of our own.
//
// One row per `client` record, showing eight of its twenty-five columns:
// platform, username, phone, nickname, state, sent, recv and add count. The
// reference puts two clickable counters on the row (success / failed) that
// are only half evidenced, so this milestone renders both as plain read-only
// cells and performs no click behavior.
//
// Read only: the page never writes to the store, no cell can be edited and
// nothing reacts to a click. It is entered from nowhere yet - the right
// toolbar row and the console stay untouched until the entry point has
// evidence.
class ClientPageWindow final : public QDialog {
public:
	ClientPageWindow(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	// Escape only hides the window, the next call reuses the instance; the
	// title bar button destroys it, exactly like the console window.
	static void Show(not_null<Window::SessionController*> controller);

	static constexpr auto kWidth = 900;
	static constexpr auto kHeight = 620;

private:
	void setupContent();
	void rebuild();

	void reject() override;

	QLabel *_empty = nullptr;
	QScrollArea *_scroll = nullptr;
	QWidget *_host = nullptr;
	QGridLayout *_grid = nullptr;
};

} // namespace OG
