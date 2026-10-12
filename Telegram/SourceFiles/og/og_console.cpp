/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_console.h"

#include "base/object_ptr.h"
#include <rpl/producer.h>
#include "settings/settings_common.h"
#include "ui/layers/generic_box.h"
#include "ui/painter.h"
#include "ui/rp_widget.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/table_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_window.h"

namespace OG {
namespace {
constexpr auto kMockToastDuration = 1500;
constexpr auto kSeatCols = 50;
constexpr auto kSeatRows = 6;
constexpr auto kSeatTotal = kSeatCols * kSeatRows;
constexpr auto kSeatRowHeight = 34;
constexpr auto kSeatPadding = 8;

struct LicenseRow {
	const char *key;
	const char *value;
};

const LicenseRow kLicenseRows[] = {
		{"Team", "Officegram\357\274\210M2 \346\216\245 /xxloginv3 team_name\357\274\211"},
		{"User", "alice\357\274\210\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 \345\275\223\345\211\215\344\274\232\350\257\235\345\217\267\357\274\211"},
		{"Seat", "200 / \347\275\221\346\240\274 300\357\274\210seat = /get-account-info \345\256\236\346\227\266\345\200\274\357\274\211"},
		{"Expire", "\346\260\270\344\271\205\357\274\210expire_time 2147483647 \302\267 \347\247\201\344\272\272\347\211\210\345\217\243\345\276\204\357\274\211"},
		{"Pointcard", "\346\234\252\346\277\200\346\264\273\357\274\210\347\273\255\350\264\271 = /agent_user_activate_pointcard\357\274\211"}
};

struct FeatureRow {
	const char *name;
	const char *toast;
	const style::icon *icon;
};

const FeatureRow kFeatures[] = {
		{"\346\212\242\347\272\242\345\214\205 RedPacket", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 features red_packet + /api/_rob_*", &st::menuIconGroups},
		{"\345\257\271\346\225\262 Cross Trading", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 red_packet_stats.json \344\275\231\351\242\235/\345\257\271\346\225\262", &st::menuIconSavedMessages},
		{"\347\276\244\345\217\221\347\247\201\344\277\241 v2", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 mass_send_dm_v2 \344\273\273\345\212\241\351\223\276", &st::menuIconUserShow},
		{"\345\212\240\345\245\275\345\217\213", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 add_contact txt/\351\200\232\350\256\257\345\275\225", &st::menuIconUserShow},
		{"\346\211\271\351\207\217\351\202\200\350\257\267", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 batch_invite", &st::menuIconUserShow},
		{"\344\273\243\350\201\212 v2", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 puppet_chat_v2", &st::menuIconSavedMessages},
		{"\347\277\273\350\257\221", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 translation /fetch_lg_keys /report_lg_usage", &st::menuIconSavedMessages},
		{"\346\211\271\351\207\217\346\213\211\344\272\272 v2", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 batch_pull_v2", &st::menuIconGroups},
		{"\347\276\244\347\256\241", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 group_manager", &st::menuIconGroups},
		{"\345\267\245\344\275\234\346\265\201\344\272\224\346\255\245\351\223\276", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 Contacts\342\206\222Group\342\206\222Puppet\342\206\222Pull\342\206\222Send", &st::menuIconChannel},
		{"\350\207\252\345\212\250\345\233\236\345\244\215", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 auto_reply tdata/auto_reply_config.json", &st::menuIconSavedMessages},
		{"\345\256\242\346\210\267CRM\346\241\243\346\241\210", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 portrait \350\241\250 30+ \345\255\227\346\256\265", &st::menuIconUserShow},
		{"\350\264\246\345\217\267\346\211\230\347\256\241", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 /upload-accounts /get-account-info", &st::menuIconChannel},
		{"\345\244\232\350\264\246\345\217\267\347\256\241\347\220\206", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 AccountManager \346\266\210\346\201\257\345\233\236\350\260\203", &st::menuIconUserShow},
		{"\347\276\244\347\273\204\351\230\262\345\205\263\350\201\224", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 AccountSpamGroupManager", &st::menuIconGroups},
		{"\345\244\264\345\203\217\347\274\223\345\255\230\346\261\240", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 AvatarCacheManager", &st::menuIconSavedMessages},
		{"\345\244\232\345\217\267\347\274\226\350\276\221\347\212\266\346\200\201", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 MultiAccountEditStateManager", &st::menuIconChannel},
		{"\345\270\255\344\275\215\345\272\217\345\210\227", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 SeatSequenceManager", &st::menuIconChannel},
		{"\345\276\205\345\212\236\351\230\237\345\210\227", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 FromTodoManager task \350\241\250", &st::menuIconSavedMessages},
		{"\345\215\241\345\257\206\347\273\255\350\264\271", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 /agent_user_activate_pointcard", &st::menuIconChannel},
		{"\345\205\254\345\221\212\344\270\255\345\277\203", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 /_ff ns.notice_id \345\274\271\347\252\227\344\275\215", &st::menuIconSavedMessages},
		{"\350\277\220\350\241\214\346\227\245\345\277\227", "\351\235\231\346\200\201\346\240\267\346\235\277 \302\267 \346\216\247\345\210\266\345\217\260\346\227\245\345\277\227\345\214\272 M2 \346\273\232\345\212\250\346\235\241", &st::menuIconGroups}
};

class SeatGrid final : public Ui::RpWidget {
public:
	explicit SeatGrid(QWidget *parent)
	: Ui::RpWidget(parent) {
	}

	[[nodiscard]] int heightForWidth(int) const override {
		return kSeatRows * kSeatRowHeight + kSeatPadding;
	}

protected:
	void paintEvent(QPaintEvent *e) override {
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

};

void AddMockLabel(
		not_null<Ui::VerticalLayout*> parent,
		const char *text) {
	parent->add(object_ptr<Ui::FlatLabel>(
		parent,
		rpl::single(QString::fromUtf8(text)),
		st::defaultFlatLabel));
}

} // namespace

void FillConsoleBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setWidth(680);
	box->setTitle(rpl::single(
		QString::fromUtf8("\346\216\247\345\210\266\345\217\260 \302\267 Officegram \351\235\231\346\200\201\346\240\267\346\235\277 M1.5a")));

	const auto content = box->verticalLayout();
	Ui::AddSkip(content);

	auto license = object_ptr<Ui::TableLayout>(content.get());
	const auto licenseRaw = license.data();
	content->add(std::move(license), st::boxRowPadding);
	for (const auto &row : kLicenseRows) {
		licenseRaw->addRow(
			object_ptr<Ui::FlatLabel>(
				licenseRaw,
				rpl::single(QString::fromUtf8(row.key)),
				st::defaultFlatLabel),
			object_ptr<Ui::FlatLabel>(
				licenseRaw,
				rpl::single(QString::fromUtf8(row.value)),
				st::defaultFlatLabel));
	}

	Ui::AddSkip(content);
	AddMockLabel(content,
		"\345\270\255\344\275\215\347\275\221\346\240\274\357\274\232\347\273\277=\345\234\250\347\272\277 \347\201\260=\346\234\252\347\224\250 \347\272\242=\350\277\207\346\234\237\357\274\210M2 \346\215\242\345\256\236\346\227\266\347\212\266\346\200\201\357\274\211");
	Ui::AddSkip(content);
	content->add(object_ptr<SeatGrid>(content.get()), st::boxRowPadding);

	Ui::AddSkip(content);
	Ui::AddDivider(content);
	Ui::AddSkip(content);

	auto main = object_ptr<Ui::TableLayout>(content.get());
	const auto mainRaw = main.data();
	content->add(std::move(main), st::boxRowPadding);
	auto left = object_ptr<Ui::VerticalLayout>(mainRaw);
	const auto leftRaw = left.data();
	auto right = object_ptr<Ui::VerticalLayout>(mainRaw);
	const auto rightRaw = right.data();
	mainRaw->addRow(std::move(left), std::move(right));

	AddMockLabel(leftRaw,
		"\345\212\237\350\203\275\345\210\227\350\241\250\357\274\210" "22 \351\241\271\357\274\211");
	for (const auto &feature : kFeatures) {
		const auto button = Settings::AddButtonWithIcon(
			leftRaw,
			rpl::single(QString::fromUtf8(feature.name)),
			st::mainMenuButton,
			Settings::IconDescriptor{ .icon = feature.icon });
		button->setClickedCallback([=, text = QString::fromUtf8(feature.toast)] {
			controller->window().showToast(text, kMockToastDuration);
		});
	}

	AddMockLabel(rightRaw,
		"\346\223\215\344\275\234\345\214\272\357\274\232\347\276\244\345\217\221\347\247\201\344\277\241 v2\357\274\210mass_send_dm_v2\357\274\211");
	Ui::AddSkip(rightRaw);
	AddMockLabel(rightRaw,
		"\345\252\222\344\275\223+\346\226\207\346\234\254 / \350\264\237\350\275\275\345\235\207\350\241\241 / \346\257\217\346\227\245\344\270\212\351\231\220 / \345\256\232\346\227\266\345\220\257\345\212\250 / \347\272\277\347\250\213\346\225\260\357\274\210\345\216\237\347\211\210\345\255\227\346\256\265\351\233\206\357\274\214M2 \351\200\220\351\241\271\346\216\245\347\272\277\357\274\211");
	Ui::AddSkip(rightRaw);
	rightRaw->add(
		object_ptr<Ui::RoundButton>(
			rightRaw,
			rpl::single(QString::fromUtf8(
				"\345\220\257\345\212\250\344\273\273\345\212\241 \302\267 \351\235\231\346\200\201\346\240\267\346\235\277")),
			st::defaultLightButton),
		st::boxRowPadding);
	rightRaw->add(
		object_ptr<Ui::RoundButton>(
			rightRaw,
			rpl::single(QString::fromUtf8(
				"\345\201\234\346\255\242\344\273\273\345\212\241 \302\267 \351\235\231\346\200\201\346\240\267\346\235\277")),
			st::defaultLightButton),
		st::boxRowPadding);

	Ui::AddSkip(content);
	box->addButton(
		rpl::single(QString::fromUtf8("\345\205\263\351\227\255")),
		[=] { box->closeBox(); });
}

} // namespace OG
