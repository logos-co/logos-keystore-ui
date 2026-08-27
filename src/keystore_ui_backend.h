#pragma once

#include <QObject>
#include <QString>

#include "rep_keystore_ui_source.h"
#include "logos_ui_plugin_context.h"

// The keystore UI backend.
//
// Every keystore call is made here over the generated typed client; the QML half renders and
// collects passwords. This module is the configured CUSTODIAN — the one caller the keystore's
// Tier D admits — which is why the mutating calls below exist here and nowhere else.
//
// Secrets are returned from methods, never published as properties: a property is cached in
// the shell process and broadcast to every connected replica.
class KeystoreUiBackend : public KeystoreUiSimpleSource,
                          public LogosUiPluginContext
{
public:
    void refresh() override;

    QString generateMnemonic(int words) override;
    bool importMnemonic(QString phrase, QString accountPassword) override;
    bool importPrivateKey(QString privHex, QString accountPassword) override;
    bool importVaultJson(QString vaultJson, QString oldPassword, QString newPassword) override;
    bool createAccount(QString accountPassword) override;

    QString exportVaultJson(QString address, QString password) override;

    bool setLabel(QString address, QString label) override;
    bool changePassword(QString address, QString oldPassword, QString newPassword) override;
    bool deleteAccount(QString address, QString password) override;

protected:
    void onContextReady() override;

private:
    /// Surface a keystore refusal verbatim and report whether the call succeeded. The rule
    /// that produced the message lives in the keystore; restating it here would be a copy
    /// free to drift from the one that actually governs.
    bool ok(const QString &reply, const QString &context);
    void loadAccounts();
    void loadIdentity();
};
