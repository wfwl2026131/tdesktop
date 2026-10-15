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
// The reference draws each seat number inside its own dot in white, so the
// label takes the largest pixel size whose three digits still fit the dot.
constexpr auto kLabelMaxPixelSize = 11;
constexpr auto kLabelMinPixelSize = 6;
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
	// The reference dot is 24x24. A narrower cell shrinks it so neighbours never
	// touch, which keeps the 4px gap the reference has at its own pitch.
	auto d = (cell - 4 < kSeatDot) ? (cell - 4) : kSeatDot;
	if (cell < kReflowCell) {
		cols = Clamp(available / kReflowCell, kReflowMinCols, kSeatCols);
		cell = available / cols;
		const auto rows = (kSeatTotal + cols - 1) / cols;
		rowHeight = inner / rows;
		d = Clamp(((cell < rowHeight) ? cell : rowHeight) - 3, kMinDot, kMaxDot);
	}
	const auto reflowed = (cols != kSeatCols);

	// The number is drawn inside its own dot in white, so the size is the
	// largest one whose "300" still fits the dot that was just measured.
	// Measuring at paint time keeps the gate honest for whichever font the
	// platform picks at that pixel size.
	auto font = QFont();
	auto labelWidth = 0;
	for (auto size = kLabelMaxPixelSize; size >= kLabelMinPixelSize; --size) {
		font.setPixelSize(size);
		labelWidth = QFontMetrics(font).horizontalAdvance(
			QStringLiteral("300"));
		if (labelWidth + kLabelSlack <= d) {
			break;
		}
	}
	const auto metrics = QFontMetrics(font);
	const auto everySeat = (labelWidth + kLabelSlack <= d);
	// The reflow always lands on a dot far smaller than any three digit label,
	// so there the row-first label is the fallback, and the tightest reflow
	// drops the labels altogether rather than overlapping them.
	const auto rowFirst = !everySeat && (rowHeight >= kMinLabelRow);

	for (auto i = 0; i != kSeatTotal; ++i) {
		const auto col = i % cols;
		const auto row = i / cols;
		const auto x = kSeatPadding + col * cell + (cell - d) / 2;
		const auto y = kSeatPadding + row * rowHeight + (rowHeight - d) / 2;
		auto color = QColor(204, 204, 204);
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
		p.setFont(font);
		if (everySeat) {
			// White and centred inside the dot - the reference form.
			p.setPen(QColor(255, 255, 255));
			p.drawText(
				x + (d - labelWidth) / 2,
				y + (d + metrics.ascent() - metrics.descent()) / 2,
				QString::number(i + 1));
		} else {
			// Row-first fallback of a narrow strip: no room inside the dot.
			p.setPen(QColor(120, 125, 130));
			p.drawText(
				kSeatPadding + col * cell,
				reflowed ? (y + d) : (kSeatPadding + row * rowHeight + 8),
				QString::number(i + 1));
		}
	}
}

} // namespace Deep
