/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>

namespace OG {

// Deep translation layer of the reference client reproduced for our fork.
//
// The reference keeps interface copy out of the binary and looks it up at
// runtime by a "Class::Text" key (evidence: `RightToolBar::Quick Reply`,
// `LoginWidget::Username`). The data lives in Qt .qm files compiled into the
// binary through the Qt resource system at `:/deep/translations/deep_%1.qm`
// (evidence: that exact template, plus the English fallback log chain).
//
// Rules for call sites:
//   * never put Chinese UI text in code, and never search the binary for it;
//   * address every string as "Class::Text" so the context equals the class
//     name, which is what QTranslator resolves against;
//   * the English text in the source is the last fallback, exactly like the
//     reference's `trying English fallback` step.
namespace Tr {

// Loads :/deep/translations/deep_<lang>.qm, falling back to deep_en.qm and
// then to the English source strings. Call once, before any Lookup().
void Start();

// Resolves "Class::Text" through the installed .qm; returns the English
// source text when the resource or the entry is missing.
[[nodiscard]] QString Lookup(const char *key);
[[nodiscard]] QString Lookup(const QString &key);

// Language code requested by the settings item (M1: OG_DEEP_LANG or "en").
[[nodiscard]] QString LanguageCode();

} // namespace Tr
} // namespace OG
