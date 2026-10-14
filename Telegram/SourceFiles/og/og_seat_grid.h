/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/rp_widget.h"

namespace Deep {

// The 1-300 seat grid of the reference client: 50 dots per row, 6 rows.
// M1 drew it inside the console window; M2 hoists it out so the always-on
// seat bar and the console window share one implementation.
class SeatGrid final : public Ui::RpWidget {
public:
	explicit SeatGrid(QWidget *parent);

	static constexpr auto kSeatCols = 50;
	static constexpr auto kSeatRows = 6;
	static constexpr auto kSeatTotal = kSeatCols * kSeatRows;
	static constexpr auto kSeatRowHeight = 34;
	static constexpr auto kSeatPadding = 8;

	[[nodiscard]] int heightForWidth(int) const override;

protected:
	void paintEvent(QPaintEvent *e) override;

};

} // namespace Deep
