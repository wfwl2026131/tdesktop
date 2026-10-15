/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_right_toolbar.h"

#include "base/object_ptr.h"
#include "og/og_tr.h"
#include <rpl/producer.h>
#include "ui/qt_object_factory.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/palette.h"
#include "styles/style_basic.h"
#include "styles/style_layers.h"

namespace Deep {
namespace {

constexpr auto kToastDuration = 1500;
constexpr auto kTabHeight = 34;
constexpr auto kMargin = 12;
constexpr auto kHintHeight = 36;
constexpr auto kActionHeight = 34;
constexpr auto kActionSkip = 6;

// Keys only - the text lives in the Deep translation data (:/deep/translations)
// with the English source string as the last fallback.
constexpr auto kUnreadKey = "RightToolBar::Unread Messages";
constexpr auto kTranslationKey = "RightToolBar::Translation";
constexpr auto kClientsKey = "RightToolBar::Client List";
constexpr auto kQuickReplyKey = "RightToolBar::Quick Reply";
constexpr auto kClientsDisabledKey = "RightToolBar::Client List is disabled";
constexpr auto kM2Key = "RightToolBar::M2 placeholder";
constexpr auto kHintKey = "RightToolBar::M1 static skeleton";

} // namespace

RightToolBar::RightToolBar(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Ui::RpWidget(parent)
, _controller(controller) {
	setMinimumSize(kWidth, kHeight);
	resize(kWidth, kHeight);
	setupTabs();
	setupHint();
	setupActions();
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

void RightToolBar::setupHint() {
	_hint = Ui::CreateChild<Ui::FlatLabel>(
		this,
		rpl::single(OG::Tr::Lookup(kHintKey)),
		st::defaultFlatLabel);
}

void RightToolBar::setupActions() {
	const auto addAction = [&](const char *key) {
		const auto button = Ui::CreateChild<Ui::RoundButton>(
			this,
			rpl::single(OG::Tr::Lookup(key)),
			st::defaultLightButton);
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

void RightToolBar::resizeEvent(QResizeEvent *e) {
	Ui::RpWidget::resizeEvent(e);

	const auto count = int(_tabs.size());
	const auto cell = count ? (width() / count) : width();
	for (auto i = 0; i != count; ++i) {
		_tabs[i]->setGeometry(i * cell, 0, cell, kTabHeight);
	}

	auto top = kTabHeight + kMargin;
	if (_hint) {
		_hint->setGeometry(
			kMargin,
			top,
			width() - 2 * kMargin,
			kHintHeight);
		top += kHintHeight + kMargin;
	}
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
	// Qt::WA_OpaquePaintEvent, so nothing behind it fills the panel.
	auto p = Painter(this);
	p.fillRect(rect(), st::windowBg);
}

} // namespace Deep
