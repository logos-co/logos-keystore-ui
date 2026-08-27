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

} // namespace

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
    setLastError(context.isEmpty() ? e : QStringLiteral("%1: %2").arg(context, e));
    return false;
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
    const QString accounts = modules().keystore_module.list_accounts();
    if (ok(accounts, QStringLiteral("accounts")))
        setAccountsJson(compact(parseObject(accounts).value(QStringLiteral("accounts"))));

    const QString labels = modules().keystore_module.get_labels();
    if (ok(labels, QStringLiteral("labels")))
        setLabelsJson(compact(parseObject(labels).value(QStringLiteral("labels"))));
}

void KeystoreUiBackend::refresh()
{
    setBusy(true);
    setLastError(QString());
    loadIdentity();
    loadAccounts();
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

bool KeystoreUiBackend::importMnemonic(QString phrase, QString accountPassword)
{
    setLastError(QString());
    QJsonObject p;
    p[QStringLiteral("phrase")] = phrase;
    p[QStringLiteral("accountIndex")] = 0;
    p[QStringLiteral("password")] = accountPassword;
    const QString params = QString::fromUtf8(QJsonDocument(p).toJson(QJsonDocument::Compact));
    const bool good = ok(modules().keystore_module.import_mnemonic(params), QString());
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

bool KeystoreUiBackend::createAccount(QString accountPassword)
{
    setLastError(QString());
    const bool good = ok(modules().keystore_module.new_account(accountPassword), QString());
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

bool KeystoreUiBackend::setLabel(QString address, QString label)
{
    setLastError(QString());
    const bool good = ok(modules().keystore_module.set_label(address, label), QStringLiteral("rename"));
    if (good)
        loadAccounts();
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
        setLastError(QStringLiteral("could not delete the account — wrong password, or this "
                                    "build is not the configured custodian"));
    return good;
}
