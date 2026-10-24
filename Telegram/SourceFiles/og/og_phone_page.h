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

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;
class QWidget;

namespace Window {
class SessionController;
} // namespace Window

namespace OG {

// The reference's Filter Phone window: a source (a file or a range under a
// country's calling code), the two intervals, a preview of what the source
// produced, and the counters. The strings are the reference's own, taken from
// the binary; the Chinese in the .ts is provisional until a screenshot of the
// reference page confirms it.
//
// The map of the reference is not reproduced: it resolves a country from a
// click, which needs a tile server and a country table, and neither exists in
// this fork yet. That is a gap written down, not a feature hidden.
class PhonePageWindow final : public QDialog {
public:
	PhonePageWindow(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		bool embedded = false);

	static void Show(not_null<Window::SessionController*> controller);

	static constexpr auto kWidth = 720;
	static constexpr auto kHeight = 560;

	void reject() override;

private:
	void setupContent();
	void rebuild();
	void refresh();
	void save();

	QLineEdit *_callingCode = nullptr;
	QSpinBox *_rangeStart = nullptr;
	QSpinBox *_rangeEnd = nullptr;
	QSpinBox *_globalInterval = nullptr;
	QSpinBox *_singleInterval = nullptr;
	QLabel *_preview = nullptr;
	QLabel *_counters = nullptr;
	QLabel *_status = nullptr;
	QPushButton *_start = nullptr;
};

} // namespace OG
