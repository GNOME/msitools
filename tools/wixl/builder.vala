namespace Wixl {

    enum Arch {
        X86 = 0,
        INTEL = 0,
        IA64 = 1,
        INTEL64 = 1,
        X64,
        ARM64;

        public static Arch from_string (string s) throws GLib.Error {
            return enum_from_string<Arch> (s);
        }

        public string to_string () {
            switch (this) {
            case X86: return "x86";
            case IA64: return "ia64";
            case X64: return "x64";
            case ARM64: return "arm64";
            default: return "";
            }
        }
    }

    enum Extension {
        UI = 0;

        public static Extension from_string(string s) throws GLib.Error {
            return enum_from_string<Extension> (s);
        }

        public string[] get_files () {
            switch (this) {
                case UI:
                    // these files are assumed to be available and aren't
                    // explicitly loaded by any of the other UI files
                    return new string[] {
                        "CancelDlg",
                        "Common",
                    };
                default: return new string[] {};
            };
        }

        public string get_dir () {
            switch (this) {
                case UI: return "ui";
                default: return "";
            };
        }

        public string to_string () {
            switch (this) {
                case UI: return "ui";
                default: return "";
            };
        }
    }

    class WixBuilder: WixNodeVisitor, WixResolver {

        public WixBuilder (string[] includedirs, Arch arch, Extension[] extensions, string extdir) {
            add_path (".");
            foreach (var i in includedirs)
                this.includedirs.append (File.new_for_path (i));
            this.arch = arch;
            this.extensions = extensions;
            this.extdir = extdir;

            if (extensions.length > 0) {
                add_path (File.new_for_path (extdir).get_path ());
            }

            foreach (var ext in this.extensions) {
                try {
                    foreach (var file in ext.get_files ()) {
                        load_extension_file(ext, file);
                    }
                } catch (GLib.Error error) {
                    printerr (error.message + "\n");
                    Posix.exit (1);
                }
            }
        }

        WixRoot root;
        MsiDatabase db;
        HashTable<string, string> variables;
        List<File> includedirs;
        string extdir;
        Extension[] extensions;
        Arch arch;

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
                default:
                    break;
                }
            }
        }

        public void load_extension_file(Extension ext, string name) throws GLib.Error {
            if (ext in extensions) {
                load_file(File.new_build_filename (extdir, ext.get_dir(), name + ".wxs"));
            } else {
                throw new Wixl.Error.FAILED ("Can't load '%s': extension '%s' isn't enabled", name, ext.to_string());
            }
        }

        public void load_file (File file, bool preproc_only = false) throws GLib.Error {
            string data;
            FileUtils.get_contents (file.get_path (), out data);

            var p = new Preprocessor (variables, includedirs, arch);
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
            AddDefaultAction add = (action) => {
                table.add_default_action (action);
            };

            // AdminExecuteSequence
            table = db.table_admin_execute_sequence;
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
            add (MSIDefault.Action.CostInitialize);
            add (MSIDefault.Action.FileCost);
            add (MSIDefault.Action.CostFinalize);
            add (MSIDefault.Action.ExecuteAction);
            table.add_sorted_actions ();

            // AdvtExecuteSequence
            table = db.table_advt_execute_sequence;
            add (MSIDefault.Action.CostInitialize);
            add (MSIDefault.Action.CostFinalize);
            add (MSIDefault.Action.InstallValidate);
            add (MSIDefault.Action.InstallInitialize);
            add (MSIDefault.Action.PublishFeatures);
            add (MSIDefault.Action.PublishProduct);
            add (MSIDefault.Action.InstallFinalize);
            if (db.table_shortcut.records.length () > 0)
                add (MSIDefault.Action.CreateShortcuts);
            table.add_sorted_actions ();

            // InstallExecuteSequence
            table = db.table_install_execute_sequence;
            add (MSIDefault.Action.ValidateProductID);
            add (MSIDefault.Action.CostInitialize);
            add (MSIDefault.Action.FileCost);
            add (MSIDefault.Action.CostFinalize);
            add (MSIDefault.Action.InstallValidate);
            add (MSIDefault.Action.InstallInitialize);
            add (MSIDefault.Action.ProcessComponents);
            add (MSIDefault.Action.UnpublishFeatures);
            add (MSIDefault.Action.RegisterUser);
            add (MSIDefault.Action.RegisterProduct);
            add (MSIDefault.Action.PublishFeatures);
            add (MSIDefault.Action.PublishProduct);
            add (MSIDefault.Action.InstallFinalize);
            if (db.table_upgrade.records.length () > 0) {
                add (MSIDefault.Action.FindRelatedProducts);
                add (MSIDefault.Action.MigrateFeatureStates);
            }
            if (db.table_launch_condition.records.length () > 0)
                add (MSIDefault.Action.LaunchConditions);
            if (db.table_registry.records.length () > 0) {
                add (MSIDefault.Action.RemoveRegistryValues);
                add (MSIDefault.Action.WriteRegistryValues);
            }
            if (db.table_ini_file.records.length () > 0) {
                add (MSIDefault.Action.WriteIniValues);
            }
            if (db.table_remove_ini_file.records.length () > 0) {
                add (MSIDefault.Action.RemoveIniValues);
            }
            if (db.table_shortcut.records.length () > 0) {
                add (MSIDefault.Action.RemoveShortcuts);
                add (MSIDefault.Action.CreateShortcuts);
            }
            if (db.table_file.records.length () > 0) {
                add (MSIDefault.Action.RemoveFiles);
                add (MSIDefault.Action.InstallFiles);
            }
            if (db.table_remove_file.records.length () > 0)
                add (MSIDefault.Action.RemoveFiles);
            if (db.table_service_control.records.length () > 0) {
                add (MSIDefault.Action.StartServices);
                add (MSIDefault.Action.StopServices);
                add (MSIDefault.Action.DeleteServices);
            }
            if (db.table_service_install.records.length () > 0)
                add (MSIDefault.Action.InstallServices);
            if (db.table_create_folder.records.length () > 0) {
                add (MSIDefault.Action.RemoveFolders);
                add (MSIDefault.Action.CreateFolders);
            }
            if (db.table_app_search.records.length () > 0)
                add (MSIDefault.Action.AppSearch);
            if (db.table_environment.records.length () > 0) {
                add (MSIDefault.Action.RemoveEnvironmentStrings);
                add (MSIDefault.Action.WriteEnvironmentStrings);
            }
            table.add_sorted_actions ();

            // InstallUISequence
            table = db.table_install_ui_sequence;
            add (MSIDefault.Action.ValidateProductID);
            add (MSIDefault.Action.CostInitialize);
            add (MSIDefault.Action.FileCost);
            add (MSIDefault.Action.CostFinalize);
            add (MSIDefault.Action.ExecuteAction);
            if (db.table_upgrade.records.length () > 0) {
                add (MSIDefault.Action.FindRelatedProducts);
                add (MSIDefault.Action.MigrateFeatureStates);
            }
            if (db.table_launch_condition.records.length () > 0)
                add (MSIDefault.Action.LaunchConditions);
            if (db.table_app_search.records.length () > 0)
                add (MSIDefault.Action.AppSearch);
            table.add_sorted_actions ();
        }

        List<WixMedia> medias;
        private void hash_files () throws GLib.Error {
            foreach (var rec in db.table_file.records) {
                var f = rec.get_data<WixFile> ("wixfile");
                if (f.CompanionFile != null || f.DefaultVersion != null)
                    // versioned/companion files don't need rows in hash table
                    continue;
                
                var component = f.parent as WixComponent;
                if (component.in_feature.length () == 0)
                    continue;

                db.table_file_hash.add_with_file (f.Id, f.file);
            }
        }

        /*
         * CompanionFile attribute makes a file a companion child of another file, so referenced
         * file must exist and the companion child must not be the key path of its component
         * 
         * See: https://docs.microsoft.com/en-us/windows/win32/msi/companion-files
         */
        private void validate_companion_files () throws GLib.Error {
            var companionByFile = new HashTable<string, string?>(str_hash, str_equal);

            foreach (var rec in db.table_file.records) {
                var f = rec.get_data<WixFile> ("wixfile");
                companionByFile.insert (f.Id, f.CompanionFile);
                if (f.CompanionFile == null)
                    continue;

                var component = f.parent as WixComponent;
                if (component.key != f)
                    continue;

                throw new Wixl.Error.FAILED ("companion file %s is key of its component".printf (f.Id));
            }
            
            foreach (var companion in companionByFile.get_values ()) {
                if (companion == null || companionByFile.contains (companion))
                    continue;

                throw new Wixl.Error.FAILED ("file %s referenced by CompanionFile does not exist".printf (companion));
            }
        }

        private void build_cabinet () throws GLib.Error {
            var sequence = 0;

            foreach (var m in medias) {
                var folder = new GCab.Folder (GCab.Compression.MSZIP);

                foreach (var rec in db.table_file.records) {
                    var f = rec.get_data<WixFile> ("wixfile");
                    if (f.DiskId != m.Id)
                        continue;

                    var component = f.parent as WixComponent;
                    if (component.in_feature.length () == 0)
                        continue;

                    folder.add_file (new GCab.File.with_file (f.Id, f.file), false);
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
                if (sc.Target != null)
                    continue;
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
            db = new MsiDatabase (arch, extensions);

            foreach (var r in roots) {
                root = r;
                root.accept (this);
            }
            root = null;

            property_update ();
            shortcut_target ();
            sequence_actions ();
            hash_files ();
            validate_companion_files ();
            build_cabinet ();

            return db;
        }

        public override void visit_product (WixProduct product) throws GLib.Error {
            if (product.Codepage != null)
                db.info.set_codepage (int.parse (product.Codepage));

            if (product.Name != null && db.info.get_subject () == null)
                db.info.set_subject (product.Name);

            db.info.set_author (product.Manufacturer);
            if (db.info.get_comments () == null)
                db.info.set_comments ("This installer database contains the logic and data required to install %s.".printf (product.Name));

            db.table_property.add ("Manufacturer", product.Manufacturer);
            db.table_property.add ("ProductLanguage", product.Language);
            db.table_property.add ("ProductCode", get_uuid (product.Id));
            db.table_property.add ("ProductName", product.Name);
            db.table_property.add ("ProductVersion", product.Version);
            db.table_property.add ("UpgradeCode", add_braces (product.UpgradeCode));
        }

        [Flags]
        enum SourceFlags {
            SHORT_NAMES,
            COMPRESSED,
            ADMIN,
            NO_PRIVILEGES,
        }

        public override void visit_package (WixPackage package) throws GLib.Error {
            if (package.Comments != null)
                db.info.set_comments (package.Comments);

            if (package.Description != null)
                db.info.set_subject (package.Description);

            if (package.Keywords != null)
                db.info.set_keywords (package.Keywords);

            if (package.InstallerVersion != null) {
                var version = int.parse (package.InstallerVersion);

                if (arch != Arch.X86 && version < 200)
                    warning ("InstallerVersion >= 200 required for !x86 builds");

                db.info.set_property (Libmsi.Property.VERSION, version);
            }

            int source = SourceFlags.COMPRESSED;
            if (package.InstallScope != null) {
                if (package.InstallScope == "perUser") {
                    source |= SourceFlags.NO_PRIVILEGES;
                } else if (package.InstallScope == "perMachine") {
                    db.table_property.add ("ALLUSERS", "1");
                } else {
                    error ("invalid InstallScope value: %s", package.InstallScope);
                }
            }
            db.info.set_property (Libmsi.Property.SOURCE, source);
        }

        public override void visit_icon (WixIcon icon) throws GLib.Error {
            FileInfo info;

            icon.file = find_file (icon.SourceFile, out info);
            db.table_icon.add (icon.Id, icon.file.get_path ());
        }

        public override void visit_property (WixProperty prop) throws GLib.Error {
            if (prop.Value == null)
                return;

            db.table_property.add (prop.Id, prop.Value);
        }

        public override void visit_media (WixMedia media) throws GLib.Error {
            var cabinet = media.Cabinet;

            if (parse_yesno (media.EmbedCab))
                cabinet = "#" + cabinet;

            var rec = db.table_media.add (media.Id, media.DiskPrompt, cabinet);
            media.record = rec;
            medias.append (media);
        }

        public override void visit_directory (WixDirectory dir) throws GLib.Error {
            var defaultdir = dir.Name ?? ".";

            if (dir.parent is WixProduct || dir.parent is WixFragment) {
                if (dir.Id != "TARGETDIR")
                    throw new Wixl.Error.FAILED ("Invalid root directory");
                db.table_directory.add (dir.Id, null, defaultdir);
            } else if (dir.parent is WixDirectory || dir.parent is WixDirectoryRef) {
                var parent = resolve<WixDirectory> (dir.parent);
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

        WixKeyElement? component_default_key = null;
        int            component_children_count;

        public override void visit_component (WixComponent comp, VisitState state) throws GLib.Error {
            var attr = 0;
            if (state == VisitState.ENTER) {
                component_default_key = null;
                component_children_count = 0;
                return;
            }

            if (comp.key == null && component_default_key != null)
                comp.key = component_default_key;

            if (comp.key is WixRegistryValue)
                attr |= ComponentAttribute.REGISTRY_KEY_PATH;

            var dir = get_directory (comp);
            string uuid;

            if (comp.Guid == "*")
                uuid = uuid_from_name (comp.full_path (this));
            else if (comp.Guid == "")
                uuid = null; // unpatchable and not uninstallable component
            else
                uuid = get_uuid (comp.Guid);

            if (comp.Win64 != null && parse_yesno (comp.Win64))
                attr |= ComponentAttribute.64BIT;
            else if (comp.Win64 == null && (arch == Arch.X64 || arch == Arch.IA64))
                attr |= ComponentAttribute.64BIT;

            if (comp.Permanent != null && parse_yesno (comp.Permanent))
                attr |= ComponentAttribute.PERMANENT;

            db.table_component.add (comp.Id, uuid, dir.Id, attr,
                                    comp.key != null ? comp.key.Id : null, comp.Condition);

        }

        enum FeatureDisplay {
            HIDDEN = 0,
            EXPAND,
            COLLAPSE;

            public static FeatureDisplay from_string(string s) throws GLib.Error {
                return enum_from_string<FeatureDisplay> (s);
            }
        }
        WixFeature? feature_root;
        int feature_display;

        enum AbsentValue {
            Allow = 0,
            Disallow;

            public static AbsentValue from_string(string s) throws GLib.Error {
                return enum_from_string<AbsentValue> (s);
            }
        }

        enum AllowAdvertiseValue {
            No = 0,
            System,
            Yes;

            public static AllowAdvertiseValue from_string(string s) throws GLib.Error {
                return enum_from_string<AllowAdvertiseValue> (s);
            }
        }

        enum InstallDefaultValue {
            FollowParent = 0,
            Local,
            Source;

            public static InstallDefaultValue from_string(string s) throws GLib.Error {
                return enum_from_string<InstallDefaultValue> (s);
            }
        }

        enum TypicalDefaultValue {
            Advertise = 0,
            Install;

            public static TypicalDefaultValue from_string(string s) throws GLib.Error {
                return enum_from_string<TypicalDefaultValue> (s);
            }
        }

        [Flags]
        enum FeatureAttributes {
            NONE = 0,
            FAVORSOURCE = 1,
            FOLLOWPARENT = 2,
            FAVORADVERTISE = 4,
            DISALLOWADVERTISE = 8,
            UIDISALLOWABSENT = 16,
            NOUNSUPPORTEDADVERTISE = 32,
        }

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
                    display = FeatureDisplay.from_string (feature.Display);
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
            int level = feature.Level != null ? int.parse (feature.Level) : 1;

            int attributes = 0;
            if (feature.InstallDefault != null) {
                switch(InstallDefaultValue.from_string(feature.InstallDefault)) {
                case InstallDefaultValue.FollowParent:
                    attributes |= FeatureAttributes.FOLLOWPARENT;
                    break;
                case InstallDefaultValue.Source:
                    attributes |= FeatureAttributes.FAVORSOURCE;
                    break;
                case InstallDefaultValue.Local:
                    break;
                }
            }

            if (feature.AllowAdvertise != null) {
                switch(AllowAdvertiseValue.from_string(feature.AllowAdvertise)) {
                case AllowAdvertiseValue.No:
                    attributes |= FeatureAttributes.DISALLOWADVERTISE;
                    break;
                case AllowAdvertiseValue.System:
                    attributes |= FeatureAttributes.NOUNSUPPORTEDADVERTISE;
                    break;
                case AllowAdvertiseValue.Yes:
                    break;
                }
            }

            if (feature.TypicalDefault != null) {
                switch(TypicalDefaultValue.from_string(feature.TypicalDefault)) {
                case TypicalDefaultValue.Advertise:
                    attributes |= FeatureAttributes.FAVORADVERTISE;
                    break;
                case TypicalDefaultValue.Install:
                    break;
                }
            }

            if (feature.Absent != null) {
                switch(AbsentValue.from_string(feature.Absent)) {
                case AbsentValue.Disallow:
                    attributes |= FeatureAttributes.UIDISALLOWABSENT;
                    break;
                case AbsentValue.Allow:
                    break;
                }
            }

            if ((attributes & (FeatureAttributes.DISALLOWADVERTISE | FeatureAttributes.FAVORADVERTISE)) == (FeatureAttributes.DISALLOWADVERTISE | FeatureAttributes.FAVORADVERTISE))
                throw new Wixl.Error.FAILED ("cannot set both Absent='disallow' and TypicalDefault='no'");

            db.table_feature.add (feature.Id, display, level, attributes, parent, feature.Title, feature.Description, feature.ConfigurableDirectory);

        }

        void feature_add_component (WixFeature feature, WixComponent component) throws GLib.Error {
            if (component.in_feature.find (feature) != null)
                return;
            component.in_feature.append (feature);
            db.table_feature_components.add (feature.Id, component.Id);
        }

        void feature_add_component_group (WixFeature feature, WixComponentGroup group) throws GLib.Error {
            foreach (var node in group.children) {
                var child = node as WixElement;
                if (child is WixComponentGroupRef) {
                    feature_add_component_group (feature, resolve<WixComponentGroup> (child));
                } else {
                    feature_add_component (feature, resolve<WixComponent> (child));
                }
            }
        }

        public override void visit_component_ref (WixComponentRef ref) throws GLib.Error {
            var component = resolve<WixComponent> (@ref);

            if (ref.parent is WixFeature) {
                feature_add_component (@ref.parent as WixFeature, component);
            } else if (ref.parent is WixComponentGroup) {
                // will be added in GroupRef
            } else
                warning ("unhandled parent type %s", @ref.parent.name);
        }

        public override void visit_component_group_ref (WixComponentGroupRef ref) throws GLib.Error {
            var group = resolve<WixComponentGroup> (@ref);

            if (ref.parent is WixFeature) {
                var feature = ref.parent as WixFeature;
                feature_add_component_group (feature, group);
            } else if (ref.parent is WixComponentGroup) {
                // is added by parent group
            } else
                warning ("unhandled parent type %s", @ref.parent.name);
        }

        enum InstallMode {
            INSTALL = 1,
            UNINSTALL,
            BOTH;

            public static InstallMode from_string(string s) throws GLib.Error {
                return enum_from_string<InstallMode> (s);
            }
        }

        WixDirectory get_directory (WixComponent comp) throws GLib.Error {
            if (comp.parent is WixComponentGroup) {
                var group = comp.parent as WixComponentGroup;
                return find_element<WixDirectory> (group.Directory);
            } else if (comp.parent is WixDirectory || comp.parent is WixDirectoryRef) {
                return resolve<WixDirectory> (comp.parent);
            } else
                error ("unhandled parent type %s", comp.parent.name);
        }

        public override void visit_remove_folder (WixRemoveFolder rm) throws GLib.Error {
            var on = InstallMode.from_string (rm.On);
            var comp = rm.parent as WixComponent;
            WixDirectory dir = get_directory (comp);

            db.table_remove_file.add (rm.Id, comp.Id, dir.Id, on, null);
        }

        void visit_key_element (WixKeyElement key, WixComponent? component = null) throws GLib.Error {
            if (component == null)
                component = key.parent as WixComponent;

            return_if_fail (component != null);

            if (component_children_count++ == 0)
                component_default_key = key;
            if (parse_yesno (key.KeyPath)) {
                component_default_key = null;
                if (component.key != null)
                    throw new Wixl.Error.FAILED ("multiple elements have keyPath='yes'");
                component.key = key;
            }
        }

        enum RegistryValueType {
            STRING,
            INTEGER,
            BINARY,
            EXPANDABLE,
            MULTI_STRING;

            public static RegistryValueType from_string(string s) throws GLib.Error {
                if (s == "string")
                    return STRING;
                if (s == "integer")
                    return INTEGER;
                if (s == "binary")
                    return BINARY;
                if (s == "expandable")
                    return EXPANDABLE;
                if (s == "multistring")
                    return MULTI_STRING;
                throw new Wixl.Error.FAILED ("Can't convert string to enum");
            }
        }

        enum RegistryRoot {
            HKCR,
            HKCU,
            HKLM,
            HKU,
            HKMU;

            public static RegistryRoot from_string(string s) throws GLib.Error {
                return enum_from_string<RegistryRoot> (s);
            }
        }

        public override void visit_registry_value (WixRegistryValue reg) throws GLib.Error {
            WixComponent comp;
            string reg_key = "";
            string reg_root = "";

            if (reg.parent is WixRegistryKey) {
                var regkey = reg.parent as WixRegistryKey;
                comp = regkey.parent as WixComponent;
                reg_key = regkey.Key + "\\" + reg.Key;
                reg_root = regkey.Root;
            } else if (reg.parent is WixComponent) {
                comp = reg.parent as WixComponent;
                reg_key = reg.Key;
            } else {
                warning ("unhandled parent kind");
                return;
            }

            if (reg.Root != null)
                reg_root = reg.Root;

            var value = reg.Value;
            var t = RegistryValueType.from_string (reg.Type);
            var r = RegistryRoot.from_string (reg_root.down ());
            if (reg.Id == null) {
                reg.Id = generate_id ("reg", 4,
                                      comp.Id,
                                      reg_root,
                                      reg_key,
                                      reg.Name != null ? reg.Name.down () : null);
            }

            switch (t) {
            case RegistryValueType.INTEGER:
                value = "#" + value;
                break;
            case RegistryValueType.STRING:
                value = value[0] == '#' ? "#" + value : value;
                break;
            case RegistryValueType.BINARY:
                value = value.has_prefix("#x") ? value : "#x" + value;
                break;
            case RegistryValueType.EXPANDABLE:
                value = value.has_prefix("#%") ? value : "#%" + value;
                break;
            default:
                break;
            }

            db.table_registry.add (reg.Id, r, reg_key, comp.Id, reg.Name, value);

            visit_key_element (reg, comp);
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

            if (Path.is_absolute (name)) {
                var file = File.new_for_path (name);
                info = file.query_info ("standard::*", 0, null);
                if (info != null)
                    return file;
            } else {
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
            }

            throw new Wixl.Error.FAILED ("Couldn't find file %s", name);
        }

        public override void visit_file (WixFile file) throws GLib.Error {
            file.DiskId = file.DiskId ?? "1";
            return_if_fail (file.DiskId == "1");

            var name = file.path_name ();
            var source = file.Source ?? name;
            var comp = file.parent as WixComponent;
            FileInfo info;
            file.file = find_file (source, out info);
            var attr = FileAttribute.VITAL;
            var version = file.CompanionFile ?? file.DefaultVersion;

            var rec = db.table_file.add (file.Id, comp.Id, name, (int)info.get_size (), attr, version);
            rec.set_data<WixFile> ("wixfile", file);

            visit_key_element (file);
        }

        public override void visit_remove_file (WixRemoveFile rf) throws GLib.Error {
            var on = InstallMode.from_string (rf.On);
            var comp = rf.parent as WixComponent;
            WixDirectory dir = get_directory (comp);

            db.table_remove_file.add (rf.Id, comp.Id, dir.Id, on, rf.Name);
        }

        enum IniFileAction {
            [Description (nick = "addLine")]
            addLine = 0,
            [Description (nick = "createLine")]
            createLine = 1,
            [Description (nick = "removeLine")]
            removeLine = 2,
            [Description (nick = "addTag")]
            addTag = 3,
            [Description (nick = "removeTag")]
            removeTag = 4;
            public static IniFileAction from_string(string s) throws GLib.Error {
                return enum_from_string<IniFileAction> (s);
            }
        }
        public override void visit_ini_file (WixIniFile inifile) throws GLib.Error {
            var component = inifile.parent as WixComponent;
            var action = IniFileAction.from_string (inifile.Action);

            if (action == IniFileAction.addLine || action == IniFileAction.createLine || action == IniFileAction.addTag)
                db.table_ini_file.add (inifile.Id, inifile.Name, inifile.Directory, inifile.Section, inifile.Key, inifile.Value, action, component.Id);
            else
                db.table_remove_ini_file.add (inifile.Id, inifile.Name, inifile.Directory, inifile.Section, inifile.Key, inifile.Value, action, component.Id);
        }

        public override void visit_shortcut (WixShortcut shortcut) throws GLib.Error {
            string? directory = shortcut.Directory;

            if (!parse_yesno (shortcut.Advertise, true))
                message ("unimplemented");

            var component = shortcut.get_component ();
            if (directory == null && shortcut.parent is WixComponent) {
                var dir = resolve<WixDirectory> (component.parent);
                directory = dir.Id;
            }

            var rec = db.table_shortcut.add (shortcut.Id, directory, shortcut.Name, component.Id);
            shortcut.record = rec;

            if (shortcut.Icon != null)
                MsiTableShortcut.set_icon (rec, shortcut.Icon);
            if (shortcut.IconIndex != null)
                MsiTableShortcut.set_icon_index (rec, int.parse (shortcut.IconIndex));
            if (shortcut.WorkingDirectory != null)
                MsiTableShortcut.set_working_dir (rec, shortcut.WorkingDirectory);
            if (shortcut.Target != null)
                MsiTableShortcut.set_target (rec, shortcut.Target);
            if (shortcut.Description != null)
                MsiTableShortcut.set_description (rec, shortcut.Description);
            if (shortcut.Arguments != null)
                MsiTableShortcut.set_arguments (rec, shortcut.Arguments);
        }

        public override void visit_sequence (WixSequence sequence) throws GLib.Error {
        }

        public override void visit_condition (WixCondition condition) throws GLib.Error {
            return_if_fail (condition.children.length () == 1);
            var text = condition.children.first ().data as WixText;

            if (condition.parent is WixFeature) {
                var level = condition.Level != null ? int.parse (condition.Level) : 0;
                db.table_condition.add (condition.parent.Id, level, text.Text.strip());
            } else if (condition.parent is WixComponent) {
                ((WixComponent)condition.parent).Condition = text.Text.strip();
            } else if (condition.parent is WixProduct)
                db.table_launch_condition.add (text.Text.strip(), condition.Message);
            else
                warning ("unhandled parent type %s", condition.parent.name);
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

            if (parse_yesno (version.MigrateFeatures))
                attributes |= UpgradeAttribute.MIGRATE_FEATURES;

            if (parse_yesno (version.OnlyDetect))
                attributes |= UpgradeAttribute.ONLY_DETECT;

            if (parse_yesno (version.IncludeMinimum, true))
                attributes |= UpgradeAttribute.VERSION_MIN_INCLUSIVE;

            if (parse_yesno (version.IncludeMaximum))
                attributes |= UpgradeAttribute.VERSION_MAX_INCLUSIVE;

            db.table_upgrade.add (get_uuid (upgrade.Id), version.Minimum, version.Maximum, attributes, version.Property);

            secureProperties += version.Property;
        }

        public override void visit_action (WixAction action) throws GLib.Error {
            var parent = action.parent as WixSequence;
            var table = db.tables.lookup (parent.name) as MsiTableSequence;

            if (action.name == "Custom")
                action.name = action.Action;
            if (action.name == "Show")
                action.name = action.Dialog;

            var node = table.get_action (action.name);

            if ( node.action != null ) {
                return;
            }

            node.action = action;

            if (action.OnExit != null) {
                if (action.OnExit == "success") node.sequence = -1;
                if (action.OnExit == "cancel") node.sequence = -2;
                if (action.OnExit == "error") node.sequence = -3;
                if (action.OnExit == "suspend") node.sequence = -4;
            } else {
                if (action.Sequence != null) {
                    node.sequence = int.parse (action.Sequence);
                }

                if (action.After != null) {
                    node.add_dep (table.get_action (action.After));
                }

                if (action.Before != null) {
                    var before = table.get_action (action.Before);
                    before.add_dep (node);
                }
            }

            // set condition from either an attribute or inner text
            if (action.Condition != null) {
                node.condition = action.Condition;
            } else if (action.children.length () > 0) {
                return_if_fail (action.children.length () == 1);
                var text = action.children.first ().data as WixText;
                node.condition = text.Text.strip();
            }
        }

        public override void visit_progid (WixProgId progid) throws GLib.Error {
            return_if_fail (!parse_yesno (progid.Advertise));

            var comp = progid.parent as WixComponent;
            var regid = generate_id ("reg", 2,
                                     comp.Id,
                                     progid.Id);

            db.table_registry.add (regid, 0, progid.Id, comp.Id, null, progid.Description);
            if (progid.Icon != null) {
                var key = progid.Id + "\\DefaultIcon";
                regid = generate_id ("reg", 2,
                                     comp.Id,
                                     key);
                db.table_registry.add (regid, 0, key, comp.Id, null,
                                    progid.IconIndex != null ? "[#%s],%s".printf (progid.Icon, progid.IconIndex) : "[#%s]".printf (progid.Icon));
            }
        }

        public override void visit_extension (WixExtension ext) throws GLib.Error {
            var progid = ext.parent as WixProgId;
            var comp = progid.parent as WixComponent;

            return_if_fail (!parse_yesno (progid.Advertise));

            var regid = generate_id ("reg", 3,
                                     comp.Id,
                                     ext.Id,
                                     "Content Type");

            db.table_registry.add (regid, 0, "." + ext.Id, comp.Id, "Content Type", ext.ContentType);

            regid = generate_id ("reg", 2,
                                 comp.Id,
                                 ext.Id);

            db.table_registry.add (regid, 0, "." + ext.Id, comp.Id, null, progid.Id);
        }

        public override void visit_verb (WixVerb verb) throws GLib.Error {
            return_if_fail (verb.Id == "open");

            var ext = verb.parent as WixExtension;
            var progid = ext.parent as WixProgId;
            var comp = progid.parent as WixComponent;

            var key = progid.Id + "\\shell\\open";
            var regid = generate_id ("reg", 2,
                                     comp.Id,
                                     key);

            db.table_registry.add (regid, 0, key, comp.Id, null, "Open");

            key += "\\command";
            regid = generate_id ("reg", 2,
                                 comp.Id,
                                 key);

            db.table_registry.add (regid, 0, key, comp.Id, null,
                                   verb.Argument != null ? "\"[#%s]\" %s".printf (verb.TargetFile, verb.Argument) : "\"[#%s]\"".printf (verb.TargetFile));
        }

        public override void visit_mime (WixMIME mime) throws GLib.Error {
            var ext = mime.parent as WixExtension;
            var progid = ext.parent as WixProgId;
            var comp = progid.parent as WixComponent;

            return_if_fail (!parse_yesno (progid.Advertise));

            var key = "MIME\\Database\\Content Type\\" + mime.ContentType;
            var regid = generate_id ("reg", 3,
                                     comp.Id,
                                     key,
                                     "Extension");

            db.table_registry.add (regid, 0, key, comp.Id, "Extension", "." + ext.Id);
        }

        public override void visit_create_folder (WixCreateFolder folder) throws GLib.Error {
            var component = folder.parent as WixComponent;
            var dir = get_directory (component);
            db.table_create_folder.add (dir.Id, component.Id);
        }

        public override void visit_fragment (WixFragment fragment) throws GLib.Error {
        }

        public override void visit_text (WixText text) throws GLib.Error {
        }

        [Flags]
        enum ServiceControlEvent {
            INSTALL_START = 1 << 0,
            INSTALL_STOP = 1 << 1,
            INSTALL_DELETE = 1 << 3,
            UNINSTALL_START = 1 << 4,
            UNINSTALL_STOP = 1 << 5,
            UNINSTALL_DELETE = 1 << 7,
        }

        private int install_mode_to_event(string modeString, ServiceControlEvent install, ServiceControlEvent uninstall) throws GLib.Error {
            InstallMode mode = InstallMode.from_string (modeString);
            int event = 0;

            if (mode == InstallMode.INSTALL || mode == InstallMode.BOTH)
                event |= install;
            if (mode == InstallMode.UNINSTALL || mode == InstallMode.BOTH)
                event |= uninstall;
            return event;
        }

        string ServiceArguments;

        public override void visit_service_argument (WixServiceArgument service_argument) throws GLib.Error {
            if (ServiceArguments == null)
                ServiceArguments = "";
            else
                ServiceArguments += "[~]";

            ServiceArguments += service_argument.get_text();
        }

        public override void visit_service_control (WixServiceControl service_control, VisitState state) throws GLib.Error {
            var comp = service_control.parent as WixComponent;
            bool? Wait = null;
            int event = 0;

            if (state == VisitState.ENTER) {
                ServiceArguments = null;
                return;
            }

            if (service_control.Wait != null)
                Wait = parse_yesno(service_control.Wait);

            if (service_control.Start != null)
                event |= install_mode_to_event(service_control.Start, ServiceControlEvent.INSTALL_START, ServiceControlEvent.UNINSTALL_START);
            if (service_control.Stop != null)
                event |= install_mode_to_event(service_control.Stop, ServiceControlEvent.INSTALL_STOP, ServiceControlEvent.UNINSTALL_STOP);
            if (service_control.Remove != null)
                event |= install_mode_to_event(service_control.Remove, ServiceControlEvent.INSTALL_DELETE, ServiceControlEvent.UNINSTALL_DELETE);

            db.table_service_control.add(service_control.Id, service_control.Name, event, ServiceArguments, Wait, comp.Id);
        }

        string ServiceDependencies;

        public override void visit_service_dependency (WixServiceDependency service_dependency) throws GLib.Error {
            if (parse_yesno (service_dependency.Group)) {
                ServiceDependencies += "+" + service_dependency.Id;
            } else {
                /* TODO: it must be the Name of a previously installed service.  */
                ServiceDependencies += service_dependency.Id;
            }
            ServiceDependencies += "[~]";
        }

        enum ServiceTypeAttribute {
            OWN_PROCESS = 1 << 4,
            SHARE_PROCESS = 1 << 5;

            public static ServiceTypeAttribute from_string(string s) throws GLib.Error {
                if (s == "ownProcess")
                    return OWN_PROCESS;
                if (s == "shareProcess")
                    return SHARE_PROCESS;
                throw new Wixl.Error.FAILED ("Can't convert string to enum");
            }
        }

        enum StartTypeAttribute {
            AUTO = 2,
            DEMAND = 3,
            DISABLED = 4;

            public static StartTypeAttribute from_string(string s) throws GLib.Error {
                return enum_from_string<StartTypeAttribute> (s);
            }
        }

        enum ErrorControlAttribute {
            IGNORE = 0,
            NORMAL = 1,
            CRITICAL = 3;

            public static ErrorControlAttribute from_string(string s) throws GLib.Error {
                return enum_from_string<ErrorControlAttribute> (s);
            }
        }

        public override void visit_service_install (WixServiceInstall service_install, VisitState state) throws GLib.Error {
            var comp = service_install.parent as WixComponent;
            int ServiceType;
            int StartType;
            int ErrorControl;
            string Description;

            if (state == VisitState.ENTER) {
                ServiceDependencies = "";
                return;
            }

            StartType = StartTypeAttribute.from_string (service_install.Start);
            ErrorControl = ErrorControlAttribute.from_string (service_install.ErrorControl);
            ServiceType = ServiceTypeAttribute.from_string (service_install.Type);

            if (parse_yesno (service_install.Interactive))
                ServiceType |= 1 << 8;

            if (ServiceDependencies == "")
                ServiceDependencies = null;
            else
                ServiceDependencies += "[~]";

            Description = service_install.Description;
            if (parse_yesno (service_install.EraseDescription))
                Description = "[~]";

            db.table_service_install.add(service_install.Id, service_install.Name, service_install.DisplayName, ServiceType, StartType, ErrorControl, service_install.LoadOrderGroup, ServiceDependencies, service_install.Account, service_install.Password, service_install.Arguments, comp.Id, Description);
        }


        enum RegistryType {
            DIRECTORY,
            FILE,
            RAW,
            64BIT = 0x10;

            public static RegistryType from_string (string s) throws GLib.Error {
                return enum_from_string<RegistryType> (s);
            }
        }

        public override void visit_registry_search (WixRegistrySearch search) throws GLib.Error {
            var property = search.parent as WixProperty;

            var root = RegistryRoot.from_string (search.Root.down ());
            var type = RegistryType.from_string (search.Type.down ());

            if (search.Win64 != null && parse_yesno (search.Win64))
                type |= RegistryType.64BIT;
            else if (search.Win64 == null && (arch == Arch.X64 || arch == Arch.IA64))
                type |= RegistryType.64BIT;

            db.table_app_search.add (property.Id, search.Id);
            db.table_reg_locator.add (search.Id, root, search.Key, search.Name, type);
        }

        [Flags]
        enum CustomActionType {
            DLL_BINARY = 1,
            EXE_BINARY = 2,
            JSCRIPT_BINARY = 5,
            VBSCRIPT_BINARY = 6,
            DLL_FILE = 17,
            EXE_FILE = 18,
            ERROR = 19,
            JSCRIPT_FILE = 21,
            VBSCRIPT_FILE = 22,
            EXE_DIR = 34,
            SET_DIR = 35,
            JSCRIPT_SEQUENCE = 37,
            VBSCRIPT_SEQUENCE = 38,
            EXE_PROPERTY = 50,
            SET_PROPERTY = 51,
            JSCRIPT_PROPERTY = 53,
            VBSCRIPT_PROPERTY = 54,
            // return processing
            CONTINUE = 0x40,
            ASYNC = 0x80,
            // scheduling
            FIRST_SEQUENCE = 0x100,
            ONCE_PER_PROCESS = 0x200,
            CLIENT_REPEAT = 0x300,
            // in-script
            IN_SCRIPT = 0x400,
            ROLLBACK = 0x100,
            COMMIT = 0x200,
            NO_IMPERSONATE = 0x800,
            TS_AWARE = 0x4000,
        }

        public override void visit_custom_action (WixCustomAction action) throws GLib.Error {
            CustomActionType type;
            string source, target;

            // FIXME: so many missing things here...
            if (action.DllEntry != null) {
                type = CustomActionType.DLL_BINARY;
                source = action.BinaryKey;
                target = action.DllEntry;
            } else if (action.JScriptCall != null) {
                type = CustomActionType.JSCRIPT_BINARY;
                source = action.BinaryKey;
                target = action.JScriptCall;
            } else if (action.ExeCommand != null && action.FileKey == null) {
                type = CustomActionType.EXE_PROPERTY;
                source = action.Property;
                target = action.ExeCommand;
            } else if (action.ExeCommand != null && action.FileKey != null) {
                if (find_element<WixFile>(action.FileKey) == null)
                    error ("file reference '%s' not defined", action.FileKey);

                type = CustomActionType.EXE_FILE;
                source = action.FileKey;
                target = action.ExeCommand;
            } else if (action.Property != null && action.Value != null) {
                type = CustomActionType.SET_PROPERTY;
                source = action.Property;
                target = action.Value;
            } else
                throw new Wixl.Error.FAILED ("Unsupported CustomAction");

            if (action.Execute == "firstSequence")
                type |= CustomActionType.FIRST_SEQUENCE;
            else if (action.Execute == "oncePerProcess")
                type |= CustomActionType.ONCE_PER_PROCESS;
            else if (action.Execute == "clientRepeat")
                type |= CustomActionType.CLIENT_REPEAT;
            else if (action.Execute == "deferred")
                type |= CustomActionType.IN_SCRIPT;
            else if (action.Execute == "rollback")
                type |= CustomActionType.IN_SCRIPT | CustomActionType.ROLLBACK;
            else if (action.Execute == "commit")
                type |= CustomActionType.IN_SCRIPT | CustomActionType.COMMIT;

            if (action.Return == "ignore")
                type |= CustomActionType.CONTINUE;
            else if (action.Return == "asyncWait")
                type |= CustomActionType.ASYNC;
            else if (action.Return == "asyncNoWait") {
                type |= CustomActionType.CONTINUE;
                type |= CustomActionType.ASYNC;
            }

            if ((type & CustomActionType.IN_SCRIPT) == CustomActionType.IN_SCRIPT && !parse_yesno (action.Impersonate))
                type |= CustomActionType.NO_IMPERSONATE;

            db.table_custom_action.add (action.Id, type, source, target);
        }

        public override void visit_binary (WixBinary binary) throws GLib.Error {
            FileInfo info;

            binary.file = find_file (binary.SourceFile, out info);
            db.table_binary.add (binary.Id, binary.file.get_path ());
        }

        public override void visit_major_upgrade (WixMajorUpgrade major) throws GLib.Error {
            var product = major.parent as WixProduct;

            var property = "WIX_DOWNGRADE_DETECTED";
            if (major.AllowDowngrades == "yes") {
                db.table_upgrade.add (get_uuid (product.UpgradeCode), product.Version, "", UpgradeAttribute.MIGRATE_FEATURES, property);
            } else {
                db.table_upgrade.add (get_uuid (product.UpgradeCode), product.Version, "", UpgradeAttribute.ONLY_DETECT, property);
            }
            secureProperties += property;

            if (major.AllowSameVersionUpgrades == "yes") {
                property = "WIX_SAME_VERSION_UPGRADE_DETECTED";
                db.table_upgrade.add (get_uuid (product.UpgradeCode), product.Version, product.Version, UpgradeAttribute.MIGRATE_FEATURES | UpgradeAttribute.VERSION_MIN_INCLUSIVE | UpgradeAttribute.VERSION_MAX_INCLUSIVE, property);
                secureProperties += property;
            }

            property = "WIX_UPGRADE_DETECTED";
            db.table_upgrade.add (get_uuid (product.UpgradeCode), "", product.Version, UpgradeAttribute.MIGRATE_FEATURES, property);
            secureProperties += property;

            if (major.DowngradeErrorMessage != null && major.AllowDowngrades != "yes") {
                db.table_launch_condition.add ("NOT WIX_DOWNGRADE_DETECTED", major.DowngradeErrorMessage);
            }

            var table = db.table_install_execute_sequence;
            var node = table.get_action ("RemoveExistingProducts");
            warn_if_fail (node.action == null);
            node.add_dep (table.get_action ("InstallValidate"));
        }

        public override void visit_media_template (WixMediaTemplate tmpl) throws GLib.Error {
            var media = new WixMedia ();
            media.EmbedCab = tmpl.EmbedCab;
            media.Cabinet = "cab1.cab";
            media.Id = "1";
            visit_media (media);
        }

        public override void visit_ui_ref (WixUIRef ref) throws GLib.Error {
            if (find_element<WixUI>(@ref.Id) == null) {
                load_extension_file(Extension.UI, @ref.Id);
            }
        }

        public override void visit_ui_text (WixUIText text) throws GLib.Error {
            db.table_ui_text.add (text.Id, text.Value);
        }

        public override void visit_text_element (WixTextElement text) throws GLib.Error {
            // set the Text value of the parent control
            var control = text.parent as WixControl;

            if (text.SourceFile != null) {
                // use the contents of the given file
                string data;
                File file = find_file (text.SourceFile, null);
                FileUtils.get_contents (file.get_path (), out data);
                control.Text = data;
            } else if (text.children.length () > 0) {
                // use the inner text from this element
                return_if_fail (text.children.length () == 1);
                var node = text.children.first ().data as WixText;
                control.Text = node.Text.strip();
            }
        }

        [Flags]
        enum TextStyleBits {
            BOLD = 1 << 0,
            ITALIC = 1 << 1,
            UNDERLINE = 1 << 2,
            STRIKE = 1 << 3,
        }

        public override void visit_text_style (WixTextStyle style) throws GLib.Error {
            int? color = null;
            TextStyleBits bits = 0;

            if (style.Blue != null || style.Green != null ||
                    style.Red != null) {

                color = 0;

                if (style.Blue != null)
                    color += 65536 * int.parse (style.Blue);
                if (style.Green != null)
                    color += 256 * int.parse (style.Green);
                if (style.Red != null)
                    color += int.parse (style.Red);
            }

            if (parse_yesno (style.Bold))
                bits |= TextStyleBits.BOLD;
            if (parse_yesno (style.Italic))
                bits |= TextStyleBits.ITALIC;
            if (parse_yesno (style.Underline))
                bits |= TextStyleBits.UNDERLINE;
            if (parse_yesno (style.Strike))
                bits |= TextStyleBits.STRIKE;

            db.table_text_style.add (style.Id, style.FaceName,
                    int.parse (style.Size), color, bits);
        }

        [Flags]
        enum DialogStyleBits {
            VISIBLE = 1 << 0,
            MODAL = 1 << 1,
            MINIMIZE = 1 << 2,
            SYS_MODAL = 1 << 3,
            KEEP_MODELESS = 1 << 4,
            TRACK_DISK_SPACE = 1 << 5,
            USE_CUSTOM_PALETTE = 1 << 6,
            RTLRO = 1 << 7,
            RIGHT_ALIGNED = 1 << 8,
            LEFT_SCROLL = 1 << 9,
            BIDI = RTLRO | RIGHT_ALIGNED | LEFT_SCROLL,
            ERROR = 1 << 16,
        }

        public override void visit_dialog (WixDialog dialog) throws GLib.Error {
            int hcenter = 50;
            int vcenter = 50;
            int attributes = 0;
            int width = int.parse (dialog.Width);
            int height = int.parse (dialog.Height);

            if (!parse_yesno (dialog.Hidden))
                attributes |= DialogStyleBits.VISIBLE;
            if (!parse_yesno (dialog.Modeless))
                attributes |= DialogStyleBits.MODAL;
            if (!parse_yesno (dialog.NoMinimize))
                attributes |= DialogStyleBits.MINIMIZE;
            if (parse_yesno (dialog.KeepModeless))
                attributes |= DialogStyleBits.KEEP_MODELESS;
            if (parse_yesno (dialog.TrackDiskSpace))
                attributes |= DialogStyleBits.TRACK_DISK_SPACE;
            if (parse_yesno (dialog.CustomPalette))
                attributes |= DialogStyleBits.USE_CUSTOM_PALETTE;
            if (parse_yesno (dialog.RightToLeft))
                attributes |= DialogStyleBits.RTLRO;
            if (parse_yesno (dialog.RightAligned))
                attributes |= DialogStyleBits.RIGHT_ALIGNED;
            if (parse_yesno (dialog.LeftScroll))
                attributes |= DialogStyleBits.LEFT_SCROLL;
            if (parse_yesno (dialog.ErrorDialog))
                attributes |= DialogStyleBits.ERROR;

            WixControl first = dialog.children.first().data as WixControl;

            db.table_dialog.add (dialog.Id, hcenter, vcenter, width, height,
                    attributes, dialog.Title, first.Id,
                    dialog.Default, dialog.Cancel);

            // do this after we know what all the children are, which means
            // keeping a reference to the record so it can be updated in place

            // loop over all the children and update the tab selection order
            // of the controls, ignoring some controls
            WixControl? firsttab = null;
            WixControl? prevtab = null;

            foreach (var child in dialog.children) {
                var control = child as WixControl;

                if (control.Type != "Bitmap" && control.Type != "CheckBox" &&
                        control.Type != "Edit" &&
                        control.Type != "PushButton" &&
                        control.Type != "RadioButtonGroup") {
                    continue;
                }

                if (control.TabSkip != null && parse_yesno (control.TabSkip)) {
                    continue;
                }

                if (firsttab == null) {
                    firsttab = control;
                }

                if (prevtab != null) {
                    db.table_control.set_next_control (prevtab.record, control.Id);
                }

                prevtab = control;
            }

            if (prevtab != null && firsttab != null && prevtab != firsttab) {
                db.table_control.set_next_control (prevtab.record, firsttab.Id);
            }
        }

        public override void visit_dialog_ref (WixDialogRef ref) throws GLib.Error {
            if (find_element<WixDialog>(@ref.Id) == null) {
                load_extension_file(Extension.UI, @ref.Id);
            }
        }

        [Flags]
        enum ControlAttributes {
            BIDI = 0xE0,
            ENABLED = 0x02,
            INDIRECT = 0x08,
            INTEGER_CONTROL = 0x10,
            LEFT_SCROLL = 0x80,
            RIGHT_ALIGNED = 0x40,
            RTLRO = 0x20,
            SUNKEN = 0x04,
            VISIBLE = 0x01,
            // Text
            FORMAT_SIZE = 0x080000,
            NO_PREFIX = 0x020000,
            NO_WRAP = 0x040000,
            PASSWORD = 0x200000,
            TRANSPARENT = 0x010000,
            USERS_LANGUAGE = 0x100000,
            // ProgressBar
            PROGRESS_95 = 0x010000,
            // Volume and Directory
            FIXED_VOLUME = 0x020000,
            REMOTE_VOLUME = 0x040000,
            // PictureButton
            BITMAP = 0x040000,
            FIXED_SIZE = 0x100000,
            ICON = 0x080000,
            ICON_SIZE_16 = 0x200000,
            ICON_SIZE_32 = 0x400000,
            ICON_SIZE_48 = 0x600000,
            // PushButton
            ELEVATION_SHIELD = 0x800000,
            // VolumeCostList
            SHOW_ROLLBACK_COST = 0x00400000,
        }

        public override void visit_control (WixControl control) throws GLib.Error {
            int attributes = 0;
            int x = 100;
            int y = 100;
            int width = 500;
            int height = 500;
            string? filename = null;
            string help;

            if (!(control.parent is WixDialog)) {
                error ("unhandled parent type %s", control.parent.name);
            }

            // static controls don't need to be enabled
            if (control.Type != "Line" && control.Type != "Bitmap" &&
                    control.Type != "Icon") {
                if (!parse_yesno (control.Disabled))
                    attributes |= ControlAttributes.ENABLED;
            }
            if (parse_yesno (control.Sunken))
                attributes |= ControlAttributes.SUNKEN;
            if (parse_yesno (control.Indirect))
                attributes |= ControlAttributes.INDIRECT;
            if (parse_yesno (control.Icon))
                attributes |= ControlAttributes.ICON;
            if (!parse_yesno (control.Hidden))
                attributes |= ControlAttributes.VISIBLE;

            // Text
            if (parse_yesno (control.NoPrefix))
                attributes |= ControlAttributes.NO_PREFIX;
            if (parse_yesno (control.Transparent))
                attributes |= ControlAttributes.TRANSPARENT;

            // ProgressBar
            if (parse_yesno (control.ProgressBlocks))
                attributes |= ControlAttributes.PROGRESS_95;

            // Volume and Directory
            if (parse_yesno (control.Fixed))
                attributes |= ControlAttributes.FIXED_VOLUME;

            // PictureButton
            if (control.IconSize != null) {
                if (control.IconSize == "16") {
                    attributes |= ControlAttributes.ICON_SIZE_16;
                } else if (control.IconSize == "32") {
                    attributes |= ControlAttributes.ICON_SIZE_32;
                } else if (control.IconSize == "48") {
                    attributes |= ControlAttributes.ICON_SIZE_48;
                }
            }
            if (parse_yesno (control.FixedSize))
                attributes |= ControlAttributes.FIXED_SIZE;

            // PushButton
            if (parse_yesno (control.ElevationShield))
                attributes |= ControlAttributes.ELEVATION_SHIELD;

            // VolumeCostList
            if (parse_yesno (control.ShowRollbackCost))
                attributes |= ControlAttributes.SHOW_ROLLBACK_COST;

            if (control.X != null)
                x = int.parse (control.X);
            if (control.Y != null)
                y = int.parse (control.Y);

            if (control.Width != null)
                width = int.parse (control.Width);
            if (control.Height != null)
                height = int.parse (control.Height);

            help = control.ToolTip + "|";

            if ( control.file != null ) {
                filename = control.file.get_path();
            }

            if (control.Default != null && parse_yesno (control.Default)) {
                if (control.parent is WixDialog) {
                    var parent = control.parent as WixDialog;
                    parent.Default = control.Id;
                }
            }

            if (control.Cancel != null && parse_yesno (control.Cancel)) {
                if (control.parent is WixDialog) {
                    var parent = control.parent as WixDialog;
                    parent.Cancel = control.Id;
                }
            }

            if (control.DefaultCondition != null) {
                var parent = control.parent as WixDialog;
                db.table_control_condition.add (parent.Id, control.Id,
                        "Default", control.DefaultCondition);
            }

            if (control.DisableCondition != null) {
                var parent = control.parent as WixDialog;
                db.table_control_condition.add (parent.Id, control.Id,
                        "Disable", control.DisableCondition);
            }

            if (control.EnableCondition != null) {
                var parent = control.parent as WixDialog;
                db.table_control_condition.add (parent.Id, control.Id,
                        "Enable", control.EnableCondition);
            }

            if (control.HideCondition != null) {
                var parent = control.parent as WixDialog;
                db.table_control_condition.add (parent.Id, control.Id,
                        "Hide", control.HideCondition);
            }

            if (control.ShowCondition != null) {
                var parent = control.parent as WixDialog;
                db.table_control_condition.add (parent.Id, control.Id,
                        "Show", control.ShowCondition);
            }

            var rec = db.table_control.add (control.parent.Id, control.Id,
                    control.Type, x, y, width, height, attributes,
                    control.Property, control.Text, filename, null);

            // save the record so we can update the control tab order later
            control.record = rec;

            // also attach the property to the relevant control table
            if ( control.Type == "CheckBox" ) {
                db.table_check_box.add (control.Property, control.CheckBoxValue);
            }
        }

        public override void visit_publish (WixPublish publish) throws GLib.Error {
            int order;
            string control;
            string dialog;
            string event;
            string argument = null;
            string condition = "1";

            if (publish.parent is WixUI) {
                control = publish.Control;
                dialog = publish.Dialog;

                if (find_element<WixDialog>(dialog) == null) {
                    load_extension_file(Extension.UI, dialog);
                }
            } else {
                control = publish.parent.Id;
                dialog = publish.parent.parent.Id;
            }

            if (publish.Property != null) {
                event = "[%s]".printf (publish.Property);
                if (publish.Value != null) {
                    argument = publish.Value;
                } else {
                    argument = "{}";
                }
            } else {
                event = publish.Event;
                argument = publish.Value;
            }

            if (publish.Condition != null) {
                condition = publish.Condition;
            }

            if (publish.Order != null) {
                // use the next integer if nested under control
                order = int.parse (publish.Order);
                WixPublish.order = order + 1;
            } else {
                // use the given value and update the next value
                order = WixPublish.order++;
            }

            db.table_control_event.add (dialog, control, event, argument,
                    condition, order);
        }

        public override void visit_radio_button (WixRadioButton radio) throws GLib.Error {
            string property;
            int order;
            WixRadioButtonGroup group = radio.parent as WixRadioButtonGroup;

            property = group.Property;
            order = group.order++;

            db.table_radio_button.add (property, order, radio.Value,
                    int.parse (radio.X), int.parse (radio.Y),
                    int.parse (radio.Width), int.parse (radio.Height),
                    radio.Text, radio.Help);
        }

        public override void visit_subscribe (WixSubscribe subscribe) throws GLib.Error {
            string control;
            string dialog;

            dialog = subscribe.parent.parent.Id;
            control = subscribe.parent.Id;

            db.table_event_mapping.add (dialog, control,
                    subscribe.Event, subscribe.Attribute);
        }

        public override void visit_progress_text (WixProgressText progress_text) throws GLib.Error {
            return_if_fail (progress_text.children.length () == 1);
            var text = progress_text.children.first ().data as WixText;

            db.table_action_text.add (progress_text.Action, text.Text.strip(), progress_text.Template);
        }

        enum EnvironmentAction {
            CREATE,
            SET,
            REMOVE;

            public static EnvironmentAction from_string(string s) throws GLib.Error {
                if (s == "create")
                    return CREATE;
                if (s == "set")
                    return SET;
                if (s == "remove")
                    return REMOVE;
                throw new Wixl.Error.FAILED ("Can't convert string to enum");
            }
        }

        enum EnvironmentPart {
            ALL,
            FIRST,
            LAST;

            public static EnvironmentPart from_string(string s) throws GLib.Error {
                if (s == null || s == "all")
                    return ALL;
                if (s == "first")
                    return FIRST;
                if (s == "last")
                    return LAST;
                throw new Wixl.Error.FAILED ("Can't convert string to enum");
            }
        }

        public override void visit_environment (WixEnvironment env) throws GLib.Error {
            string name_prefix = null, value = null;

            switch(EnvironmentAction.from_string(env.Action)) {
                case EnvironmentAction.CREATE:
                    name_prefix = "+";
                    break;
                case EnvironmentAction.SET:
                    name_prefix = "=";
                    break;
                case EnvironmentAction.REMOVE:
                    name_prefix = "!";
                    break;
            }

            if(parse_yesno(env.Permanent)) {
                name_prefix += "-";
            }

            if(parse_yesno(env.System)) {
                name_prefix += "*";
            }

            var name = name_prefix + env.Name;

            var separator = ";";
            if(env.Separator != null)
                separator = env.Separator;

            switch(EnvironmentPart.from_string(env.Part)) {
                case EnvironmentPart.ALL:
                    value = env.Value;
                    break;
                case EnvironmentPart.FIRST:
                    value = env.Value + separator + "[~]";
                    break;
                case EnvironmentPart.LAST:
                    value = "[~]" + separator + env.Value;
                    break;
            }
            var component = env.parent as WixComponent;
            db.table_environment.add(Uuid.string_random (), name, value, component.Id);
        }

        public override void visit_copy_file (WixCopyFile copy_file) throws GLib.Error {
            if (copy_file.parent is WixComponent) {
                WixComponent parent = copy_file.parent as WixComponent;
                if(copy_file.FileId != null && copy_file.FileId != "") {
                    if(copy_file.Delete != null && copy_file.Delete != "no") {
                        throw new Wixl.Error.FAILED ("Delete must be not specified or 'no' when referncing a FileId");   
                    }
                    WixComponent component = copy_file.parent as WixComponent;
                    db.table_duplicate_file.add(copy_file.Id, component.Id, copy_file.FileId, copy_file.DestinationName, copy_file.DestinationDirectory);
                } else {
                    if(copy_file.Delete != null && copy_file.Delete == "yes") {
                        db.table_move_file.add(copy_file.Id, parent.Id, copy_file.SourceName, copy_file.DestinationName, copy_file.SourceDirectory, copy_file.DestinationDirectory, 1);
                    } else {
                        db.table_move_file.add(copy_file.Id, parent.Id, copy_file.SourceName, copy_file.DestinationName, copy_file.SourceDirectory, copy_file.DestinationDirectory, 0);
                    }
                }
            } else if (copy_file.parent is WixFile) {
                if(copy_file.Delete != null && copy_file.Delete != "no") {
                    throw new Wixl.Error.FAILED ("Delete must be not specified or 'no' when nested under a File");   
                }
                WixFile parent = copy_file.parent as WixFile;
                WixComponent component = parent.parent as WixComponent;
                db.table_duplicate_file.add(copy_file.Id, component.Id, parent.Id, copy_file.DestinationName, copy_file.DestinationDirectory);
            }
        }
    }

} // Wixl
