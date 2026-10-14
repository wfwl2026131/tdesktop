/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_seat_grid.h"

#include "ui/painter.h"

#include <QtGui/QFontMetrics>

namespace Deep {
namespace {

// Below this cell width the 50 dots of the reference form would render as
// 2px specks, so the same kSeatTotal seats are reflowed into fewer columns
// instead of leaving the always-on strip blank.
constexpr auto kReflowCell = 10;
constexpr auto kReflowMinCols = 10;
constexpr auto kMinDot = 3;
constexpr auto kMaxDot = 9;
// The reference numbers every seat, so a label goes on each dot as soon as a
// three digit number fits inside one cell; kLabelSlack keeps neighbours apart.
constexpr auto kLabelPixelSize = 9;
constexpr auto kLabelSlack = 2;
// A reflowed row slot this tall still fits its row-first label; a shorter one
// would run the labels into the next row, so those dots stay plain.
constexpr auto kMinLabelRow = 24;

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

	// The reference draws 50 dots per row and numbers every seat (130, 131,
	// 132 ...), not only the first one of a row. In a narrow section that form
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
	font.setPixelSize(kLabelPixelSize);
	// Measured at paint time instead of hard-coded, so the gate keeps telling
	// the truth for whichever font the platform picks at this pixel size.
	const auto labelWidth = QFontMetrics(font).horizontalAdvance(
		QStringLiteral("300"));
	const auto everySeat = (cell >= labelWidth + kLabelSlack);
	// The reflow always lands on cell ~= kReflowCell, so it never reaches
	// everySeat; there the row-first label is the fallback, and the tightest
	// reflow drops the labels altogether rather than overlapping them.
	const auto rowFirst = !everySeat && (rowHeight >= kMinLabelRow);

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
		if (!everySeat && (!rowFirst || col != 0)) {
			continue;
		}
		p.setPen(QColor(120, 125, 130));
		p.setFont(font);
		// An every-seat label sits centred just above its own dot; a row-first
		// one stays at the left edge of its row.
		const auto left = everySeat
			? (x + (d - labelWidth) / 2)
			: (kSeatPadding + col * cell);
		const auto baseline = everySeat
			? (y - kLabelSlack)
			: (reflowed
				? (y + d)
				: (kSeatPadding + row * rowHeight + 8));
		p.drawText(left, baseline, QString::number(i + 1));
	}
}

} // namespace Deep
