import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Logos.Controls
import Logos.Theme

// The keystore.
//
// This is the only surface that creates, imports, exports or deletes an account. Wallets
// read which accounts exist and request signatures; they never reach any of this. The
// keystore enforces that by caller identity — this view being the only one that *shows* the
// controls is convenience, not the boundary.
//
// Secrets are held for exactly as long as the screen showing them: a generated phrase and an
// exported vault come back from a SLOT and are cleared when the sheet closes. Neither is a
// property, because a property is cached in the shell process and broadcast to every replica.
//
// A .rep SLOT with a return type answers a QRemoteObjectPendingReply, NOT the value — it is
// async over QtRO. Every such call goes through `logos.watch(call, onSuccess, onError)`. Using
// the return directly compiles, runs, and is always truthy, so a failed import silently looks
// like a successful one.
//
// Rendering rule: every item showing a string this view did not author sets
// `textFormat: Text.PlainText`. LogosText is a bare Text with no textFormat, i.e. Qt's
// AutoText HTML autodetection.
Item {
    id: root
    objectName: "keystoreRoot"
    anchors.fill: parent

    Rectangle { anchors.fill: parent; color: Theme.palette.background }

    readonly property var backend: logos.module("keystore_ui")

    // Writable, fed by the signal — a binding containing a function call evaluates once at
    // creation, before ui-host has handed over, and latches false forever.
    property bool ready: false
    Connections {
        target: logos
        function onViewModuleReadyChanged(moduleName, isReady) {
            if (moduleName === "keystore_ui") root.ready = isReady && root.backend !== null
        }
    }

    function j(t, fb) { try { return JSON.parse(t && t.length ? t : fb) } catch (e) { return JSON.parse(fb) } }
    readonly property var accounts: ready ? j(backend.accountsJson, "[]") : []
    readonly property var labels: ready ? j(backend.labelsJson, "{}") : ({})
    readonly property bool custodian: ready && backend.isCustodian

    function shortAddr(a) { return (!a || a.length < 12) ? (a || "") : a.substring(0,6) + "…" + a.substring(a.length-4) }
    function labelFor(a) {
        if (!a) return ""
        var k = a.replace(/^0x/, "").toLowerCase()
        for (var key in labels) if (key.toLowerCase() === k) return labels[key]
        return ""
    }
    property string selected: ""

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacing.medium
        spacing: Theme.spacing.small

        RowLayout {
            Layout.fillWidth: true
            LogosText { text: "Accounts"; font.pixelSize: 22 }
            Item { Layout.fillWidth: true }
            LogosBadge {
                objectName: "custodianBadge"
                text: root.custodian ? "CUSTODIAN" : "READ ONLY"
                color: root.custodian ? Theme.palette.success : Theme.palette.warning
            }
        }

        // Said once, plainly, rather than letting every button fail one screen later.
        LogosText {
            objectName: "notCustodianNotice"
            Layout.fillWidth: true
            visible: root.ready && !root.custodian
            wrapMode: Text.WordWrap
            color: Theme.palette.textSecondary
            text: "This build is not the keystore's configured custodian, so accounts cannot be "
                  + "changed here. The keystore names its custodian in keystore.json."
        }

        LogosText {
            objectName: "errorLabel"
            Layout.fillWidth: true
            visible: root.ready && root.backend.lastError.length > 0
            textFormat: Text.PlainText
            wrapMode: Text.WordWrap
            color: Theme.palette.error
            text: root.ready ? root.backend.lastError : ""
        }

        RowLayout {
            spacing: Theme.spacing.tiny
            LogosButton { objectName: "newAccountButton";  text: "Create";        enabled: root.custodian; onClicked: createSheet.open() }
            LogosButton { objectName: "importPhraseButton"; text: "Import phrase"; enabled: root.custodian; onClicked: phraseSheet.open() }
            LogosButton { objectName: "importKeyButton";    text: "Import key";    enabled: root.custodian; onClicked: keySheet.open() }
            LogosButton { objectName: "importVaultButton";  text: "Import vault";  enabled: root.custodian; onClicked: vaultSheet.open() }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            LogosText {
                objectName: "accountsEmpty"
                anchors.centerIn: parent
                visible: root.accounts.length === 0
                text: "No accounts yet"
                color: Theme.palette.textSecondary
            }
            LogosListView {
                objectName: "accountList"
                anchors.fill: parent
                visible: root.accounts.length > 0
                model: root.accounts
                delegate: RowLayout {
                    width: ListView.view ? ListView.view.width : 0
                    LogosText {
                        objectName: "account_" + modelData
                        textFormat: Text.PlainText
                        text: (root.labelFor(modelData) ? root.labelFor(modelData) + "  ·  " : "")
                              + root.shortAddr(modelData)
                    }
                    Item { Layout.fillWidth: true }
                    LogosButton {
                        objectName: "manage_" + modelData
                        text: "Manage"
                        enabled: root.custodian
                        onClicked: { root.selected = modelData; manageSheet.open() }
                    }
                }
            }
        }
    }

    // ── Create: generate, SHOW, confirm, then commit ───────────────────────────────
    LogosDialog {
        id: createSheet
        objectName: "createSheet"
        title: "Create an account"
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 560)

        // Held here only, and cleared on close. Never a property.
        property string phrase: ""
        property var words: []
        onOpened: { phrase = ""; words = []; confirmField.text = ""; newPw.text = "" }
        onClosed: { phrase = ""; words = [] }

        contentItem: ColumnLayout {
            spacing: Theme.spacing.small

            LogosButton {
                objectName: "generateButton"
                text: "Generate a recovery phrase"
                visible: createSheet.phrase.length === 0
                onClicked: logos.watch(root.backend.generateMnemonic(12),
                    function (phrase) {
                        createSheet.phrase = phrase || ""
                        createSheet.words = createSheet.phrase.length ? createSheet.phrase.split(" ") : []
                    })
            }

            LogosText {
                objectName: "phraseWarning"
                visible: createSheet.phrase.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.palette.warning
                text: "Write this down. It is the only way to recover the account, it is shown "
                      + "once, and anyone who reads it can spend from the account."
            }
            LogosText {
                objectName: "phraseText"
                visible: createSheet.phrase.length > 0
                Layout.fillWidth: true
                textFormat: Text.PlainText
                wrapMode: Text.WordWrap
                text: createSheet.phrase
            }

            LogosText {
                visible: createSheet.phrase.length > 0
                color: Theme.palette.textSecondary
                text: createSheet.words.length > 0
                      ? "Confirm words 1, 5 and 12, separated by spaces"
                      : ""
            }
            LogosTextField {
                id: confirmField
                objectName: "confirmField"
                visible: createSheet.phrase.length > 0
                Layout.fillWidth: true
                placeholderText: "word1 word5 word12"
            }
            LogosTextField {
                id: newPw
                objectName: "createPasswordField"
                visible: createSheet.phrase.length > 0
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: "Password for this account"
            }

            RowLayout {
                Layout.fillWidth: true
                LogosButton { objectName: "createCancel"; text: "Cancel"; onClicked: createSheet.close() }
                Item { Layout.fillWidth: true }
                LogosButton {
                    objectName: "createConfirm"
                    text: "Create account"
                    // The confirmation is the point: an unwritten phrase is an unrecoverable
                    // account, so it is checked here before anything is stored.
                    enabled: createSheet.words.length === 12 && newPw.text.length > 0
                             && confirmField.text.trim().toLowerCase().split(/\s+/).join(" ")
                                === [createSheet.words[0], createSheet.words[4], createSheet.words[11]].join(" ")
                    onClicked: logos.watch(root.backend.importMnemonic(createSheet.phrase, newPw.text),
                        function (good) { if (good) createSheet.close() })
                }
            }
        }
    }

    // ── Import: phrase / private key / vault JSON ──────────────────────────────────
    LogosDialog {
        id: phraseSheet
        objectName: "importPhraseSheet"
        title: "Import a recovery phrase"
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 560)
        onOpened: { seedField.text = ""; seedPw.text = "" }
        onClosed: seedField.text = ""
        contentItem: ColumnLayout {
            spacing: Theme.spacing.small
            LogosTextArea {
                id: seedField
                objectName: "seedField"
                Layout.fillWidth: true
                placeholderText: "Recovery phrase (BIP-39 words)"
            }
            LogosTextField {
                id: seedPw
                objectName: "seedPasswordField"
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: "Password for this account"
            }
            RowLayout {
                Layout.fillWidth: true
                LogosButton { text: "Cancel"; onClicked: phraseSheet.close() }
                Item { Layout.fillWidth: true }
                LogosButton {
                    objectName: "importPhraseConfirm"
                    text: "Import"
                    enabled: seedField.text.trim().length > 0 && seedPw.text.length > 0
                    onClicked: logos.watch(root.backend.importMnemonic(seedField.text.trim(), seedPw.text),
                        function (good) { if (good) phraseSheet.close() })
                }
            }
        }
    }

    LogosDialog {
        id: keySheet
        objectName: "importKeySheet"
        title: "Import a private key"
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 560)
        onOpened: { keyField.text = ""; keyPw.text = "" }
        onClosed: keyField.text = ""
        contentItem: ColumnLayout {
            spacing: Theme.spacing.small
            LogosTextField {
                id: keyField
                objectName: "privKeyField"
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: "Private key (hex)"
            }
            LogosTextField {
                id: keyPw
                objectName: "privKeyPasswordField"
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: "Password for this account"
            }
            RowLayout {
                Layout.fillWidth: true
                LogosButton { text: "Cancel"; onClicked: keySheet.close() }
                Item { Layout.fillWidth: true }
                LogosButton {
                    objectName: "importKeyConfirm"
                    text: "Import"
                    enabled: keyField.text.trim().length > 0 && keyPw.text.length > 0
                    onClicked: logos.watch(root.backend.importPrivateKey(keyField.text.trim(), keyPw.text),
                        function (good) { if (good) keySheet.close() })
                }
            }
        }
    }

    LogosDialog {
        id: vaultSheet
        objectName: "importVaultSheet"
        title: "Import a keystore vault"
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 560)
        onOpened: { vaultField.text = ""; vaultOld.text = ""; vaultNew.text = "" }
        onClosed: vaultField.text = ""
        contentItem: ColumnLayout {
            spacing: Theme.spacing.small
            LogosTextArea {
                id: vaultField
                objectName: "vaultJsonField"
                Layout.fillWidth: true
                placeholderText: "Vault JSON"
            }
            LogosTextField {
                id: vaultOld
                objectName: "vaultOldPasswordField"
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: "Its current password"
            }
            LogosTextField {
                id: vaultNew
                objectName: "vaultNewPasswordField"
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: "Password to store it under here"
            }
            RowLayout {
                Layout.fillWidth: true
                LogosButton { text: "Cancel"; onClicked: vaultSheet.close() }
                Item { Layout.fillWidth: true }
                LogosButton {
                    objectName: "importVaultConfirm"
                    text: "Import"
                    enabled: vaultField.text.trim().length > 0 && vaultNew.text.length > 0
                    onClicked: logos.watch(root.backend.importVaultJson(vaultField.text.trim(), vaultOld.text, vaultNew.text),
                        function (good) { if (good) vaultSheet.close() })
                }
            }
        }
    }

    // ── Manage one account: rename, change password, export, delete ────────────────
    LogosDialog {
        id: manageSheet
        objectName: "manageSheet"
        title: "Manage account"
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 560)

        property string exported: ""
        onOpened: {
            exported = ""
            renameField.text = root.labelFor(root.selected)
            oldPw.text = ""; newPw2.text = ""; exportPw.text = ""; deletePw.text = ""
        }
        onClosed: exported = ""

        contentItem: ColumnLayout {
            spacing: Theme.spacing.small

            LogosText {
                objectName: "manageAddress"
                textFormat: Text.PlainText
                Layout.fillWidth: true
                wrapMode: Text.WrapAnywhere
                text: root.selected
            }

            LogosText { text: "Name"; color: Theme.palette.textSecondary }
            RowLayout {
                Layout.fillWidth: true
                LogosTextField { id: renameField; objectName: "renameField"; Layout.fillWidth: true; placeholderText: "Account name" }
                LogosButton {
                    objectName: "renameConfirm"
                    text: "Rename"
                    onClicked: logos.watch(root.backend.setLabel(root.selected, renameField.text), function () {})
                }
            }

            LogosText { text: "Change password"; color: Theme.palette.textSecondary }
            LogosTextField { id: oldPw;  objectName: "oldPasswordField"; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: "Current password" }
            LogosTextField { id: newPw2; objectName: "newPasswordField"; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: "New password" }
            LogosButton {
                objectName: "changePasswordConfirm"
                text: "Change password"
                enabled: oldPw.text.length > 0 && newPw2.text.length > 0
                onClicked: logos.watch(root.backend.changePassword(root.selected, oldPw.text, newPw2.text),
                    function (good) { if (good) { oldPw.text = ""; newPw2.text = "" } })
            }

            LogosText { text: "Export"; color: Theme.palette.textSecondary }
            LogosTextField { id: exportPw; objectName: "exportPasswordField"; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: "Password" }
            LogosButton {
                objectName: "exportConfirm"
                text: "Export vault JSON"
                enabled: exportPw.text.length > 0
                onClicked: logos.watch(root.backend.exportVaultJson(root.selected, exportPw.text),
                    function (vault) { manageSheet.exported = vault || "" })
            }
            LogosText {
                objectName: "exportedVault"
                visible: manageSheet.exported.length > 0
                Layout.fillWidth: true
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                // Shown so it can be copied, and dropped when the sheet closes.
                text: manageSheet.exported
            }

            LogosText { text: "Delete"; color: Theme.palette.error }
            LogosTextField { id: deletePw; objectName: "deletePasswordField"; Layout.fillWidth: true; echoMode: TextInput.Password; placeholderText: "Password, to confirm" }
            LogosButton {
                objectName: "deleteConfirm"
                text: "Delete this account"
                enabled: deletePw.text.length > 0
                onClicked: logos.watch(root.backend.deleteAccount(root.selected, deletePw.text),
                    function (good) { if (good) manageSheet.close() })
            }

            LogosButton { objectName: "manageClose"; text: "Close"; onClicked: manageSheet.close() }
        }
    }
}
