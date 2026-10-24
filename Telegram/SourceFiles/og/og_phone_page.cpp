/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "og/og_phone_page.h"

#include "og/og_phone.h"
#include "og/og_store.h"
#include "og/og_tr.h"

#include <QtCore/QPointer>
#include <QtCore/QStringList>

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

namespace OG {
namespace {

constexpr auto kMargin = 16;

// A range can be huge and the page must not try to hold it: the preview shows
// a sample of the numbers and the counters show the size, which is what the
// reference does with its own preview pane. The counters stop at the limit
// below, and the label says so instead of quietly reporting a smaller range.
constexpr auto kPreviewLimit = 20;
constexpr auto kRangeLimit = 100000;

} // namespace

PhonePageWindow::PhonePageWindow(
	QWidget *parent,
	not_null<Window::SessionController*> controller,
	bool embedded)
: QDialog(parent) {
	Q_UNUSED(controller);

	if (!embedded) {
		setWindowTitle(Tr::Lookup("PhoneFilterPage::Filter Phone"));
		setWindowFlags(
			Qt::Dialog
			| Qt::WindowTitleHint
			| Qt::WindowSystemMenuHint
			| Qt::WindowCloseButtonHint);
		resize(kWidth, kHeight);
	} else {
		setWindowFlags(Qt::Widget);
	}
	setupContent();
	rebuild();
}

void PhonePageWindow::Show(
		not_null<Window::SessionController*> controller) {
	static auto instance = QPointer<PhonePageWindow>();
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
	const auto window = new PhonePageWindow(nullptr, controller);
	window->setAttribute(Qt::WA_DeleteOnClose);
	instance = window;
	window->show();
	window->raise();
	window->activateWindow();
}

void PhonePageWindow::reject() {
	hide();
}

void PhonePageWindow::setupContent() {
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
	layout->setSpacing(10);

	layout->addWidget(new QLabel(Tr::Lookup(
		"PhoneFilterPage::Number Source"), this));

	auto *codeLine = new QHBoxLayout();
	codeLine->setSpacing(8);
	codeLine->addWidget(new QLabel(Tr::Lookup(
		"PhoneFilterPage::Pick Country"), this));
	_callingCode = new QLineEdit(this);
	_callingCode->setPlaceholderText(Tr::Lookup(
		"PhoneFilterPage::Calling code without +"));
	_callingCode->setMaximumWidth(120);
	codeLine->addWidget(_callingCode);
	codeLine->addStretch(1);
	layout->addLayout(codeLine);

	auto *rangeLine = new QHBoxLayout();
	rangeLine->setSpacing(8);
	rangeLine->addWidget(new QLabel(Tr::Lookup(
		"PhoneFilterPage::Range start (digits only)"), this));
	_rangeStart = new QSpinBox(this);
	_rangeStart->setRange(0, 999999999);
	rangeLine->addWidget(_rangeStart);
	rangeLine->addWidget(new QLabel(Tr::Lookup(
		"PhoneFilterPage::Range end (digits only)"), this));
	_rangeEnd = new QSpinBox(this);
	_rangeEnd->setRange(0, 999999999);
	rangeLine->addWidget(_rangeEnd);
	// Not named `refresh`: the member function is what the handlers call, and a
	// local of that name shadows it, which the compiler reports as "called
	// object type 'QPushButton *' is not a function".
	auto *refreshButton = new QPushButton(Tr::Lookup(
		"PhoneFilterPage::Refresh Output"), this);
	rangeLine->addWidget(refreshButton);
	rangeLine->addStretch(1);
	layout->addLayout(rangeLine);

	_preview = new QLabel(this);
	_preview->setWordWrap(true);
	_preview->setText(Tr::Lookup(
		"PhoneFilterPage::Phone numbers preview will appear here"));
	layout->addWidget(_preview);

	_counters = new QLabel(this);
	_counters->setWordWrap(true);
	layout->addWidget(_counters);

	layout->addWidget(new QLabel(Tr::Lookup(
		"PhoneFilterPage::Execution Settings"), this));

	auto *globalLine = new QHBoxLayout();
	globalLine->setSpacing(8);
	globalLine->addWidget(new QLabel(Tr::Lookup(
		"PhoneFilterPage::Global interval (ms)"), this));
	_globalInterval = new QSpinBox(this);
	_globalInterval->setRange(0, 600000);
	_globalInterval->setSingleStep(100);
	globalLine->addWidget(_globalInterval);
	globalLine->addStretch(1);
	layout->addLayout(globalLine);

	auto *singleLine = new QHBoxLayout();
	singleLine->setSpacing(8);
	singleLine->addWidget(new QLabel(Tr::Lookup(
		"PhoneFilterPage::Single account interval (ms)"), this));
	_singleInterval = new QSpinBox(this);
	_singleInterval->setRange(0, 600000);
	_singleInterval->setSingleStep(100);
	singleLine->addWidget(_singleInterval);
	singleLine->addStretch(1);
	layout->addLayout(singleLine);

	// The reference explains the two numbers in one line under them, and the
	// explanation is the rule the pacer applies.
	auto *note = new QLabel(Tr::Lookup(
		"PhoneFilterPage::Global interval throttles every request across all"
		" accounts. Single account interval is the minimum delay between"
		" requests from the same account."), this);
	note->setWordWrap(true);
	layout->addWidget(note);

	auto *buttons = new QHBoxLayout();
	buttons->setSpacing(8);
	_start = new QPushButton(Tr::Lookup(
		"PhoneFilterPage::Filter started"), this);
	buttons->addWidget(_start);
	buttons->addStretch(1);
	layout->addLayout(buttons);

	_status = new QLabel(this);
	_status->setWordWrap(true);
	layout->addWidget(_status);
	layout->addStretch(1);

	connect(refreshButton, &QPushButton::clicked, this, [this] {
		refresh();
	});
	connect(_callingCode, &QLineEdit::textChanged, this, [this] {
		refresh();
	});
	connect(
		_rangeStart,
		QOverload<int>::of(&QSpinBox::valueChanged),
		this,
		[this] {
			refresh();
		});
	connect(
		_rangeEnd,
		QOverload<int>::of(&QSpinBox::valueChanged),
		this,
		[this] {
			refresh();
		});
	connect(
		_globalInterval,
		QOverload<int>::of(&QSpinBox::valueChanged),
		this,
		[this] {
			save();
		});
	connect(
		_singleInterval,
		QOverload<int>::of(&QSpinBox::valueChanged),
		this,
		[this] {
			save();
		});
	connect(_start, &QPushButton::clicked, this, [this] {
		const auto code = _callingCode->text().trimmed();
		const auto from = _rangeStart->value();
		const auto to = _rangeEnd->value();
		if (code.isEmpty()) {
			_status->setText(Tr::Lookup(
				"PhoneFilterPage::Country not selected"));
			return;
		}
		if (to < from) {
			_status->setText(Tr::Lookup(
				"PhoneFilterPage::Range end must not be below the start"));
			return;
		}
		// Same rule as the other pages: this milestone decides the request and
		// says what it would be; sending belongs to the session that comes
		// next.
		_status->setText(
			Tr::Lookup("PhoneFilterPage::Filter started")
			+ QString::fromLatin1(": +")
			+ code
			+ QString::fromLatin1(" ")
			+ QString::number(from)
			+ QString::fromLatin1("-")
			+ QString::number(to));
	});
}

void PhonePageWindow::rebuild() {
	const auto config = StoreInstance().phoneFilterSettings();
	_globalInterval->blockSignals(true);
	_singleInterval->blockSignals(true);
	_globalInterval->setValue(config.globalIntervalMs);
	_singleInterval->setValue(config.singleIntervalMs);
	_globalInterval->blockSignals(false);
	_singleInterval->blockSignals(false);
	refresh();
}

void PhonePageWindow::refresh() {
	auto code = _callingCode->text().trimmed();
	while (code.startsWith(QChar('+'))) {
		code.remove(0, 1);
	}
	const auto from = qint64(_rangeStart->value());
	const auto to = qint64(_rangeEnd->value());
	if (code.isEmpty() || (to < from)) {
		_preview->setText(Tr::Lookup(
			"PhoneFilterPage::Phone numbers preview will appear here"));
		_counters->setText(Tr::Lookup(
			"PhoneFilterPage::Range size: 0"));
		return;
	}
	// The calling code is a prefix of the national number, so the number the
	// page shows is what the run would send: the same normalization the list
	// splitter uses, which is why a range of zeros is rejected here and not
	// three requests later.
	const auto full = to - from + 1;
	const auto size = qMin<qint64>(full, kRangeLimit);
	auto preview = QStringList();
	auto valid = 0;
	const auto step = qMax<qint64>((size + kPreviewLimit - 1) / kPreviewLimit, 1);
	for (auto i = from; i < from + size; ++i) {
		const auto raw = code + QString::number(i);
		const auto number = NormalizePhoneNumber(raw);
		if (number.isEmpty()) {
			continue;
		}
		++valid;
		if (preview.size() < kPreviewLimit && ((i - from) % step == 0)) {
			preview.append(number);
		}
	}
	_preview->setText(preview.isEmpty()
		? Tr::Lookup("PhoneFilterPage::Phone numbers preview will appear here")
		: preview.join(QString::fromLatin1(", ")));
	_counters->setText(
		Tr::Lookup("PhoneFilterPage::Range size: %1")
			.arg(QString::number(size))
		+ QString::fromLatin1("   ")
		+ Tr::Lookup("PhoneFilterPage::Valid: %1")
			.arg(QString::number(valid))
		+ QString::fromLatin1("   ")
		+ Tr::Lookup("PhoneFilterPage::Invalid: %1")
			.arg(QString::number(size - valid))
		+ ((full > size)
			? QString::fromLatin1("   ")
				+ Tr::Lookup("PhoneFilterPage::Processed: %1 / %2")
					.arg(QString::number(size))
					.arg(QString::number(full))
			: QString()));
}

void PhonePageWindow::save() {
	auto config = StoreInstance().phoneFilterSettings();
	config.globalIntervalMs = _globalInterval->value();
	config.singleIntervalMs = _singleInterval->value();
	StoreInstance().setPhoneFilterSettings(config);
}

} // namespace OG
