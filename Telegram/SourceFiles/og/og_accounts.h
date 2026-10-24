/*
This file is part of an Officegram project, a Telegram Desktop fork
for marketing CRM workflows.

For license and copyright information of the underlying Telegram Desktop
code please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>

#include <cstdint>

namespace OG {

// The account pool, as a table of the store.
//
// Every console entry that works on more than one session (batch join, batch
// login, batch rename, video manage, filter phone) starts from the same thing:
// the list of accounts the operator imported, with the state each one is in.
// The reference keeps that list next to its project files and re-reads it on
// start; here it is one table of the store document, so the pages read it
// through the usual accessors instead of parsing files of their own.
//
// The columns follow the pool the reference keeps and the console describes:
// a label to name the row, the phone it was imported from, the session file it
// belongs to, the state it was last seen in, and the two way / frozen
// distinction its bulk logouts are built on. Nothing here talks to Telegram:
// the columns are what the operator entered plus what a previous run recorded.

// What the pool knows about the number itself. The reference treats the three
// as different buckets in its logouts (frozen, then two way, then the rest),
// so an unknown row must never be silently folded into one of them.
enum class AccountKind {
	Unknown,
	Frozen,
	OneWay,
	TwoWay,
};

struct AccountRow {
	qint64 id = 0;
	QString label; // unique, the pool names every row by it
	QString phone; // as entered, kept without spaces or dashes
	QString session; // path of the .session file, empty until imported
	QString status; // new / ok / logged-out / limited
	AccountKind kind = AccountKind::Unknown;
	qint64 added_at = 0;
	qint64 last_seen = 0;
};

// The three tokens the reference writes into its configuration files. The
// spelling is the one the python side of this project already stores, so a
// database moved between the two keeps its meaning.
[[nodiscard]] QString AccountKindToken(AccountKind kind);
[[nodiscard]] AccountKind AccountKindFromToken(const QString &token);
[[nodiscard]] bool IsKnownAccountKind(AccountKind kind);

// The status column, restricted to what the pool actually records. A page that
// shows something else would be inventing a state.
[[nodiscard]] QString AccountStatusToken(const QString &status);
[[nodiscard]] bool IsKnownAccountStatus(const QString &status);

// A phone is stored digits only, with a leading plus for a fully qualified
// number: that is what both the reference files and the MTP layer want, and
// normalising on the way in keeps two rows for one number from existing.
[[nodiscard]] QString NormalizePhone(const QString &raw);

// A label is the row's primary key, so it has to survive being typed twice:
// surrounding whitespace is dropped and the label must not be empty.
[[nodiscard]] QString NormalizeAccountLabel(const QString &raw);

// The pool rejects a second row with the same label or the same number: two
// sessions for one number is the shape that gets both of them limited.
[[nodiscard]] bool PoolAccepts(
	const AccountRow &candidate,
	const AccountRow *byLabel,
	const AccountRow *byPhone);

// Prints the pool's rules against a fixed set of rows. Not called from the
// client: it is the driver the acceptance run builds, the same shape the
// auto-reply matcher uses.
int RunAccountsSelfCheck();

} // namespace OG
