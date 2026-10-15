/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_seat_bar.h"

#include "ui/painter.h"
#include "ui/qt_object_factory.h"
#include "ui/rp_widget.h"
#include "styles/palette.h"
#include "styles/style_basic.h"

namespace OG {

SeatBar::SeatBar(QWidget *parent)
: Ui::RpWidget(parent) {
	_grid = Ui::CreateChild<Deep::SeatGrid>(this);
}

void SeatBar::resizeEvent(QResizeEvent *e) {
	Ui::RpWidget::resizeEvent(e);

	if (_grid) {
		// The window's own buttons share this band on the right, so the grid
		// stops clear of them instead of running the full window width.
		_grid->setGeometry(
			0,
			kTitleHeight,
			width() - kButtonsArea,
			kGridHeight);
	}
}

void SeatBar::paintEvent(QPaintEvent *e) {
	// Without this the strip stays unpainted: the main window sets
	// Qt::WA_OpaquePaintEvent, so nothing behind it fills the band.
	auto p = Painter(this);
	p.fillRect(rect(), st::windowBg);
}

} // namespace OG
