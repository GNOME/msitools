namespace Wixl {

    class WixBuilder: WixElementVisitor {

        public WixBuilder (WixRoot root) {
            this.root = root;
        }

        WixRoot root;
        MsiDatabase db;
        List<WixFile> files;
        List<WixMedia> medias;

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
            if (db.table_file.records.length () > 0)
                add ("InstallFiles", 4000);
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

        public void build_cabinet () throws GLib.Error {
            var sequence = 0;

            foreach (var m in medias) {
                var folder = new GCab.Folder (GCab.Compression.MSZIP);

                foreach (var f in files) {
                    if (f.DiskId != m.Id)
                        continue;

                    folder.add_file (new GCab.File.with_file (f.Id, File.new_for_path (f.Name)), false);
                    var rec = f.record;
                    sequence += 1;
                    MsiTableFile.set_sequence (rec, sequence);
                }

                var cab = new GCab.Cabinet ();
                cab.add_folder (folder);
                var output = new MemoryOutputStream (null, realloc, free);
                cab.write (output, null, null, null);
                var input = new MemoryInputStream.from_data (output.get_data ()[0:output.data_size], null);
                if (parse_yesno (m.EmbedCab))
                    db.table_streams.add (m.Cabinet, input, output.data_size);

                db.table_media.set_last_sequence (m.record, sequence);
            }
        }

        public MsiDatabase build () throws GLib.Error {
            db = new MsiDatabase ();

            root.accept (this);
            sequence_actions ();
            build_cabinet ();

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
            var cabinet = media.Cabinet;

            medias.append (media);
            if (parse_yesno (media.EmbedCab))
                cabinet = "#" + cabinet;

            var rec = db.table_media.add (media.Id, media.DiskPrompt, cabinet);
            media.record = rec;
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

        [Flags]
        enum ComponentAttribute {
            LOCAL_ONLY = 0,
            SOURCE_ONLY,
            OPTIONAL,
            REGISTRY_KEY_PATH,
            SHARED_DLL_REF_COUNT,
            PERMANENT,
            ODBC_DATA_SOURCE,
            TRANSITIVE,
            NEVER_OVERWRITE,
            64BIT,
            REGISTRY_REFLECTION,
            UNINSTALL_ON_SUPERSEDENCE,
            SHARED,
        }

        public override void visit_component (WixComponent comp) throws GLib.Error {
            var attr = 0;

            if (comp.key is WixRegistryValue)
                attr |= ComponentAttribute.REGISTRY_KEY_PATH;

            if (comp.parent.get_type () == typeof (WixDirectory)) {
                var parent = comp.parent as WixDirectory;
                db.table_component.add (comp.Id, add_braces (comp.Guid), parent.Id, attr,
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

        [Flags]
        enum FileAttribute {
            READ_ONLY = 1 << 0,
            HIDDEN = 1 << 1,
            SYSTEM = 1 << 2,
            VITAL = 1 << 9,
            CHECKSUM = 1 << 10,
            PATCH_ADDED = 1 << 11,
            NON_COMPRESSED = 1 << 12,
            COMPRESSED = 1 << 13
        }

        public override void visit_file (WixFile file) throws GLib.Error {
            return_if_fail (file.DiskId == "1");
            files.append (file);

            var comp = file.parent as WixComponent;
            var gfile = File.new_for_path (file.Name);
            var info = gfile.query_info ("standard::*", 0, null);
            var attr = FileAttribute.VITAL;

            var rec = db.table_file.add (file.Id, comp.Id, file.Name, (int)info.get_size (), attr);
            file.record = rec;

            visit_key_element (file);
        }
    }

} // Wixl
