import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Logos.Controls
import Logos.Theme

// The storage choice, in one place because the same words appear on Create and on Import and
// two copies is how they drift apart.
//
// It is made once for a whole wallet, not per account, because it cannot honestly be made per
// account: the key that derives account 3 derives account 5 as well. The question the wording
// answers is "what does someone who copies this folder get", because that is the only part of
// this the person choosing can actually weigh.
ColumnLayout {
    id: root

    readonly property bool derivable: keepOption.checked
    readonly property string groupPassword: keepPw.text
    // A kept derivation key needs a password of its own; the keystore refuses without one.
    readonly property bool complete: !derivable || keepPw.text.length > 0

    function reset() { plainOption.checked = true; keepPw.text = "" }

    spacing: Theme.spacing.tiny

    ButtonGroup { id: choice }

    LogosText {
        text: "Adding another account later"
        color: Theme.palette.textSecondary
    }

    LogosRadioButton {
        id: plainOption
        objectName: "storagePlainOption"
        ButtonGroup.group: choice
        checked: true
        text: "Do not keep the phrase  (recommended)"
    }
    LogosText {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.spacing.xlarge
        wrapMode: Text.WordWrap
        color: Theme.palette.textSecondary
        text: "If someone copies this folder and guesses this account's password, they get "
              + "this one account. To add another account later, you will have to type your "
              + "recovery phrase again."
    }

    LogosRadioButton {
        id: keepOption
        objectName: "storageKeepOption"
        ButtonGroup.group: choice
        text: "Keep a derivation key"
    }
    LogosText {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.spacing.xlarge
        wrapMode: Text.WordWrap
        color: Theme.palette.warning
        text: "Adding another account later takes one click. But if someone copies this "
              + "folder and guesses this wallet's password, they get every account under "
              + "this wallet — including ones you have not created yet."
    }
    LogosTextField {
        id: keepPw
        objectName: "groupPasswordField"
        visible: keepOption.checked
        Layout.fillWidth: true
        Layout.leftMargin: Theme.spacing.xlarge
        echoMode: TextInput.Password
        placeholderText: "Password for this wallet's derivation key"
    }
    // The counterweight, so this reads as a trade and not as a one-sided scare.
    LogosText {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.spacing.xlarge
        visible: keepOption.checked
        wrapMode: Text.WordWrap
        color: Theme.palette.textSecondary
        text: "In exchange, your recovery phrase is handled once, here, instead of once for "
              + "every account you ever add."
    }
}
