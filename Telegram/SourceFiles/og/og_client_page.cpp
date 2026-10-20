/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_client_page.h"

#include "og/og_tr.h"

#include <QtCore/QPointer>

#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>

namespace OG {
namespace {

constexpr auto kMargin = 10;

constexpr auto kPlatformWidth = 70;
constexpr auto kTextWidth = 150;
constexpr auto kStateWidth = 70;
constexpr auto kNumberWidth = 80;
constexpr auto kCellPadding = 8;

// The eight columns this page shows, in the order the reference puts them on
// the row. The other columns of the client record stay out of the view.
enum Column {
	kPlatformColumn = 0,
	kUsernameColumn,
	kPhoneColumn,
	kNicknameColumn,
	kStateColumn,
	kSentColumn,
	kRecvColumn,
	kAddColumn,
	kColumnCount,
};

[[nodiscard]] int ColumnWidth(int column) {
	switch (column) {
	case kPlatformColumn: return kPlatformWidth;
	case kUsernameColumn: return kTextWidth;
	case kPhoneColumn: return kTextWidth;
	case kNicknameColumn: return kTextWidth;
	case kStateColumn: return kStateWidth;
	default: return kNumberWidth;
	}
}

[[nodiscard]] bool AlignRight(int column) {
	return (column == kPlatformColumn)
		|| (column == kStateColumn)
		|| (column == kSentColumn)
		|| (column == kRecvColumn)
		|| (column == kAddColumn);
}

[[nodiscard]] QString ValueText(const QString &value) {
	return value.isEmpty() ? QString::fromLatin1("-") : value;
}

// Every cell keeps the width its column was given and the text is elided, so
// a long username can never push the page wider than its window.
QLabel *AddCell(
		QGridLayout *grid,
		QWidget *parent,
		int row,
		int column,
		const QString &text,
		bool bold = false) {
	const auto width = ColumnWidth(column);
	auto *label = new QLabel(parent);
	auto font = label->font();
	font.setBold(bold);
	label->setFont(font);
	label->setFixedWidth(width);
	label->setText(label->fontMetrics().elidedText(
		text,
		Qt::ElideRight,
		width - kCellPadding));
	if (!bold) {
		label->setToolTip(text);
	}
	label->setAlignment(
		(AlignRight(column) ? Qt::AlignRight : Qt::AlignLeft)
		| Qt::AlignVCenter);
	grid->addWidget(label, row, column, Qt::AlignLeft | Qt::AlignVCenter);
	return label;
}

} // namespace

ClientPageWindow::ClientPageWindow(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: QDialog(parent) {
	// The controller is part of the signature for parity with the console
	// window and for an entry point that is still undecided; the page itself
	// reads the store only.
	Q_UNUSED(controller);

	setWindowTitle(Tr::Lookup("ClientPage::Title"));
	setWindowFlags(
		Qt::Dialog
		| Qt::WindowTitleHint
		| Qt::WindowSystemMenuHint
		| Qt::WindowCloseButtonHint);
	resize(kWidth, kHeight);
	setupContent();
	rebuild();
}

void ClientPageWindow::Show(
		not_null<Window::SessionController*> controller) {
	static auto instance = QPointer<ClientPageWindow>();
	if (instance) {
		if (instance->windowState() & Qt::WindowMinimized) {
			instance->setWindowState(
				(instance->windowState() & ~Qt::WindowMinimized)
				| Qt::WindowActive);
		}
		instance->show();
		instance->raise();
		instance->activateWindow();
		return;
	}
	const auto window = new ClientPageWindow(nullptr, controller);
	window->setAttribute(Qt::WA_DeleteOnClose);
	instance = window;
	window->show();
	window->raise();
	window->activateWindow();
}

void ClientPageWindow::reject() {
	// Escape hides the window instead of destroying it, so the next Show()
	// brings back the same instance.
	hide();
}

void ClientPageWindow::setupContent() {
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(kMargin, kMargin, kMargin, kMargin);

	_empty = new QLabel(Tr::Lookup("ClientPage::Empty"), this);
	_empty->setAlignment(Qt::AlignCenter);
	layout->addWidget(_empty);

	_scroll = new QScrollArea(this);
	_scroll->setWidgetResizable(true);
	_scroll->setFrameShape(QFrame::NoFrame);
	layout->addWidget(_scroll, 1);

	_host = new QWidget();
	_grid = new QGridLayout(_host);
	_grid->setContentsMargins(0, 0, 0, 0);
	// No column spacing on purpose: every cell already keeps kCellPadding
	// pixels of its own width free on the right (the text is elided into
	// width - kCellPadding), so the gap is there. Counting it twice needs
	// 830 + 7 * 8 = 886 px, which is wider than the 866 px left once the
	// vertical scroll bar is in, and the last column would be pushed out.
	_grid->setHorizontalSpacing(0);
	_grid->setVerticalSpacing(4);
	_grid->setAlignment(Qt::AlignTop);
	_scroll->setWidget(_host);
}

void ClientPageWindow::rebuild() {
	// One refresh per open for now: the page is read only, so nothing here
	// can change the store underneath it.
	while (auto *item = _grid->takeAt(0)) {
		if (auto *widget = item->widget()) {
			widget->hide();
			widget->deleteLater();
		}
		delete item;
	}

	const auto &clients = StoreInstance().clients();
	const auto empty = clients.empty();
	_empty->setVisible(empty);
	_scroll->setVisible(!empty);
	if (empty) {
		return;
	}

	AddCell(_grid, _host, 0, kPlatformColumn,
		Tr::Lookup("ClientPage::Platform"), true);
	AddCell(_grid, _host, 0, kUsernameColumn,
		Tr::Lookup("ClientPage::Username"), true);
	AddCell(_grid, _host, 0, kPhoneColumn,
		Tr::Lookup("ClientPage::Phone"), true);
	AddCell(_grid, _host, 0, kNicknameColumn,
		Tr::Lookup("ClientPage::Nickname"), true);
	AddCell(_grid, _host, 0, kStateColumn,
		Tr::Lookup("ClientPage::State"), true);
	AddCell(_grid, _host, 0, kSentColumn,
		Tr::Lookup("ClientPage::Sent Count"), true);
	AddCell(_grid, _host, 0, kRecvColumn,
		Tr::Lookup("ClientPage::Recv Count"), true);
	AddCell(_grid, _host, 0, kAddColumn,
		Tr::Lookup("ClientPage::Add Count"), true);

	for (auto i = 0; i != int(clients.size()); ++i) {
		const auto &client = clients[i];
		const auto row = i + 1;
		AddCell(_grid, _host, row, kPlatformColumn,
			QString::number(client.platform));
		AddCell(_grid, _host, row, kUsernameColumn,
			ValueText(client.username));
		AddCell(_grid, _host, row, kPhoneColumn,
			ValueText(client.phone));
		AddCell(_grid, _host, row, kNicknameColumn,
			ValueText(client.nickname));
		AddCell(_grid, _host, row, kStateColumn,
			QString::number(client.state));
		AddCell(_grid, _host, row, kSentColumn,
			QString::number(client.sent_count));
		AddCell(_grid, _host, row, kRecvColumn,
			QString::number(client.recv_count));
		AddCell(_grid, _host, row, kAddColumn,
			QString::number(client.add_count));
	}
}

} // namespace OG
