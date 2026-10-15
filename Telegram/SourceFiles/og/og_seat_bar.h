/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "og/og_seat_grid.h"
#include "ui/rp_widget.h"

namespace OG {

// Always-on seat strip of M2: the shared seat grid, docked above the main
// section of the main window. The reference draws no text in the strip, so it
// is the grid alone - the window's own buttons sit inside the same band.
class SeatBar final : public Ui::RpWidget {
public:
	explicit SeatBar(QWidget *parent);

	// No title line of its own: the strip is the grid band (6 rows of 27 plus
	// one 8px padding), which is also how the window's top-right buttons know
	// where to centre themselves.
	static constexpr auto kTitleHeight = 0;
	static constexpr auto kGridHeight = Deep::SeatGrid::kSeatRows
		* Deep::SeatGrid::kSeatRowHeight
		+ Deep::SeatGrid::kSeatPadding;
	static constexpr auto kHeight = kTitleHeight + kGridHeight;

	// The window's top-right buttons share this band and are centred in it.
	// Their geometry lives here so the grid's right end and the button row
	// cannot drift apart: two 80px buttons, the gap between them, the 40px to
	// the window edge and a small clearance for the grid.
	static constexpr auto kButtonWidth = 80;
	static constexpr auto kButtonHeight = 33;
	static constexpr auto kButtonSkip = 8;
	static constexpr auto kButtonRight = 40;
	static constexpr auto kGridClearance = 8;
	static constexpr auto kButtonsArea =
		kButtonWidth + kButtonSkip + kButtonWidth + kButtonRight
			+ kGridClearance;

protected:
	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *e) override;

private:
	Deep::SeatGrid *_grid = nullptr;

};

} // namespace OG
