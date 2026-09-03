#include "keystore_ui_backend.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

// The generated umbrella carrying `struct LogosModules`.
#include "logos_sdk.h"

namespace {

QJsonObject parseObject(const QString &reply)
{
    return QJsonDocument::fromJson(reply.toUtf8()).object();
}

QString compact(const QJsonValue &v)
{
    if (v.isArray())
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    if (v.isObject())
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    return {};
}

QString params(const QJsonObject &o)
{
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

} // namespace

void KeystoreUiBackend::say(const QString &line)
{
    setLastError(lastError().isEmpty() ? line : lastError() + QLatin1Char('\n') + line);
}

bool KeystoreUiBackend::ok(const QString &reply, const QString &context)
{
    const QJsonObject o = parseObject(reply);
    if (o.value(QStringLiteral("ok")).toBool())
        return true;
    QString e = o.value(QStringLiteral("error")).toString();
    if (e.isEmpty())
        e = QStringLiteral("the keystore refused the request");
    // "not authorized" is what a non-custodian gets. Say what that means rather than
    // repeating a string that reads like a bug.
    if (e == QLatin1String("not authorized"))
        e = QStringLiteral("this build is not the keystore's configured custodian, so it may "
                           "not change accounts");
    say(context.isEmpty() ? e : QStringLiteral("%1: %2").arg(context, e));
    return false;
}

bool KeystoreUiBackend::read(const QString &key, const QString &reply, const QString &context)
{
    const bool good = ok(reply, context);
    m_reads[key] = good;
    return good;
}

void KeystoreUiBackend::publishReads()
{
    setReadsJson(QString::fromUtf8(QJsonDocument(m_reads).toJson(QJsonDocument::Compact)));
}

void KeystoreUiBackend::onContextReady()
{
    // The account list changes under us when this UI is not the only thing running.
    modules().keystore_module.onAccounts_changed([this](int) {
        QTimer::singleShot(0, [this] { refresh(); });
    });
    refresh();
}

void KeystoreUiBackend::loadIdentity()
{
    const QString reply = modules().keystore_module.caller_identity();
    const QJsonObject o = parseObject(reply);
    setIdentityJson(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));

    // Say plainly whether we hold the role, instead of letting every mutation fail
    // mysteriously one screen later.
    const QString me = o.value(QStringLiteral("identity")).toString();
    const QString custodian = o.value(QStringLiteral("custodian")).toString();
    setIsCustodian(!me.isEmpty() && me == custodian);
}

void KeystoreUiBackend::loadAccounts()
{
    // A refused read clears what it feeds, like the wallet reads below. The listing can now
    // fail — an unreadable keystore directory refuses instead of answering "no accounts" —
    // and leaving the previous list on screen would show accounts with a confidence the
    // keystore has just withdrawn, next to an error saying it could not read them.
    const QString accounts = modules().keystore_module.list_accounts();
    setAccountsJson(read(QStringLiteral("accounts"), accounts, QStringLiteral("accounts"))
                        ? compact(parseObject(accounts).value(QStringLiteral("accounts")))
                        : QStringLiteral("[]"));

    const QString labels = modules().keystore_module.get_labels();
    setLabelsJson(read(QStringLiteral("labels"), labels, QStringLiteral("labels"))
                      ? compact(parseObject(labels).value(QStringLiteral("labels")))
                      : QStringLiteral("{}"));
    publishReads();
}

void KeystoreUiBackend::loadGroups()
{
    // A refused read clears what it feeds. Leaving the previous answer on screen would say
    // "no wallets" or "these wallets" with the same confidence the keystore just withdrew.
    const QString groups = modules().keystore_module.list_groups();
    setGroupsJson(read(QStringLiteral("groups"), groups, QStringLiteral("wallets"))
                      ? compact(parseObject(groups).value(QStringLiteral("groups")))
                      : QStringLiteral("[]"));

    const QString provenance = modules().keystore_module.get_provenance();
    setProvenanceJson(read(QStringLiteral("provenance"), provenance, QStringLiteral("provenance"))
                          ? compact(parseObject(provenance).value(QStringLiteral("accounts")))
                          : QStringLiteral("{}"));

    // Its own document, so a wallet whose record is gone keeps its name — and an unreadable
    // one refuses here rather than answering that nothing is named.
    const QString names = modules().keystore_module.get_group_labels();
    setWalletNamesJson(read(QStringLiteral("walletNames"), names, QStringLiteral("wallet names"))
                           ? compact(parseObject(names).value(QStringLiteral("labels")))
                           : QStringLiteral("{}"));

    // Read from the key directory alone, so it survives an unreadable groups.json. Without
    // it a whole-wallet key whose record is gone could not be named, and so never deleted.
    // The whole reply is carried: `staged` and `unexplained` are the states that refuse a
    // new account, and a refusal the user cannot see the reason for is a wedge.
    const QString keys = modules().keystore_module.list_derivation_keys();
    QJsonObject dir;
    if (read(QStringLiteral("derivationKeys"), keys, QStringLiteral("derivation keys"))) {
        const QJsonObject r = parseObject(keys);
        for (const QString &k : { QStringLiteral("groups"), QStringLiteral("staged"),
                                  QStringLiteral("unexplained") })
            dir[k] = r.value(k).toArray();
    }
    setKeyDirectoryJson(compact(dir));
    publishReads();
}

void KeystoreUiBackend::refresh()
{
    setBusy(true);
    setLastError(QString());
    loadIdentity();
    loadAccounts();
    loadGroups();
    setStatusText(isCustodian() ? QStringLiteral("Ready")
                                : QStringLiteral("Not the configured custodian"));
    setBusy(false);
}

QString KeystoreUiBackend::generateMnemonic(int words)
{
    setLastError(QString());
    const QString reply = modules().keystore_module.create_mnemonic(words);
    if (!ok(reply, QStringLiteral("generate")))
        return {};
    return parseObject(reply).value(QStringLiteral("phrase")).toString();
}

bool KeystoreUiBackend::importMnemonic(QString phrase, QString bip39Passphrase,
                                       QString accountPassword, QString groupPassword,
                                       bool derivable, QString groupLabel)
{
    setLastError(QString());
    QJsonObject p;
    p[QStringLiteral("phrase")] = phrase;
    p[QStringLiteral("passphrase")] = bip39Passphrase;
    p[QStringLiteral("password")] = accountPassword;
    // The wallet's name, kept whichever storage is chosen: a group record is written for both,
    // and this is the only moment the keystore accepts one.
    p[QStringLiteral("groupLabel")] = groupLabel;
    // An import always creates the wallet's FIRST account; further ones come from
    // deriveNextAccount, which is the only thing that knows which indices are taken.
    p[QStringLiteral("storage")] = derivable ? QStringLiteral("extkey") : QStringLiteral("plain");
    if (derivable)
        p[QStringLiteral("groupPassword")] = groupPassword;
    const bool good = ok(modules().keystore_module.import_mnemonic(params(p)), QString());
    if (good)
        refresh();
    return good;
}

bool KeystoreUiBackend::importPrivateKey(QString privHex, QString accountPassword)
{
    setLastError(QString());
    const bool good =
        ok(modules().keystore_module.import_private_key(privHex, accountPassword), QString());
    if (good)
        refresh();
    return good;
}

bool KeystoreUiBackend::importVaultJson(QString vaultJson, QString oldPassword, QString newPassword)
{
    setLastError(QString());
    const bool good = ok(
        modules().keystore_module.import_keystore_json(vaultJson, oldPassword, newPassword),
        QString());
    if (good)
        refresh();
    return good;
}

bool KeystoreUiBackend::deriveNextAccount(QString group, QString groupPassword,
                                          QString accountPassword)
{
    setLastError(QString());
    QJsonObject p;
    // Omitted, not empty: the keystore reads an absent group as "the only one that can
    // derive", and refuses by name when several can.
    if (!group.isEmpty())
        p[QStringLiteral("group")] = group;
    p[QStringLiteral("groupPassword")] = groupPassword;
    p[QStringLiteral("password")] = accountPassword;
    const bool good = ok(modules().keystore_module.derive_next_account(params(p)), QString());
    if (good)
        refresh();
    return good;
}

bool KeystoreUiBackend::deriveAccountAt(QString group, QString groupPassword,
                                        QString accountPassword, int bip44Account, int change,
                                        int index)
{
    setLastError(QString());
    // The keystore's path levels are unsigned; a negative would come back as an opaque
    // parse failure instead of something the person typing it can act on.
    if (bip44Account < 0 || change < 0 || index < 0) {
        say(QStringLiteral("account, change and index must not be negative"));
        return false;
    }
    QJsonObject p;
    p[QStringLiteral("group")] = group;
    p[QStringLiteral("groupPassword")] = groupPassword;
    p[QStringLiteral("password")] = accountPassword;
    p[QStringLiteral("bip44Account")] = bip44Account;
    p[QStringLiteral("change")] = change;
    p[QStringLiteral("index")] = index;
    const bool good = ok(modules().keystore_module.derive_account_at(params(p)), QString());
    if (good)
        refresh();
    return good;
}

QString KeystoreUiBackend::previewAddresses(QString group, QString groupPassword, int change,
                                            int from, int count)
{
    setLastError(QString());
    if (change < 0 || from < 0 || count < 0) {
        say(QStringLiteral("change, start and count must not be negative"));
        return {};
    }
    QJsonObject p;
    p[QStringLiteral("group")] = group;
    p[QStringLiteral("groupPassword")] = groupPassword;
    p[QStringLiteral("change")] = change;
    p[QStringLiteral("from")] = from;
    p[QStringLiteral("count")] = count;
    const QString reply = modules().keystore_module.preview_addresses(params(p));
    if (!ok(reply, QStringLiteral("preview")))
        return {};
    return compact(parseObject(reply).value(QStringLiteral("addresses")));
}

bool KeystoreUiBackend::forgetDerivation(QString group)
{
    setLastError(QString());
    QJsonObject p;
    p[QStringLiteral("group")] = group;
    const QString reply = modules().keystore_module.forget_derivation(params(p));
    const bool good = ok(reply, QStringLiteral("stop keeping the derivation key"));
    if (good)
        refresh();
    // The key is gone either way. A stranded key had no record to update; anything else
    // means groups.json is unreadable, and the wallet may still read as derivable. Appended,
    // because refresh() above may have just recorded refusals of its own.
    const QJsonObject r = parseObject(reply);
    if (good && !r.value(QStringLiteral("recordUpdated")).toBool()
        && !r.value(QStringLiteral("stranded")).toBool())
        say(QStringLiteral("the derivation key is deleted, but this wallet's record could not "
                           "be updated — it may still read as derivable"));
    return good;
}

bool KeystoreUiBackend::removeWallet(QString group)
{
    setLastError(QString());
    QJsonObject p;
    p[QStringLiteral("group")] = group;
    const QString reply = modules().keystore_module.remove_group(params(p));
    const QJsonObject r = parseObject(reply);
    // Already gone is not a refusal to report: another surface removed it, and the row this
    // was pressed on is stale. Match on the tail — the keystore prefixes the variant.
    if (!r.value(QStringLiteral("ok")).toBool()
        && r.value(QStringLiteral("error")).toString().contains(QLatin1String("no such group"))) {
        refresh();
        return true;
    }
    const bool good = ok(reply, QStringLiteral("remove wallet"));
    if (good)
        refresh();
    return good;
}

QString KeystoreUiBackend::exportVaultJson(QString address, QString password)
{
    setLastError(QString());
    const QString reply = modules().keystore_module.export_keystore_json(address, password);
    if (!ok(reply, QStringLiteral("export")))
        return {};
    return parseObject(reply).value(QStringLiteral("keystore")).toString();
}

bool KeystoreUiBackend::setLabel(QString address, QString label, QString password)
{
    setLastError(QString());
    // The keystore decides which of the two this is; it ignores the password when the label
    // is blank, and no KDF runs. Passing it either way keeps that rule in one place.
    const bool good = ok(modules().keystore_module.set_label(address, label, password),
                         QStringLiteral("rename"));
    if (good)
        loadAccounts();
    return good;
}

bool KeystoreUiBackend::setWalletName(QString group, QString name, QString address,
                                      QString password)
{
    setLastError(QString());
    QJsonObject p;
    p[QStringLiteral("group")] = group;
    p[QStringLiteral("label")] = name;
    // Omitted rather than empty: a present address is what tells the keystore an ACCOUNT is
    // being offered, so sending a blank one would turn a wallet with none into a failed
    // lookup. The password travels on its own, because a wallet with no accounts and a key
    // on disk is proved against that key instead.
    if (!address.isEmpty())
        p[QStringLiteral("address")] = address;
    if (!password.isEmpty())
        p[QStringLiteral("password")] = password;
    const bool good = ok(modules().keystore_module.set_group_label(params(p)),
                         QStringLiteral("rename wallet"));
    if (good)
        loadGroups();
    return good;
}

bool KeystoreUiBackend::changePassword(QString address, QString oldPassword, QString newPassword)
{
    setLastError(QString());
    return ok(modules().keystore_module.change_password(address, oldPassword, newPassword),
              QStringLiteral("change password"));
}

bool KeystoreUiBackend::deleteAccount(QString address, QString password)
{
    setLastError(QString());
    // delete_account answers a bare bool: a refusal and a wrong password are indistinguishable
    // there by construction, so there is no message to surface beyond this.
    const bool good = modules().keystore_module.delete_account(address, password);
    if (good)
        refresh();
    else
        say(QStringLiteral("could not delete the account — wrong password, or this build is "
                           "not the configured custodian"));
    return good;
}
