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

class QPushButton;
class QVBoxLayout;
class QWidget;

namespace Window {
class SessionController;
} // namespace Window

namespace OG {

// The todo queue of the reference client, as a window of our own.
//
// Layout: the three status tabs of the reference query (1 pending, 2 done,
// 3 archived) over the rows of one status, ordered by the store exactly like
// the reference SELECT orders them. Every row is rebuilt from the store after
// an action, so the view can never drift from the data.
//
// The window is entered from the console for now; whether the reference
// reaches it from a right toolbar tab is still an open question, so nothing
// here touches the tab row.
class TodoPageWindow final : public QDialog {
public:
	TodoPageWindow(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	// Escape only hides the window, the next call reuses the instance; the
	// title bar button destroys it, exactly like the console window.
	static void Show(not_null<Window::SessionController*> controller);

	static constexpr auto kWidth = 900;
	static constexpr auto kHeight = 640;

private:
	void setupContent();
	void rebuild();

	void addTodoRow(const TodoEntry &entry);
	void addArchiveRow(const TodoArchive &entry);

	void reject() override;

	TodoStatus _status = TodoStatus::Pending;
	QVBoxLayout *_rows = nullptr;
	QWidget *_rowsHost = nullptr;
	QPushButton *_tabPending = nullptr;
	QPushButton *_tabDone = nullptr;
	QPushButton *_tabArchived = nullptr;
};

} // namespace OG
