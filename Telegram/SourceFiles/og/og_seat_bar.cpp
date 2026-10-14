/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_seat_bar.h"

#include "base/object_ptr.h"
#include "og/og_tr.h"
#include <rpl/producer.h>
#include "ui/qt_object_factory.h"
#include "ui/rp_widget.h"
#include "ui/widgets/labels.h"
#include "styles/style_widgets.h"

namespace OG {
namespace {

constexpr auto kPadding = 8;

// Keys only - the text lives in the Deep translation data, with the English
// source text below as the last fallback.
constexpr auto kTitleKey = "SeatBar::Seats";

} // namespace

SeatBar::SeatBar(QWidget *parent)
: Ui::RpWidget(parent) {
	_title = Ui::CreateChild<Ui::FlatLabel>(
		this,
		rpl::single(Tr::Lookup(kTitleKey)),
		st::defaultFlatLabel);
	_grid = Ui::CreateChild<Deep::SeatGrid>(this);
}

void SeatBar::resizeEvent(QResizeEvent *e) {
	Ui::RpWidget::resizeEvent(e);

	if (_title) {
		_title->setGeometry(kPadding, 0, width() - 2 * kPadding, kTitleHeight);
	}
	if (_grid) {
		_grid->setGeometry(0, kTitleHeight, width(), kGridHeight);
	}
}

} // namespace OG
