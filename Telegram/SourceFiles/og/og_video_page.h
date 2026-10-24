/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/required.h"
#include "og/og_video.h"

#include <QtWidgets/QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;

namespace Window {
class SessionController;
} // namespace Window

namespace OG {

// The group video page, read against the reference window line by line: a
// group address, the request interval, a tick per pool account, and the two
// buttons the reference shows. What the buttons do here is decide what the
// request would be and say so; the session that would send it belongs to a
// later milestone, which is the same split the auto-reply page was built on.
class VideoPageWindow final : public QDialog {
public:
	VideoPageWindow(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		bool embedded = false);

	static void Show(not_null<Window::SessionController*> controller);

	static constexpr auto kWidth = 720;
	static constexpr auto kHeight = 520;

	// Escape hides instead of closing, the contract every page of this fork
	// keeps: the half typed address survives and Show() brings it back.
	void reject() override;

private:
	void setupContent();
	void rebuild();
	void reloadAccounts();
	void save();
	void act(GroupCallAction action);

	QLineEdit *_link = nullptr;
	QSpinBox *_interval = nullptr;
	QVBoxLayout *_accounts = nullptr;
	QWidget *_accountsHost = nullptr;
	QLabel *_status = nullptr;
	QPushButton *_join = nullptr;
	QPushButton *_leave = nullptr;
};

} // namespace OG
