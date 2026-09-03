{
  description = "Logos keystore_ui — the one surface that creates, imports, exports and deletes accounts.";

  inputs = {
    logos-module-builder.url = "github:logos-co/logos-module-builder";
    # Pinned to the branch until logos-evm-keystore-module#7 merges: this UI is the
    # custodian that gate names, and it needs the change_password/label contract that
    # lands with it. Move to the bare URL on merge.
    keystore_module = {
      url = "github:logos-co/logos-evm-keystore-module/feat/tier-d";
      inputs.logos-module-builder.follows = "logos-module-builder";
    };
  };

  # mkLogosQmlModule, NOT mkLogosModule: the generic builder compiles the plugin but never
  # assembles the QML, and the .lgx step then fails with "view file not found".
  outputs = inputs@{ logos-module-builder, ... }:
    logos-module-builder.lib.mkLogosQmlModule {
      src = ./.;
      configFile = ./metadata.json;
      flakeInputs = inputs;
    };
}
