/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_panel.h"

#include "base/object_ptr.h"
#include "og/og_core.h"
#include <rpl/producer.h>
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/labels.h"
#include "styles/style_layers.h"

namespace OG {

void FillWorkbenchBox(not_null<Ui::GenericBox*> box) {
	box->setWidth(st::boxWideWidth);
	box->setTitle(rpl::single(
		QString::fromUtf8("\350\220\245\351\224\200\345\267\245\344\275\234\345\217\260 \302\267 Officegram M1")));

	const auto content = box->verticalLayout();
	Ui::AddSkip(content);

	const auto addRow = [&](const QString &text) {
		content->add(object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(text),
			st::defaultFlatLabel));
	};
	addRow(QString::fromUtf8("OG \346\234\215\345\212\241\347\253\257\357\274\232%1")
		.arg(OG::Config::serverBase()));
	addRow(QString::fromUtf8("\345\270\255\344\275\215\344\270\212\351\231\220\357\274\232%1\357\274\210M2 \350\265\267\346\224\271\344\270\272 /get-account-info \345\256\236\346\227\266\345\200\274\357\274\211")
		.arg(OG::Config::kSeatLimit));
	addRow(QString::fromUtf8("\345\277\203\350\267\263 /_ff\357\274\232\346\234\252\350\277\236\346\216\245\357\274\210M2 \345\256\236\350\243\205\357\274\211"));
	addRow(QString::fromUtf8("\346\250\241\345\235\227\357\274\232og_core \342\234\223 / og_menu \342\234\223 / og_panel \342\234\223"
		" / og_events \302\267 og_accounts_sync \302\267 og_gate\357\274\210\345\276\205\345\273\272\357\274\211"));

	Ui::AddSkip(content);
	box->addButton(
		rpl::single(QString::fromUtf8("\345\205\263\351\227\255")),
		[=] { box->closeBox(); });
}

} // namespace OG
