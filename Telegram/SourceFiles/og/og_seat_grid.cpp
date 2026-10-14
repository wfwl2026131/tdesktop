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
namespace {

// Below this cell width the 50 dots of the reference form would render as
// 2px specks, so the same kSeatTotal seats are reflowed into fewer columns
// instead of leaving the always-on strip blank.
constexpr auto kReflowCell = 10;
constexpr auto kReflowMinCols = 10;
constexpr auto kMinDot = 3;
constexpr auto kMaxDot = 9;

[[nodiscard]] int Clamp(int value, int low, int high) {
	return (value < low) ? low : ((value > high) ? high : value);
}

} // namespace

SeatGrid::SeatGrid(QWidget *parent)
: Ui::RpWidget(parent) {
}

int SeatGrid::heightForWidth(int) const {
	return kSeatRows * kSeatRowHeight + kSeatPadding;
}

void SeatGrid::paintEvent(QPaintEvent *e) {
	auto p = Painter(this);
	const auto available = width() - 2 * kSeatPadding;
	const auto inner = height() - 2 * kSeatPadding;
	if (available <= 0 || inner <= 0) {
		return;
	}

	// The reference draws 50 dots per row. In a narrow main section that form
	// is unreadable, so all kSeatTotal seats are kept but reflowed into fewer,
	// wider columns - the strip must never turn into an empty block.
	auto cols = kSeatCols;
	auto cell = available / cols;
	auto rowHeight = kSeatRowHeight;
	auto d = (cell - 4 < kMaxDot) ? (cell - 4) : kMaxDot;
	if (cell < kReflowCell) {
		cols = Clamp(available / kReflowCell, kReflowMinCols, kSeatCols);
		cell = available / cols;
		const auto rows = (kSeatTotal + cols - 1) / cols;
		rowHeight = inner / rows;
		d = Clamp(((cell < rowHeight) ? cell : rowHeight) - 3, kMinDot, kMaxDot);
	}
	const auto reflowed = (cols != kSeatCols);

	auto font = QFont();
	font.setPixelSize(9);
	for (auto i = 0; i != kSeatTotal; ++i) {
		const auto col = i % cols;
		const auto row = i / cols;
		const auto x = kSeatPadding + col * cell + (cell - d) / 2;
		const auto y = reflowed
			? (kSeatPadding + row * rowHeight + (rowHeight - d) / 2)
			: (kSeatPadding + row * rowHeight + 10);
		auto color = QColor(165, 170, 175);
		if (i < 24) {
			color = QColor(60, 174, 96);
		} else if (i >= 200 && i < 204) {
			color = QColor(220, 80, 80);
		}
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		p.drawEllipse(x, y, d, d);
		if (col == 0) {
			p.setPen(QColor(120, 125, 130));
			p.setFont(font);
			p.drawText(
				kSeatPadding + col * cell,
				reflowed ? (y + d) : (kSeatPadding + row * rowHeight + 8),
				QString::number(i + 1));
		}
	}
}

} // namespace Deep
