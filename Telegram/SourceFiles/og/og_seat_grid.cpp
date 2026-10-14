/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_seat_grid.h"

#include "ui/painter.h"

namespace Deep {

SeatGrid::SeatGrid(QWidget *parent)
: Ui::RpWidget(parent) {
}

int SeatGrid::heightForWidth(int) const {
	return kSeatRows * kSeatRowHeight + kSeatPadding;
}

void SeatGrid::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	const auto cell = (width() - 2 * kSeatPadding) / kSeatCols;
	if (cell < 4) {
		return;
	}
	const auto d = (cell - 4 < 9) ? (cell - 4) : 9;
	auto font = QFont();
	font.setPixelSize(9);
	for (auto i = 0; i != kSeatTotal; ++i) {
		const auto col = i % kSeatCols;
		const auto row = i / kSeatCols;
		const auto x = kSeatPadding + col * cell + (cell - d) / 2;
		const auto y = kSeatPadding + row * kSeatRowHeight + 10;
		auto color = QColor(165, 170, 175);
		if (i < 24) {
			color = QColor(60, 174, 96);
		} else if (i >= 200 && i < 204) {
			color = QColor(220, 80, 80);
		}
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		p.drawEllipse(x, y, d, d);
		if ((i % kSeatCols) == 0) {
			p.setPen(QColor(120, 125, 130));
			p.setFont(font);
			p.drawText(
				kSeatPadding + col * cell,
				kSeatPadding + row * kSeatRowHeight + 8,
				QString::number(i + 1));
		}
	}
}

} // namespace Deep
