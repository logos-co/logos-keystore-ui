# keystore_ui

The keystore management surface. Create, import, export, rename, re-password and delete accounts.

This is the only module permitted to mutate the keystore: `keystore_module` gates its account-management methods on a configured *custodian*, and that custodian is this module. Wallet UIs can read which accounts exist; they cannot create, import or export one, and they never see a seed phrase, a private key or a vault password.

Part of the [Logos](https://github.com/logos-co) modular application platform.
Built and tested through the `logos-workspace` `ws` CLI.

> Status: scaffolding. See the architecture plan for scope and phases.
