namespace Wixl {

    class WixBuilder: WixNodeVisitor {

        public WixBuilder () {
            add_path (".");
        }

        WixRoot root;
        MsiDatabase db;
        HashTable<string, string> variables;

        construct {
            variables = new HashTable<string, string> (str_hash, str_equal);
        }

        public void define_variable (string name, string value) {
            variables.insert (name, value);
        }

        List<File> path;
        public void add_path (string p) {
            var file = File.new_for_path (p);
            path.append (file);
        }

        List<WixRoot> roots;
        public void load_doc (Xml.Doc doc) throws GLib.Error {
            for (var child = doc.children; child != null; child = child->next) {
                switch (child->type) {
                case Xml.ElementType.ELEMENT_NODE:
                    if (child->name != "Wix")
                        warning ("unhandled node %s", child->name);
                    var root = new WixRoot ();
                    root.load (child);
                    roots.append (root);
                    break;
                }
            }
        }

        public void load_file (File file, bool preproc_only = false) throws GLib.Error {
            string data;
            FileUtils.get_contents (file.get_path (), out data);

            var p = new Preprocessor (variables);
            var doc = p.preprocess (data, file);
            if (preproc_only) {
                doc.dump_format (FileStream.fdopen (1, "w"));
                return;
            }

            load_doc (doc);
        }

        public G? find_element<G> (string Id) {
            foreach (var r in roots) {
                var e = r.find_element<G> (Id);
                if (e != null)
                    return e;
            }

            return null;
        }

        public G[] get_elements<G> () {
            G[] elems = {};
            foreach (var r in roots)
                elems = r.add_elements<G> (elems);

            return elems;
        }

        delegate void AddDefaultAction (MSIDefault.Action action) throws GLib.Error;

        private void sequence_actions () throws GLib.Error {
            MsiTableSequence? table = null;
            var flags = 0;
            AddDefaultAction add = (action) => {
                var default = MSIDefault.get_action (action);
                if (!(flags in default.flags))
                    critical ("Action %s shouldn't be added in this sequence", default.name);
                var seq = table.get_action (default.name);
                seq.sequence = default.sequence;
            };

            // AdminExecuteSequence
            table = db.table_admin_execute_sequence;
            flags = MSIDefault.ActionFlags.ADMIN_EXECUTE_SEQUENCE;
            add (MSIDefault.Action.CostInitialize);
            add (MSIDefault.Action.FileCost);
            add (MSIDefault.Action.CostFinalize);
            add (MSIDefault.Action.InstallValidate);
            add (MSIDefault.Action.InstallInitialize);
            add (MSIDefault.Action.InstallAdminPackage);
            add (MSIDefault.Action.InstallFiles);
            add (MSIDefault.Action.InstallFinalize);
            table.add_sorted_actions ();

            // AdminUISequence
            table = db.table_admin_ui_sequence;
            flags = MSIDefault.ActionFlags.ADMIN_UI_SEQUENCE;
            add (MSIDefault.Action.CostInitialize);
            add (MSIDefault.Action.FileCost);
            add (MSIDefault.Action.CostFinalize);
            add (MSIDefault.Action.ExecuteAction);
            table.add_sorted_actions ();

            // AdvtExecuteSequence
            table = db.table_advt_execute_sequence;
            flags = MSIDefault.ActionFlags.ADVT_EXECUTE_SEQUENCE;
            add (MSIDefault.Action.CostInitialize);
            add (MSIDefault.Action.CostFinalize);
            add (MSIDefault.Action.InstallValidate);
            add (MSIDefault.Action.InstallInitialize);
            if (db.table_shortcut.records.length () > 0)
                add (MSIDefault.Action.CreateShortcuts);
            add (MSIDefault.Action.PublishFeatures);
            add (MSIDefault.Action.PublishProduct);
            add (MSIDefault.Action.InstallFinalize);
            table.add_sorted_actions ();

            // InstallExecuteSequence
            table = db.table_install_execute_sequence;
            flags = MSIDefault.ActionFlags.INSTALL_EXECUTE_SEQUENCE;
            if (db.table_upgrade.records.length () > 0)
                add (MSIDefault.Action.FindRelatedProducts);
            if (db.table_launch_condition.records.length () > 0)
                add (MSIDefault.Action.LaunchConditions);
            add (MSIDefault.Action.ValidateProductID);
            add (MSIDefault.Action.CostInitialize);
            add (MSIDefault.Action.FileCost);
            add (MSIDefault.Action.CostFinalize);
            add (MSIDefault.Action.InstallValidate);
            add (MSIDefault.Action.InstallInitialize);
            add (MSIDefault.Action.ProcessComponents);
            add (MSIDefault.Action.UnpublishFeatures);
            if (db.table_registry.records.length () > 0)
                add (MSIDefault.Action.RemoveRegistryValues);
            if (db.table_shortcut.records.length () > 0)
                add (MSIDefault.Action.RemoveShortcuts);
            if (db.table_file.records.length () > 0 ||
                db.table_remove_file.records.length () > 0)
                add (MSIDefault.Action.RemoveFiles);
            if (db.table_file.records.length () > 0)
                add (MSIDefault.Action.InstallFiles);
            if (db.table_shortcut.records.length () > 0)
                add (MSIDefault.Action.CreateShortcuts);
            if (db.table_registry.records.length () > 0)
                add (MSIDefault.Action.WriteRegistryValues);
            add (MSIDefault.Action.RegisterUser);
            add (MSIDefault.Action.RegisterProduct);
            add (MSIDefault.Action.PublishFeatures);
            add (MSIDefault.Action.PublishProduct);
            add (MSIDefault.Action.InstallFinalize);
            table.add_sorted_actions ();

            // InstallUISequence
            table = db.table_install_ui_sequence;
            flags = MSIDefault.ActionFlags.INSTALL_UI_SEQUENCE;
            if (db.table_upgrade.records.length () > 0)
                add (MSIDefault.Action.FindRelatedProducts);
            if (db.table_launch_condition.records.length () > 0)
                add (MSIDefault.Action.LaunchConditions);
            add (MSIDefault.Action.ValidateProductID);
            add (MSIDefault.Action.CostInitialize);
            add (MSIDefault.Action.FileCost);
            add (MSIDefault.Action.CostFinalize);
            add (MSIDefault.Action.ExecuteAction);
            table.add_sorted_actions ();
        }

        private void build_cabinet () throws GLib.Error {
            var sequence = 0;
            var medias = get_elements<WixMedia> ();
            var files = get_elements<WixFile> ();

            foreach (var m in medias) {
                var folder = new GCab.Folder (GCab.Compression.MSZIP);

                foreach (var f in files) {
                    if (f.DiskId != m.Id)
                        continue;

                    folder.add_file (new GCab.File.with_file (f.Id, f.file), false);
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

        private void shortcut_target () throws GLib.Error {
            var shortcuts = get_elements<WixShortcut> ();

            foreach (var sc in shortcuts) {
                var component = sc.get_component ();
                var feature = component.in_feature.first ().data;
                MsiTableShortcut.set_target (sc.record, feature.Id);
            }
        }

        string[] secureProperties;

        public void property_update () throws GLib.Error {
            if (secureProperties.length != 0) {
                var prop = string.joinv (";", secureProperties);
                db.table_property.add ("SecureCustomProperties", prop);
            }
        }

        public MsiDatabase build () throws GLib.Error {
            db = new MsiDatabase ();

            foreach (var r in roots) {
                root = r;
                root.accept (this);
            }
            root = null;

            property_update ();
            shortcut_target ();
            sequence_actions ();
            build_cabinet ();

            return db;
        }

        public override void visit_product (WixProduct product) throws GLib.Error {
            if (product.Codepage != null)
                db.info.set_codepage (int.parse (product.Codepage));

            if (product.Name != null)
                db.info.set_subject (product.Name);

            db.info.set_author (product.Manufacturer);

            db.table_property.add ("Manufacturer", product.Manufacturer);
            db.table_property.add ("ProductLanguage", product.Language);
            db.table_property.add ("ProductCode", get_uuid (product.Id));
            db.table_property.add ("ProductName", product.Name);
            db.table_property.add ("ProductVersion", product.Version);
            db.table_property.add ("UpgradeCode", add_braces (product.UpgradeCode));
        }

        public override void visit_package (WixPackage package) throws GLib.Error {
            db.info.set_comments (package.Comments);

            if (package.Description != null)
                db.info.set_subject (package.Description);

            if (package.Keywords != null)
                db.info.set_keywords (package.Keywords);

            if (package.InstallerVersion != null)
                db.info.set_property (Libmsi.Property.VERSION, int.parse (package.InstallerVersion));

        }

        public override void visit_icon (WixIcon icon) throws GLib.Error {
            FileInfo info;

            icon.file = find_file (icon.SourceFile, out info);
            db.table_icon.add (icon.Id, icon.file.get_path ());
        }

        public override void visit_property (WixProperty prop) throws GLib.Error {
            db.table_property.add (prop.Id, prop.Value);
        }

        public override void visit_media (WixMedia media) throws GLib.Error {
            var cabinet = media.Cabinet;

            if (parse_yesno (media.EmbedCab))
                cabinet = "#" + cabinet;

            var rec = db.table_media.add (media.Id, media.DiskPrompt, cabinet);
            media.record = rec;
        }

        public override void visit_directory (WixDirectory dir) throws GLib.Error {
            var defaultdir = dir.Name ?? ".";

            if (dir.parent.get_type () == typeof (WixProduct)) {
                if (dir.Id != "TARGETDIR")
                    throw new Wixl.Error.FAILED ("Invalid root directory");
                db.table_directory.add (dir.Id, null, defaultdir);
            } else if (dir.parent.get_type () == typeof (WixDirectory)) {
                var parent = dir.parent as WixDirectory;
                db.table_directory.add (dir.Id, parent.Id, defaultdir);
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

        G? resolve<G> (WixElement element) throws GLib.Error {
            if (element.get_type () == typeof (G))
                return element;
            else if (element is WixElementRef) {
                var ref = element as WixElementRef<G>;
                if (ref.ref_type != typeof (G))
                    return null;
                if (ref.resolved != null)
                    return ref.resolved;
                ref.resolved = find_element<G> (element.Id);
                return ref.resolved;
            }

            throw new Wixl.Error.FAILED ("couldn't resolve %s", element.Id);
        }

        public override void visit_component (WixComponent comp) throws GLib.Error {
            var attr = 0;

            if (comp.key is WixRegistryValue)
                attr |= ComponentAttribute.REGISTRY_KEY_PATH;

            var parent = resolve<WixDirectory> (comp.parent);
            db.table_component.add (comp.Id, add_braces (comp.Guid), parent.Id, attr,
                                    comp.key != null ? comp.key.Id : null);

        }

        enum FeatureDisplay {
            HIDDEN = 0,
            EXPAND,
            COLLAPSE
        }
        WixFeature? feature_root;
        int feature_display;

        public override void visit_feature (WixFeature feature, VisitState state) throws GLib.Error {
            if (state == VisitState.ENTER && feature_root == null) {
                feature_display = 0;
                feature_root = feature;
            } else if (state == VisitState.LEAVE && feature_root == feature) {
                feature_root = null;
            }

            if (state != VisitState.ENTER)
                return;

            int display = FeatureDisplay.COLLAPSE;
            if (feature.Display != null) {
                try {
                    display = enum_from_string (typeof (FeatureDisplay), feature.Display);
                } catch (GLib.Error error) {
                    display = int.parse (feature.Display);
                    if (display != 0)
                        feature_display = display;
                }
            }

            switch (display) {
            case FeatureDisplay.COLLAPSE:
                display = feature_display = (feature_display | 1) + 1;
                break;
            case FeatureDisplay.EXPAND:
                display = feature_display = (feature_display + 1) | 1;
                break;
            }

            string? parent = (feature.parent is WixFeature) ? feature.parent.Id : null;

            db.table_feature.add (feature.Id, display, int.parse (feature.Level), 0, parent, feature.Title, feature.Description, feature.ConfigurableDirectory);

        }

        public override void visit_component_ref (WixComponentRef ref) throws GLib.Error {
            if (ref.parent is WixFeature) {
                var feature = ref.parent as WixFeature;

                var component = resolve<WixComponent> (@ref);
                component.in_feature.append (feature);
                db.table_feature_components.add (feature.Id, @ref.Id);
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

            if (component.key == null || parse_yesno (key.KeyPath))
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

        File? find_file (string name, out FileInfo info) throws GLib.Error {
            info = null;

            foreach (var p in path) {
                var file = p.get_child (name);
                try {
                    info = file.query_info ("standard::*", 0, null);
                    if (info != null)
                        return file;
                } catch (IOError error) {
                    if (error is IOError.NOT_FOUND)
                        continue;
                    throw error;
                }
            }

            throw new Wixl.Error.FAILED ("Couldn't find file %s", name);
        }

        public override void visit_file (WixFile file) throws GLib.Error {
            file.DiskId = file.DiskId ?? "1";
            return_if_fail (file.DiskId == "1");

            var name = file.Id;
            if (file.Name != null)
                name = file.Name;
            else if (file.Source != null)
                name = Path.get_basename (file.Source);

            var comp = file.parent as WixComponent;
            FileInfo info;
            file.file = find_file (name, out info);
            var attr = FileAttribute.VITAL;

            var rec = db.table_file.add (file.Id, comp.Id, name, (int)info.get_size (), attr);
            file.record = rec;

            visit_key_element (file);
        }

        public override void visit_shortcut (WixShortcut shortcut) throws GLib.Error {
            if (!parse_yesno (shortcut.Advertise))
                throw new Wixl.Error.FIXME ("unimplemented");

            var component = shortcut.get_component ();
            var rec = db.table_shortcut.add (shortcut.Id, shortcut.Directory, shortcut.Name, component.Id);
            shortcut.record = rec;

            if (shortcut.Icon != null)
                MsiTableShortcut.set_icon (rec, shortcut.Icon, int.parse (shortcut.IconIndex));
            if (shortcut.WorkingDirectory != null)
                MsiTableShortcut.set_working_dir (rec, shortcut.WorkingDirectory);
        }

        public override void visit_sequence (WixSequence sequence) throws GLib.Error {
        }

        public override void visit_condition (WixCondition condition) throws GLib.Error {
            return_if_fail (condition.children.length () == 1);
            var text = condition.children.first ().data as WixText;

            db.table_launch_condition.add (text.Text, condition.Message);
        }

        [Flags]
        enum UpgradeAttribute {
            MIGRATE_FEATURES = 1 << 0,
            ONLY_DETECT = 1 << 1,
            IGNORE_REMOVE_FAILURE = 1 << 2,
            VERSION_MIN_INCLUSIVE = 1 << 8,
            VERSION_MAX_INCLUSIVE = 1 << 9,
            LANGUAGES_EXCLUSIVE = 1 << 10
        }

        public override void visit_upgrade (WixUpgrade upgrade) throws GLib.Error {
        }

        public override void visit_upgrade_version (WixUpgradeVersion version) throws GLib.Error {
            var upgrade = version.parent as WixUpgrade;
            UpgradeAttribute attributes = 0;

            if (parse_yesno (version.OnlyDetect))
                attributes |= UpgradeAttribute.ONLY_DETECT;

            if (parse_yesno (version.IncludeMinimum, true))
                attributes |= UpgradeAttribute.VERSION_MIN_INCLUSIVE;

            db.table_upgrade.add (get_uuid (upgrade.Id), version.Minimum, version.Maximum, attributes, version.Property);

            secureProperties += version.Property;
        }

        public override void visit_action (WixAction action) throws GLib.Error {
            var parent = action.parent as WixSequence;
            var table = db.tables.lookup (parent.name) as MsiTableSequence;

            var node = table.get_action (action.name);
            warn_if_fail (node.action == null);
            node.action = action;

            if (action.After != null)
                node.add_dep (table.get_action (action.After));

            if (action.Before != null) {
                var before = table.get_action (action.Before);
                before.add_dep (node);
            }
        }

        public override void visit_create_folder (WixCreateFolder folder) throws GLib.Error {
        }

        public override void visit_fragment (WixFragment fragment) throws GLib.Error {
        }

        public override void visit_directory_ref (WixDirectoryRef ref) throws GLib.Error {
        }

        public override void visit_text (WixText text) throws GLib.Error {
        }
    }

} // Wixl
