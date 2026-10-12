/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_toolbar.h"

#include "base/object_ptr.h"
#include <rpl/producer.h>
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/labels.h"
#include "styles/style_layers.h"

namespace OG {
namespace {

void AddToolbarLine(
		not_null<Ui::VerticalLayout*> parent,
		const char *text) {
	parent->add(object_ptr<Ui::FlatLabel>(
		parent,
		rpl::single(QString::fromUtf8(text)),
		st::defaultFlatLabel));
}

} // namespace

void FillToolbarBox(not_null<Ui::GenericBox*> box) {
	box->setWidth(st::boxWideWidth);
	box->setTitle(rpl::single(
		QString::fromUtf8("\345\267\245\345\205\267\346\240\217 \302\267 Officegram \351\235\231\346\200\201\346\240\267\346\235\277 M1.5a")));

	const auto content = box->verticalLayout();
	Ui::AddSkip(content);

	AddToolbarLine(content, "Tab\357\274\232\346\234\252\350\257\273\346\266\210\346\201\257 | \345\277\253\351\200\237\345\233\236\345\244\215 | \347\277\273\350\257\221 | \345\256\242\346\210\267\345\210\227\350\241\250");
	Ui::AddSkip(content);
	AddToolbarLine(content, "\346\234\252\350\257\273\346\266\210\346\201\257\357\274\232\346\214\211\344\274\232\350\257\235\345\210\227\346\234\252\350\257\273\357\274\210M2 \345\256\236\346\227\266\302\267\347\255\233\351\200\211\357\274\232\345\205\250\351\203\250/\347\247\201\350\201\212/\347\276\244\347\273\204/\346\226\260\345\256\242\357\274\211");
	Ui::AddSkip(content);
	AddToolbarLine(content, "\345\277\253\351\200\237\345\233\236\345\244\215\357\274\232\350\257\235\346\234\257\345\272\223+\347\274\226\350\276\221\345\214\272\357\274\210auto_reply\302\267tdata/auto_reply_config.json\357\274\211");
	Ui::AddSkip(content);
	AddToolbarLine(content, "\347\277\273\350\257\221\357\274\232\345\257\206\351\222\245\347\212\266\346\200\201/\345\267\262\347\224\250/\347\274\223\345\255\230\345\255\227\347\254\246\357\274\210fetch_lg_keys\302\267report_lg_usage\357\274\211");
	Ui::AddSkip(content);
	AddToolbarLine(content, "\345\256\242\346\210\267\345\210\227\350\241\250\357\274\232\350\231\232\346\213\237\345\210\227\350\241\250\302\267\345\217\214\351\223\276\346\211\223\345\274\200\344\274\232\350\257\235\357\274\210portrait \350\241\250 CRM\357\274\211");
	Ui::AddSkip(content);

	Ui::AddSkip(content);
	box->addButton(
		rpl::single(QString::fromUtf8("\345\267\262\350\257\273\346\211\200\351\200\211")),
		[=] { box->closeBox(); });
	box->addButton(
		rpl::single(QString::fromUtf8("\345\205\250\351\203\250\345\267\262\350\257\273")),
		[=] { box->closeBox(); });
	box->addButton(
		rpl::single(QString::fromUtf8("\346\270\205\351\231\244\345\213\276\351\200\211")),
		[=] { box->closeBox(); });
	box->addButton(
		rpl::single(QString::fromUtf8("\345\205\263\351\227\255")),
		[=] { box->closeBox(); });
}

} // namespace OG
