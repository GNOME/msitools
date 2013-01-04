namespace Wixl {

    class WixBuilder: WixElementVisitor {
        WixRoot root;
        MsiDatabase db;

        public WixBuilder (WixRoot root) {
            this.root = root;
        }

        delegate void AddSequence (string action, int sequence) throws GLib.Error;

        void sequence_actions () throws GLib.Error {
            AddSequence add = (action, sequence) => {
                db.table_admin_execute_sequence.add (action, sequence);
            };
            add ("CostInitialize", 800);
            add ("FileCost", 900);
            add ("CostFinalize", 1000);
            add ("InstallValidate", 1400);
            add ("InstallInitialize", 1500);
            add ("InstallAdminPackage", 3900);
            add ("InstallFiles", 4000);
            add ("InstallFinalize", 6600);

            add = (action, sequence) => {
                db.table_admin_ui_sequence.add (action, sequence);
            };
            add ("CostInitialize", 800);
            add ("FileCost", 900);
            add ("CostFinalize", 1000);
            add ("ExecuteAction", 1300);

            add = (action, sequence) => {
                db.table_advt_execute_sequence.add (action, sequence);
            };
            add ("CostInitialize", 800);
            add ("CostFinalize", 1000);
            add ("InstallValidate", 1400);
            add ("InstallInitialize", 1500);
            add ("PublishFeatures", 6300);
            add ("PublishProduct", 6400);
            add ("InstallFinalize", 6600);

            add = (action, sequence) => {
                db.table_install_execute_sequence.add (action, sequence);
            };
            add ("ValidateProductID", 700);
            add ("CostInitialize", 800);
            add ("FileCost", 900);
            add ("CostFinalize", 1000);
            add ("InstallValidate", 1400);
            add ("InstallInitialize", 1500);
            add ("ProcessComponents", 1600);
            add ("UnpublishFeatures", 1800);
            if (db.table_registry.records.length () > 0) {
                add ("RemoveRegistryValues", 2600);
                add ("WriteRegistryValues", 2600);
            }
            if (db.table_remove_file.records.length () > 0)
                add ("RemoveFiles", 3500);
            add ("RegisterUser", 6000);
            add ("RegisterProduct", 6100);
            add ("PublishFeatures", 6300);
            add ("PublishProduct", 6400);
            add ("InstallFinalize", 6600);

            add = (action, sequence) => {
                db.table_install_ui_sequence.add (action, sequence);
            };
            add ("ValidateProductID", 700);
            add ("CostInitialize", 800);
            add ("FileCost", 900);
            add ("CostFinalize", 1000);
            add ("ExecuteAction", 1300);
        }

        public MsiDatabase build () throws GLib.Error {
            db = new MsiDatabase ();
            root.accept (this);
            sequence_actions ();
            return db;
        }

        public override void visit_product (WixProduct product) throws GLib.Error {
            db.info.set_codepage (int.parse (product.Codepage));
            db.info.set_author (product.Manufacturer);

            db.table_property.add ("Manufacturer", product.Manufacturer);
            db.table_property.add ("ProductLanguage", product.Codepage);
            db.table_property.add ("ProductCode", add_braces (product.Id));
            db.table_property.add ("ProductName", product.Name);
            db.table_property.add ("ProductVersion", product.Version);
            db.table_property.add ("UpgradeCode", add_braces (product.UpgradeCode));
        }

        public override void visit_package (WixPackage package) throws GLib.Error {
            db.info.set_keywords (package.Keywords);
            db.info.set_subject (package.Description);
            db.info.set_comments (package.Comments);
        }

        public override void visit_icon (WixIcon icon) throws GLib.Error {
            db.table_icon.add (icon.Id, icon.SourceFile);
        }

        public override void visit_property (WixProperty prop) throws GLib.Error {
            db.table_property.add (prop.Id, prop.Value);
        }

        public override void visit_media (WixMedia media) throws GLib.Error {
            db.table_media.add (media.Id, media.EmbedCab, media.DiskPrompt, "#" + media.Cabinet);
        }

        public override void visit_directory (WixDirectory dir) throws GLib.Error {
            if (dir.parent.get_type () == typeof (WixProduct)) {
                if (dir.Id != "TARGETDIR")
                    throw new Wixl.Error.FAILED ("Invalid root directory");
                db.table_directory.add (dir.Id, null, dir.Name);
            } else if (dir.parent.get_type () == typeof (WixDirectory)) {
                var parent = dir.parent as WixDirectory;
                db.table_directory.add (dir.Id, parent.Id, dir.Name);
            } else
                warning ("unhandled parent type %s", dir.parent.name);
        }

        public override void visit_component (WixComponent comp) throws GLib.Error {
            if (comp.parent.get_type () == typeof (WixDirectory)) {
                var parent = comp.parent as WixDirectory;
                db.table_component.add (comp.Id, add_braces (comp.Guid), parent.Id, 0,
                                        comp.key != null ? comp.key.Id : null);
            } else
                warning ("unhandled parent type %s", comp.parent.name);
        }

        public override void visit_feature (WixFeature feature) throws GLib.Error {
            db.table_feature.add (feature.Id, 2, int.parse (feature.Level), 0);
        }

        public override void visit_component_ref (WixComponentRef ref) throws GLib.Error {
            if (ref.parent is WixFeature) {
                var parent = ref.parent as WixFeature;
                db.table_feature_components.add (parent.Id, @ref.Id);
            } else
                warning ("unhandled parent type %s", @ref.parent.name);
        }

        enum RemoveFileInstallMode {
            INSTALL = 1,
            UNINSTALL,
            BOTH
        }

        public override void visit_remove_folder (WixRemoveFolder rm) throws GLib.Error {
            var on = enum_from_string (typeof (RemoveFileInstallMode), rm.On);
            var comp = rm.parent as WixComponent;
            var dir = comp.parent as WixDirectory;

            db.table_remove_file.add (rm.Id, comp.Id, dir.Id, on);
        }

        void visit_key_element (WixKeyElement key) throws GLib.Error {
            var component = key.parent as WixComponent;

            if (!parse_yesno (key.KeyPath))
                return;

            component.key = key;
        }

        enum RegistryValueType {
            STRING,
            INTEGER,
            BINARY,
            EXPANDABLE,
            MULTI_STRING
        }

        enum RegistryRoot {
            HKCR,
            HKCU,
            HKLM,
            HKU,
            HKMU
        }

        public override void visit_registry_value (WixRegistryValue reg) throws GLib.Error {
            var comp = reg.parent as WixComponent;
            var value = reg.Value;
            var t = enum_from_string (typeof (RegistryValueType), reg.Type);
            var r = enum_from_string (typeof (RegistryRoot), reg.Root.down ());
            if (reg.Id == null) {
                reg.Id = generate_id ("reg", 4,
                                      comp.Id,
                                      reg.Root,
                                      reg.Key != null ? reg.Key.down () : null,
                                      reg.Name != null ? reg.Name.down () : null);
            }

            switch (t) {
            case RegistryValueType.STRING:
                value = value[0] == '#' ? "#" + value : value;
                break;
            }

            db.table_registry.add (reg.Id, r, reg.Key, comp.Id);

            visit_key_element (reg);
        }
    }

} // Wixl
