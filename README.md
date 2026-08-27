# keystore_ui

The one place accounts are created, imported, exported and deleted.

Every other surface only reads which accounts exist. Wallets request signatures and render an
account picker; `signer_ui` shows what is being signed and takes the password to authorise it.
Neither can change the keystore, and that is enforced by the keystore itself — this module is
its configured **custodian**, and Tier D admits nobody else.

## Screens

| Screen | Keystore method |
|---|---|
| Accounts | `list_accounts`, `get_labels` *(both ungated)* |
| Create | `create_mnemonic` → shown once → confirmed → `import_mnemonic` |
| Import phrase | `import_mnemonic` |
| Import private key | `import_private_key` |
| Import vault | `import_keystore_json` |
| Rename | `set_label` |
| Change password | `change_password` |
| Export | `export_keystore_json` |
| Delete | `delete_account` |

## Two deliberate choices

**Secrets are returned from methods, never published as properties.** A generated phrase and
an exported vault come back from a SLOT and are cleared when the sheet closes. A PROP would be
cached in the shell process and broadcast to every connected replica — strictly worse than the
thing it would be carrying.

**Creating an account requires confirming three words of the phrase** before anything is
stored. An unwritten phrase is an unrecoverable account, and the moment to catch that is
before the account exists, not after.

If this build is not the configured custodian it says so once, plainly, instead of letting
every button fail one screen later.

## Building

```bash
nix build .#lgx-portable   # for Basecamp / logosctl
nix build .#install        # the dev variant, for logos-standalone-app
```
