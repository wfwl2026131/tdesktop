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

class QVBoxLayout;
class QWidget;

namespace Window {
class SessionController;
} // namespace Window

namespace OG {

// The CRM profile of the reference client, as a window of our own.
//
// Left column: the client rows of the store. Right column: the portrait that
// matches the selected client by platform and key (the seat is preferred when
// both sides carry one), rendered group by group. The eight reserve columns
// are the only ones a portrait page does not show - they are not interface
// data yet and the reference keeps no meaning in them.
//
// The window is entered from the console for now; whether the reference opens
// it from a right toolbar tab is still an open question, so nothing here
// touches the tab row.
class PortraitPageWindow final : public QDialog {
public:
	PortraitPageWindow(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	// Escape only hides the window, the next call reuses the instance; the
	// title bar button destroys it, exactly like the console window.
	static void Show(not_null<Window::SessionController*> controller);

	static constexpr auto kWidth = 1000;
	static constexpr auto kHeight = 680;
	static constexpr auto kClientColumnWidth = 280;

private:
	void setupContent();
	void rebuildClients();
	void showPortrait();

	void reject() override;

	qint64 _selected = 0;
	QVBoxLayout *_clientRows = nullptr;
	QWidget *_clientHost = nullptr;
	QVBoxLayout *_details = nullptr;
	QWidget *_detailsHost = nullptr;
};

} // namespace OG
