/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_right_toolbar.h"

#include "base/object_ptr.h"
#include "data/data_session.h"
#include "main/main_session.h"
#include "og/og_tr.h"
#include <rpl/producer.h>
#include "ui/qt_object_factory.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/widgets/buttons.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/palette.h"
#include "styles/style_basic.h"
#include "styles/style_layers.h"

#include <array>

#include <QtGui/QBrush>
#include <QtGui/QColor>
#include <QtGui/QFont>
#include <QtGui/QFontMetrics>
#include <QtGui/QPen>

namespace Deep {
namespace {

constexpr auto kToastDuration = 1500;
constexpr auto kTabHeight = 34;
constexpr auto kMargin = 12;
constexpr auto kActionTop = 94;
constexpr auto kActionHeight = 34;
constexpr auto kActionSkip = 6;

// The reference paints the top of the right column dark - 67 logical px of
// rgb(23,33,43), the same colour as the toolbar button above it - and the rest
// white. Only the tab row and the first filter row sit inside that band (the
// action buttons start below it), so only the tab labels turn light there.
constexpr auto kTopDarkHeight = 67;
const auto kTopDarkBg = QColor(23, 33, 43);
const auto kTopDarkFg = QColor(255, 255, 255);

// 1 logical px darker line between the tab row and the filter rows: measured
// 71..72 physical px below the top of the band, i.e. logical ~35.5.
constexpr auto kTopDarkLineTop = 35;
const auto kTopDarkLineBg = QColor(16, 25, 33);

// The panel body below the band does not follow the system theme - the
// reference keeps it white in both light and dark mode - so its background
// and everything drawn on it are pinned to the light-mode colours.
const auto kBodyBg = QColor(255, 255, 255);
const auto kActionFg = QColor(63, 137, 199);

// Keys only - the text lives in the Deep translation data (:/deep/translations)
// with the English source string as the last fallback.
constexpr auto kUnreadKey = "RightToolBar::Unread Messages";
constexpr auto kTranslationKey = "RightToolBar::Translation";
constexpr auto kClientsKey = "RightToolBar::Client List";
constexpr auto kQuickReplyKey = "RightToolBar::Quick Reply";
constexpr auto kClientsDisabledKey = "RightToolBar::Client List is disabled";
constexpr auto kM2Key = "RightToolBar::M2 placeholder";

// The first segment of the first filter row shows the unread total of the
// current account after its label. One parameterised key keeps the number
// inside the translated string, so the translation owns the gap between the
// label and the badge: the reference draws the number in half-width brackets
// as an ink cluster separate from the two characters, with a wider gap than
// the one between them.
constexpr auto kRow2UnreadKey = "RightToolBar::Unread (%1)";

// Geometry of the two filter rows, taken from the measured crop of the
// reference window and expressed in widget-local logical px. The first row
// (unread / read / configured) starts at the panel edge, has no gaps and
// square segments; the second one (all / private / group / new) is 8 px in
// with equal 50 px segments and 6 px between them.
constexpr auto kRow2Left = 0;
constexpr auto kRow2Top = 38;
constexpr auto kRow2Height = 23;
constexpr auto kRow2Gap = 0;
constexpr auto kRow2Widths = std::array<int, 3>{ 55, 47, 47 };

constexpr auto kRow3Left = 8;
constexpr auto kRow3Top = 69;
constexpr auto kRow3Height = 22;
constexpr auto kRow3Gap = 6;
constexpr auto kRow3Widths = std::array<int, 4>{ 50, 50, 50, 50 };
constexpr auto kRow3Radius = 3;

// The reference keeps both rows in their light-mode colours in dark mode too,
// so they are pinned here instead of coming from the palette.
const auto kRow2Bg = QColor(255, 255, 255);
const auto kRow2Fg = QColor(0xC4, 0xC4, 0xFF);
const auto kRow2SelectedBg = QColor(0xF9, 0xA1, 0x2C);
const auto kRow2SelectedFg = QColor(255, 255, 255);

const auto kRow3Bg = QColor(255, 255, 255);
const auto kRow3Fg = QColor(0, 0, 0);
const auto kRow3SelectedBg = QColor(0x3A, 0xA0, 0xFF);
const auto kRow3SelectedFg = QColor(255, 255, 255);
const auto kRow3Border = QColor(0xCC, 0xCC, 0xCC);

// Labels of the two rows, in the order they are painted. The keys resolve
// through the RightToolBar context of the Deep translation data.
constexpr auto kRow2Keys = std::array<const char *, 3>{
	kRow2UnreadKey,
	"RightToolBar::Read",
	"RightToolBar::Configured",
};
constexpr auto kRow3Keys = std::array<const char *, 4>{
	"RightToolBar::All",
	"RightToolBar::Private",
	"RightToolBar::Group",
	"RightToolBar::New",
};

// The largest pixel size whose line height leaves kRowHeightSlack inside the
// row and whose labels still fit their own segment. Measuring at paint time
// keeps the gate honest for whichever font the platform picks at that pixel
// size.
constexpr auto kRowMaxPixelSize = 14;
constexpr auto kRowMinPixelSize = 6;
constexpr auto kRowHeightSlack = 6;
constexpr auto kRowTextSlack = 4;

struct FilterRowStyle {
	int height = 0;
	int radius = 0;
	QColor bg;
	QColor fg;
	QColor selectedBg;
	QColor selectedFg;
	QColor border; // transparent = the reference draws no outline here
};

const auto kRow2Style = FilterRowStyle{
	.height = kRow2Height,
	.radius = 0,
	.bg = kRow2Bg,
	.fg = kRow2Fg,
	.selectedBg = kRow2SelectedBg,
	.selectedFg = kRow2SelectedFg,
	.border = QColor(Qt::transparent),
};
const auto kRow3Style = FilterRowStyle{
	.height = kRow3Height,
	.radius = kRow3Radius,
	.bg = kRow3Bg,
	.fg = kRow3Fg,
	.selectedBg = kRow3SelectedBg,
	.selectedFg = kRow3SelectedFg,
	.border = kRow3Border,
};

// Segment rectangles of one row: `left` is the widget-local x of the first
// segment, `gap` the space left between the segments.
template <size_t N>
std::array<QRect, N> RowRects(
		int left,
		int top,
		int height,
		int gap,
		const std::array<int, N> &widths) {
	auto result = std::array<QRect, N>();
	auto x = left;
	for (auto i = 0; i != int(N); ++i) {
		result[i] = QRect(x, top, widths[i], height);
		x += widths[i] + gap;
	}
	return result;
}

template <size_t N>
std::array<QString, N> RowTexts(const std::array<const char *, N> &keys) {
	auto result = std::array<QString, N>();
	for (auto i = 0; i != int(N); ++i) {
		result[i] = OG::Tr::Lookup(keys[i]);
	}
	return result;
}

template <size_t N>
QFont RowFont(
		int rowHeight,
		const std::array<QRect, N> &rects,
		const std::array<QString, N> &texts) {
	const auto limit = rowHeight - kRowHeightSlack;
	auto font = QFont();
	for (auto size = kRowMaxPixelSize; size >= kRowMinPixelSize; --size) {
		font.setPixelSize(size);
		const auto metrics = QFontMetrics(font);
		if (metrics.height() > limit) {
			continue;
		}
		auto fits = true;
		for (auto i = 0; i != int(N); ++i) {
			const auto needed = metrics.horizontalAdvance(texts[i])
				+ kRowTextSlack;
			if (needed > rects[i].width()) {
				fits = false;
				break;
			}
		}
		if (fits) {
			return font;
		}
	}
	font.setPixelSize(kRowMinPixelSize);
	return font;
}

template <size_t N>
void PaintFilterRow(
		QPainter &p,
		const std::array<QRect, N> &rects,
		const std::array<QString, N> &texts,
		int selected,
		const FilterRowStyle &style) {
	const auto font = RowFont(style.height, rects, texts);
	const auto radius = qreal(style.radius);
	const auto border = (style.border.alpha() > 0);
	auto hq = PainterHighQualityEnabler(p);
	p.setFont(font);
	for (auto i = 0; i != int(N); ++i) {
		const auto &rect = rects[i];
		const auto active = (i == selected);
		const auto outline = !active && border;
		p.setPen(Qt::NoPen);
		p.setBrush(active ? style.selectedBg : style.bg);
		if (radius > 0) {
			p.drawRoundedRect(rect, radius, radius);
		} else {
			p.drawRect(rect);
		}
		if (outline) {
			p.setPen(QPen(style.border, 1));
			p.setBrush(Qt::NoBrush);
			if (radius > 0) {
				p.drawRoundedRect(rect, radius, radius);
			} else {
				p.drawRect(rect);
			}
		}
		p.setPen(active ? style.selectedFg : style.fg);
		p.drawText(rect, Qt::AlignCenter, texts[i]);
	}
}

} // namespace

RightToolBar::RightToolBar(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Ui::RpWidget(parent)
, _controller(controller) {
	setMinimumSize(kWidth, kHeight);
	resize(kWidth, kHeight);
	setupTabs();
	setupActions();
	setupUnreadCount();
}

void RightToolBar::setupTabs() {
	const auto addTab = [&](const char *key, bool disabled) {
		const auto button = Ui::CreateChild<Ui::RoundButton>(
			this,
			rpl::single(OG::Tr::Lookup(key)),
			st::defaultLightButton);
		const auto text = disabled
			? OG::Tr::Lookup(kClientsDisabledKey)
			: (OG::Tr::Lookup(key)
				+ QString::fromLatin1(" / ")
				+ OG::Tr::Lookup(kM2Key));
		// The tab row sits inside the dark band, so the button's own light
		// fill would both cover the band and hide the light text on it.
		button->setBrushOverride(QBrush(kTopDarkBg));
		button->setTextFgOverride(kTopDarkFg);
		button->setClickedCallback([=] {
			_controller->window().showToast(text, kToastDuration);
		});
		_tabs.push_back(button);
	};
	addTab(kUnreadKey, false);
	addTab(kQuickReplyKey, false);
	addTab(kTranslationKey, false);
	addTab(kClientsKey, true);
}

void RightToolBar::setupActions() {
	const auto addAction = [&](const char *key) {
		const auto button = Ui::CreateChild<Ui::RoundButton>(
			this,
			rpl::single(OG::Tr::Lookup(key)),
			st::defaultLightButton);
		// The body stays white in dark mode as well, so pin the button to the
		// light-mode look instead of letting the theme repaint it dark.
		button->setBrushOverride(QBrush(kBodyBg));
		button->setTextFgOverride(kActionFg);
		button->setClickedCallback([=] {
			_controller->window().showToast(
				OG::Tr::Lookup(key),
				kToastDuration);
		});
		_actions.push_back(button);
	};
	addAction("RightToolBar::Mark Selected Read");
	addAction("RightToolBar::Mark All Read");
	addAction("RightToolBar::Clear Selection");
}

void RightToolBar::setupUnreadCount() {
	// The badge shows the account-wide unread total of this session, the number
	// the application keeps for its own account badge: Session::unreadBadge()
	// folds the main chats list with the "count unread messages" and "include
	// muted" settings, and unreadBadgeChanges() fires whenever that list's
	// unread state changes. Nothing here is filtered, selected or written - the
	// label and its badge stay read-only.
	const auto session = &_controller->session();
	const auto refresh = [=] {
		const auto count = session->data().unreadBadge();
		if (_unreadCount != count) {
			_unreadCount = count;
			update();
		}
	};
	refresh();
	session->data().unreadBadgeChanges(
	) | rpl::on_next(refresh, _lifetime);
}

void RightToolBar::resizeEvent(QResizeEvent *e) {
	Ui::RpWidget::resizeEvent(e);

	const auto count = int(_tabs.size());
	const auto cell = count ? (width() / count) : width();
	for (auto i = 0; i != count; ++i) {
		_tabs[i]->setGeometry(i * cell, 0, cell, kTabHeight);
	}

	// The two filter rows above the actions are painted by hand, so the first
	// action sits at the position measured in the reference instead of being
	// derived from the tab row.
	auto top = kActionTop;
	for (const auto &action : _actions) {
		action->setGeometry(
			kMargin,
			top,
			width() - 2 * kMargin,
			kActionHeight);
		top += kActionHeight + kActionSkip;
	}
}

void RightToolBar::paintEvent(QPaintEvent *e) {
	// Without this the dock stays unpainted: the main window sets
	// Qt::WA_OpaquePaintEvent, so nothing behind it fills the panel. The
	// reference paints the top of the panel dark, the rest white.
	auto p = Painter(this);
	p.fillRect(rect(), kBodyBg);
	p.fillRect(QRect(0, 0, width(), kTopDarkHeight), kTopDarkBg);
	p.fillRect(QRect(0, kTopDarkLineTop, width(), 1), kTopDarkLineBg);

	// The first segment of this row carries the account's unread total, which
	// the key lookup alone cannot know: RowTexts() returns one translated
	// string per segment, so the count is substituted here.
	auto row2Texts = RowTexts(kRow2Keys);
	row2Texts[0] = row2Texts[0].arg(_unreadCount);
	PaintFilterRow(
		p,
		RowRects(kRow2Left, kRow2Top, kRow2Height, kRow2Gap, kRow2Widths),
		row2Texts,
		_row2Selected,
		kRow2Style);
	PaintFilterRow(
		p,
		RowRects(kRow3Left, kRow3Top, kRow3Height, kRow3Gap, kRow3Widths),
		RowTexts(kRow3Keys),
		_row3Selected,
		kRow3Style);
}

void RightToolBar::mousePressEvent(QMouseEvent *e) {
	// Both rows only switch their own selection, as in the reference M1 step:
	// no toast, no filtering, no live data behind them yet.
	if (e->button() == Qt::LeftButton) {
		const auto position = e->pos();
		const auto row2 = RowRects(
			kRow2Left,
			kRow2Top,
			kRow2Height,
			kRow2Gap,
			kRow2Widths);
		for (auto i = 0; i != int(row2.size()); ++i) {
			if (row2[i].contains(position)) {
				_row2Selected = i;
				update();
				return;
			}
		}
		const auto row3 = RowRects(
			kRow3Left,
			kRow3Top,
			kRow3Height,
			kRow3Gap,
			kRow3Widths);
		for (auto i = 0; i != int(row3.size()); ++i) {
			if (row3[i].contains(position)) {
				_row3Selected = i;
				update();
				return;
			}
		}
	}
	Ui::RpWidget::mousePressEvent(e);
}

} // namespace Deep
