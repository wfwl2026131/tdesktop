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

namespace Ui {
class FlatLabel;
} // namespace Ui

namespace OG {

// Always-on seat strip of M2: a title line plus the shared seat grid, docked
// above the main section of the main window.
class SeatBar final : public Ui::RpWidget {
public:
	explicit SeatBar(QWidget *parent);

	static constexpr auto kTitleHeight = 20;
	static constexpr auto kGridHeight = Deep::SeatGrid::kSeatRows
		* Deep::SeatGrid::kSeatRowHeight
		+ Deep::SeatGrid::kSeatPadding;
	static constexpr auto kHeight = kTitleHeight + kGridHeight;

protected:
	void resizeEvent(QResizeEvent *e) override;

private:
	Deep::SeatGrid *_grid = nullptr;
	Ui::FlatLabel *_title = nullptr;

};

} // namespace OG
