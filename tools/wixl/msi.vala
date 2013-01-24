namespace Wixl {

    public abstract class MsiTable: Object {
        public class string name;
        public List<Libmsi.Record> records;

        public class string sql_create;
        public class string sql_insert;

        public virtual void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, sql_create);
            query.execute ();

            if (sql_insert == null)
                return;

            query = new Libmsi.Query (db, sql_insert);
            foreach (var r in records)
                query.execute (r);
        }
    }

    class MsiTableIcon: MsiTable {
        static construct {
            name = "Icon";
            sql_create = "CREATE TABLE `Icon` (`Name` CHAR(72) NOT NULL, `Data` OBJECT NOT NULL PRIMARY KEY `Name`)";
            sql_insert = "INSERT INTO `Icon` (`Name`, `Data`) VALUES (?, ?)";
        }

        public void add (string id, string filename) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, id) ||
                !rec.load_stream (2, filename))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    public abstract class MsiTableSequence: MsiTable {
        public class MSIDefault.ActionFlags flags;

        private void add (string action, string? condition, int sequence) throws GLib.Error {
            var rec = new Libmsi.Record (3);

            if (!rec.set_string (1, action) ||
                (condition != null && !rec.set_string (2, condition)) ||
                !rec.set_int (3, sequence))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }

        protected class void set_sequence_table_name (string table) {
            name = table;
            sql_create = "CREATE TABLE `%s` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` INT PRIMARY KEY `Action`)".printf (table);
            sql_insert = "INSERT INTO `%s` (`Action`, `Condition`, `Sequence`) VALUES (?, ?, ?)".printf (table);
        }

        public class Action {
            public string name;
            public string? condition;
            public int sequence = -1;
            public WixAction? action;

            public bool visited = false;
            public bool incoming_deps = false;

            // Use it as a set, so value is not refcounted please vala
            public HashTable<Action, Action*> depends_on = new HashTable<Action, Action*> (direct_hash, direct_equal);

            public void add_dep (Action a) {
                hash_table_add (depends_on, a);
                a.incoming_deps = true;
            }
        }

        HashTable<string, Action> actions = new HashTable<string, Action> (str_hash, str_equal);

        void sort_topological_visit (Action action, ref List<Action> sorted) {
            if (action.visited)
                return;

            action.visited = true;

            Action dep;
            var it = HashTableIter <Action, Action*> (action.depends_on);
            while (it.next (null, out dep))
                sort_topological_visit (dep, ref sorted);

            sorted.append (action);
        }

        List<Action> sort_topological () {
            List<Action> sorted = null;

            Action action;
            var it = HashTableIter <string, Action> (actions);
            while (it.next (null, out action)) {
                if (action.incoming_deps)
                    continue;
                sort_topological_visit (action, ref sorted);
            }

            return sorted;
        }

        void add_implicit_deps () {
            CompareFunc<Action> cmp = (a, b) => {
                return a.sequence - b.sequence;
            };
            var list = actions.get_values ();
            list.sort (cmp);

            Action? prev = null;
            foreach (var a in list) {
                if (a.sequence == -1)
                    continue;
                if (prev != null)
                    a.add_dep (prev);
                prev = a;
            }
        }

        public void add_sorted_actions () throws GLib.Error {
            add_implicit_deps ();
            var sorted = sort_topological ();

            int sequence = 0;
            foreach (var action in sorted) {
                if (action.sequence == -1)
                    action.sequence = ++sequence;

                sequence = action.sequence;
                add (action.name, action.condition, action.sequence);
            }
        }

        public Action get_action (string name) {
            var action = actions.lookup (name);
            if (action != null)
                return action;

            action = new Action ();
            actions.insert (name, action);
            action.name = name;

            var default = MSIDefault.get_action_by_name (name);
            if (default != null)
                action.sequence = default.sequence;

            return action;
        }

        public void add_default_action (MSIDefault.Action action) {
            var default = MSIDefault.get_action (action);
            if (!(flags in default.flags))
                critical ("Action %s shouldn't be added in %s", default.name, name);

            var seq = actions.lookup (default.name);
            if (seq != null)
                return;

            seq = new Action ();
            actions.insert (default.name, seq);
            seq.name = default.name;
            seq.sequence = default.sequence;
            seq.condition = default.condition;
        }
    }

    class MsiTableAdminExecuteSequence: MsiTableSequence {
        static construct {
            set_sequence_table_name ("AdminExecuteSequence");
            flags = MSIDefault.ActionFlags.ADMIN_EXECUTE_SEQUENCE;
        }
    }

    class MsiTableAdminUISequence: MsiTableSequence {
        static construct {
            set_sequence_table_name ("AdminUISequence");
            flags = MSIDefault.ActionFlags.ADMIN_UI_SEQUENCE;
        }
    }

    class MsiTableAdvtExecuteSequence: MsiTableSequence {
        static construct {
            set_sequence_table_name ("AdvtExecuteSequence");
            flags = MSIDefault.ActionFlags.ADVT_EXECUTE_SEQUENCE;
        }
    }

    class MsiTableInstallExecuteSequence: MsiTableSequence {
        static construct {
            set_sequence_table_name ("InstallExecuteSequence");
            flags = MSIDefault.ActionFlags.INSTALL_EXECUTE_SEQUENCE;
        }
    }

    class MsiTableInstallUISequence: MsiTableSequence {
        static construct {
            set_sequence_table_name ("InstallUISequence");
            flags = MSIDefault.ActionFlags.INSTALL_UI_SEQUENCE;
        }
    }

    class MsiTableError: MsiTable {
        static construct {
            name = "Error";
            sql_create = "CREATE TABLE `Error` (`Error` INT NOT NULL, `Message` CHAR(0) LOCALIZABLE PRIMARY KEY `Error`)";
        }
    }

    class MsiTableFile: MsiTable {
        static construct {
            name = "File";
            sql_create = "CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` INT, `Sequence` LONG NOT NULL PRIMARY KEY `File`)";
            sql_insert = "INSERT INTO `File` (`File`, `Component_`, `FileName`, `FileSize`, `Attributes`, `Sequence`) VALUES (?, ?, ?, ?, ?, ?)";
        }

        public Libmsi.Record add (string File, string Component, string FileName, int FileSize, int Attributes, int Sequence = 0) throws GLib.Error {
            var rec = new Libmsi.Record (6);

            if (!rec.set_string (1, File) ||
                !rec.set_string (2, Component) ||
                !rec.set_string (3, FileName) ||
                !rec.set_int (4, FileSize) ||
                !rec.set_int (5, Attributes) ||
                !rec.set_int (6, Sequence))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);

            return rec;
        }

        public static bool set_sequence (Libmsi.Record rec, int Sequence) {
            return rec.set_int (6, Sequence);
        }
    }

    class MsiTableMedia: MsiTable {
        static construct {
            name = "Media";
            sql_create = "CREATE TABLE `Media` (`DiskId` INT NOT NULL, `LastSequence` LONG NOT NULL, `DiskPrompt` CHAR(64) LOCALIZABLE, `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`)";
            sql_insert = "INSERT INTO `Media` (`DiskId`, `LastSequence`, `DiskPrompt`, `Cabinet`) VALUES (?, ?, ?, ?)";
        }

        public bool set_last_sequence (Libmsi.Record rec, int last_sequence) {
            return rec.set_int (2, last_sequence);
        }

        public Libmsi.Record add (string DiskId, string? DiskPrompt, string Cabinet) throws GLib.Error {
            var rec = new Libmsi.Record (4);

            if (!rec.set_int (1, int.parse (DiskId)) ||
                !rec.set_int (2, 0) ||
                (DiskPrompt != null && !rec.set_string (3, DiskPrompt)) ||
                !rec.set_string (4, Cabinet))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
            return rec;
        }
    }

    class MsiTableUpgrade: MsiTable {
        static construct {
            name = "Upgrade";
            sql_create = "CREATE TABLE `Upgrade` (`UpgradeCode` CHAR(38) NOT NULL, `VersionMin` CHAR(20), `VersionMax` CHAR(20), `Language` CHAR(255), `Attributes` LONG NOT NULL, `Remove` CHAR(255), `ActionProperty` CHAR(72) NOT NULL PRIMARY KEY `UpgradeCode`, `VersionMin`, `VersionMax`, `Language`, `Attributes`)";
            sql_insert = "INSERT INTO `Upgrade` (`UpgradeCode`, `VersionMin`, `VersionMax`, `Attributes`, `ActionProperty`) VALUES (?, ?, ?, ?, ?)";
        }

        public void add (string UpgradeCode, string VersionMin, string? VersionMax, int Attributes, string ActionProperty) throws GLib.Error {
            var rec = new Libmsi.Record (5);

            if (!rec.set_string (1, UpgradeCode) ||
                !rec.set_string (2, VersionMin) ||
                (VersionMax != null && !rec.set_string (3, VersionMax)) ||
                !rec.set_int (4, Attributes) ||
                !rec.set_string (5, ActionProperty))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableLaunchCondition: MsiTable {
        static construct {
            name = "LaunchCondition";
            sql_create = "CREATE TABLE `LaunchCondition` (`Condition` CHAR(255) NOT NULL, `Description` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Condition`)";
            sql_insert = "INSERT INTO `LaunchCondition` (`Condition`, `Description`) VALUES (?, ?)";
        }

        public void add (string condition, string description) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, condition) ||
                !rec.set_string (2, description))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableProperty: MsiTable {
        static construct {
            name = "Property";
            sql_create = "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL LOCALIZABLE PRIMARY KEY `Property`)";
            sql_insert = "INSERT INTO `Property` (`Property`, `Value`) VALUES (?, ?)";
        }

        public void add (string prop, string value) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, prop) ||
                !rec.set_string (2, value))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableDirectory: MsiTable {
        static construct {
            name = "Directory";
            sql_create = "CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)";
            sql_insert = "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES (?, ?, ?)";
        }

        public void add (string Directory, string? Parent, string DefaultDir) throws GLib.Error {
            var rec = new Libmsi.Record (3);
            if (!rec.set_string (1, Directory) ||
                !rec.set_string (2, Parent) ||
                !rec.set_string (3, DefaultDir))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableComponent: MsiTable {
        static construct {
            name = "Component";
            sql_create = "CREATE TABLE `Component` (`Component` CHAR(72) NOT NULL, `ComponentId` CHAR(38), `Directory_` CHAR(72) NOT NULL, `Attributes` INT NOT NULL, `Condition` CHAR(255), `KeyPath` CHAR(72) PRIMARY KEY `Component`)";
            sql_insert = "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `KeyPath`) VALUES (?, ?, ?, ?, ?)";
        }

        public void add (string Component, string ComponentId, string Directory, int Attributes, string? KeyPath = null) throws GLib.Error {
            var rec = new Libmsi.Record (5);
            if (!rec.set_string (1, Component) ||
                !rec.set_string (2, ComponentId) ||
                !rec.set_string (3, Directory) ||
                !rec.set_int (4, Attributes) ||
                !rec.set_string (5, KeyPath))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableFeatureComponents: MsiTable {
        static construct {
            name = "FeatureComponents";
            sql_create = "CREATE TABLE `FeatureComponents` (`Feature_` CHAR(38) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Feature_`, `Component_`)";
            sql_insert = "INSERT INTO `FeatureComponents` (`Feature_`, `Component_`) VALUES (?, ?)";
        }

        public void add (string Feature, string Component) throws GLib.Error {
            var rec = new Libmsi.Record (2);
            if (!rec.set_string (1, Feature) ||
                !rec.set_string (2, Component))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableRegistry: MsiTable {
        static construct {
            name = "Registry";
            sql_create = "CREATE TABLE `Registry` (`Registry` CHAR(72) NOT NULL, `Root` INT NOT NULL, `Key` CHAR(255) NOT NULL LOCALIZABLE, `Name` CHAR(255) LOCALIZABLE, `Value` CHAR(0) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Registry`)";
            sql_insert = "INSERT INTO `Registry` (`Registry`, `Root`, `Key`, `Component_`, `Name`, `Value`) VALUES (?, ?, ?, ?, ?, ?)";
        }

        public void add (string Registry, int Root, string Key, string Component, string? Name, string? Value) throws GLib.Error {
            var rec = new Libmsi.Record (6);
            if (!rec.set_string (1, Registry) ||
                !rec.set_int (2, Root) ||
                !rec.set_string (3, Key) ||
                !rec.set_string (4, Component) ||
                (Name != null && !rec.set_string (5, Name)) ||
                (Value != null && !rec.set_string (6, Value)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableShortcut: MsiTable {
        static construct {
            name = "Shortcut";
            sql_create = "CREATE TABLE `Shortcut` (`Shortcut` CHAR(72) NOT NULL, `Directory_` CHAR(72) NOT NULL, `Name` CHAR(128) NOT NULL LOCALIZABLE, `Component_` CHAR(72) NOT NULL, `Target` CHAR(72) NOT NULL, `Arguments` CHAR(255), `Description` CHAR(255) LOCALIZABLE, `Hotkey` INT, `Icon_` CHAR(72), `IconIndex` INT, `ShowCmd` INT, `WkDir` CHAR(72), `DisplayResourceDLL` CHAR(255), `DisplayResourceId` INT, `DescriptionResourceDLL` CHAR(255), `DescriptionResourceId` INT PRIMARY KEY `Shortcut`)";
            sql_insert = "INSERT INTO `Shortcut` (`Shortcut`, `Directory_`, `Name`, `Component_`, `Target`, `Icon_`, `IconIndex`, `WkDir`, `Description`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
        }

        public Libmsi.Record add (string Shortcut, string Directory, string Name, string Component) throws GLib.Error {
            var rec = new Libmsi.Record (9);

            if (!rec.set_string (1, Shortcut) ||
                !rec.set_string (2, Directory) ||
                !rec.set_string (3, Name) ||
                !rec.set_string (4, Component))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);

            return rec;
        }

        public static void set_target (Libmsi.Record rec, string Target) throws GLib.Error {
            if (!rec.set_string (5, Target))
                throw new Wixl.Error.FAILED ("failed to set record");
        }

        public static void set_icon (Libmsi.Record rec, string Icon) throws GLib.Error {
            if (!rec.set_string (6, Icon))
                throw new Wixl.Error.FAILED ("failed to set record");
        }

        public static void set_icon_index (Libmsi.Record rec, int IconIndex) throws GLib.Error {
            if (!rec.set_int (7, IconIndex))
                throw new Wixl.Error.FAILED ("failed to set record");
        }

        public static void set_working_dir (Libmsi.Record rec, string WkDir) throws GLib.Error {
            if (!rec.set_string (8, WkDir))
                throw new Wixl.Error.FAILED ("failed to set record");
        }

        public static void set_description (Libmsi.Record rec, string Description) throws GLib.Error {
            if (!rec.set_string (9, Description))
                throw new Wixl.Error.FAILED ("failed to set record");
        }
    }

    class MsiTableRemoveFile: MsiTable {
        static construct {
            name = "RemoveFile";
            sql_create = "CREATE TABLE `RemoveFile` (`FileKey` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) LOCALIZABLE, `DirProperty` CHAR(72) NOT NULL, `InstallMode` INT NOT NULL PRIMARY KEY `FileKey`)";
            sql_insert = "INSERT INTO `RemoveFile` (`FileKey`, `Component_`, `DirProperty`, `InstallMode`) VALUES (?, ?, ?, ?)";
        }

        public void add (string FileKey, string Component, string DirProperty, int InstallMode) throws GLib.Error {
            var rec = new Libmsi.Record (4);
            if (!rec.set_string (1, FileKey) ||
                !rec.set_string (2, Component) ||
                !rec.set_string (3, DirProperty) ||
                !rec.set_int (4, InstallMode))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableFeature: MsiTable {
        static construct {
            name = "Feature";
            sql_create = "CREATE TABLE `Feature` (`Feature` CHAR(38) NOT NULL, `Feature_Parent` CHAR(38), `Title` CHAR(64) LOCALIZABLE, `Description` CHAR(255) LOCALIZABLE, `Display` INT, `Level` INT NOT NULL, `Directory_` CHAR(72), `Attributes` INT NOT NULL PRIMARY KEY `Feature`)";
            sql_insert = "INSERT INTO `Feature` (`Feature`, `Display`, `Level`, `Attributes`, `Feature_Parent`, `Title`, `Description`, `Directory_`) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        }

        public void add (string Feature, int Display, int Level, int Attributes, string? Parent = null, string? Title = null, string? Description = null, string? ConfigurableDirectory = null) throws GLib.Error {
            var rec = new Libmsi.Record (8);
            if (!rec.set_string (1, Feature) ||
                !rec.set_int (2, Display) ||
                !rec.set_int (3, Level) ||
                !rec.set_int (4, Attributes) ||
                (Parent != null && !rec.set_string (5, Parent)) ||
                (Title != null && !rec.set_string (6, Title)) ||
                (Description != null && !rec.set_string (7, Description)) ||
                (ConfigurableDirectory != null && !rec.set_string (8, ConfigurableDirectory)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableServiceControl : MsiTable {
        static construct {
            name = "ServiceControl";
            sql_create = "CREATE TABLE `ServiceControl` (`ServiceControl` CHAR(72) NOT NULL, `Name` CHAR(255) NOT NULL LOCALIZABLE, `Event` INT NOT NULL, `Arguments` CHAR(255) LOCALIZABLE, `Wait` INT, `Component_` CHAR(72) NOT NULL PRIMARY KEY `ServiceControl`)";
            sql_insert = "INSERT INTO `ServiceControl` (`ServiceControl`, `Name`, `Event`, `Arguments`, `Wait`, `Component_`) VALUES (?, ?, ?, ?, ?, ?)";
        }

        public void add (string ServiceControl, string Name, int Event, string? Arguments = null, bool? Wait = null, string Component) throws GLib.Error {
            var rec = new Libmsi.Record (6);
            if (!rec.set_string (1, ServiceControl) ||
                !rec.set_string (2, Name) ||
                !rec.set_int (3, Event) ||
                (Arguments != null && !rec.set_string (4, Arguments)) ||
                (Wait != null && !rec.set_int (5, Wait ? 1 : 0)) ||
                !rec.set_string (6, Component))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableServiceInstall : MsiTable {
        static construct {
            name = "ServiceInstall";
            sql_create = "CREATE TABLE `ServiceInstall` (`ServiceInstall` CHAR(72) NOT NULL, `Name` CHAR(255) NOT NULL, `DisplayName` CHAR(255) LOCALIZABLE, `ServiceType` LONG NOT NULL, `StartType` LONG NOT NULL, `ErrorControl` LONG NOT NULL, `LoadOrderGroup` CHAR(255), `Dependencies` CHAR(255), `StartName` CHAR(255), `Password` CHAR(255), `Arguments` CHAR(255), `Component_` CHAR(72) NOT NULL, `Description` CHAR(255) LOCALIZABLE PRIMARY KEY `ServiceInstall`)";
            sql_insert = "INSERT INTO `ServiceInstall` (`ServiceInstall`, `Name`, `DisplayName`, `ServiceType`, `StartType`, `ErrorControl`, `LoadOrderGroup`, `Dependencies`, `StartName`, `Password`, `Arguments`, `Component_`, `Description`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        }

        public void add (string ServiceInstall, string Name, string? DisplayName = null, int ServiceType, int StartType, int ErrorControl, string? LoadOrderGroup = null, string? Dependencies = null, string? StartName = null, string? Password = null, string? Arguments = null, string Component, string? Description = null) throws GLib.Error {
            var rec = new Libmsi.Record (13);
            if (!rec.set_string (1, ServiceInstall) ||
                !rec.set_string (2, Name) ||
                (DisplayName != null && !rec.set_string (3, DisplayName)) ||
                !rec.set_int (4, ServiceType) ||
                !rec.set_int (5, StartType) ||
                !rec.set_int (6, ErrorControl) ||
                (LoadOrderGroup != null && !rec.set_string (7, LoadOrderGroup)) ||
                (Dependencies != null && !rec.set_string (8, Dependencies)) ||
                (StartName != null && !rec.set_string (9, StartName)) ||
                (Password != null && !rec.set_string (10, Password)) ||
                (Arguments != null && !rec.set_string (11, Arguments)) ||
                !rec.set_string (12, Component) ||
                (Description != null && !rec.set_string (13, Description)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableAppSearch : MsiTable {
        static construct {
            name = "AppSearch";
            sql_create = "CREATE TABLE `AppSearch` (`Property` CHAR(72) NOT NULL, `Signature_` CHAR(72) NOT NULL PRIMARY KEY `Property`, `Signature_`)";
            sql_insert = "INSERT INTO `AppSearch` (`Property`, `Signature_`) VALUES (?, ?)";
        }

        public void add (string Property, string Signature) throws GLib.Error {
            var rec = new Libmsi.Record (2);
            if (!rec.set_string (1, Property) ||
                !rec.set_string (2, Signature))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableCustomAction : MsiTable {
        static construct {
            name = "CustomAction";
            sql_create = "CREATE TABLE `CustomAction` (`Action` CHAR(72) NOT NULL, `Type` INT NOT NULL, `Source` CHAR(72), `Target` CHAR(255), `ExtendedType` LONG PRIMARY KEY `Action`)";
            sql_insert = "INSERT INTO `CustomAction` (`Action`, `Type`, `Source`, `Target`) VALUES (?, ?, ?, ?)";
        }

        public void add (string Action, int Type, string Source, string Target) throws GLib.Error {
            var rec = new Libmsi.Record (4);
            if (!rec.set_string (1, Action) ||
                !rec.set_int (2, Type) ||
                !rec.set_string (3, Source) ||
                !rec.set_string (4, Target))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableRegLocator : MsiTable {
        static construct {
            name = "RegLocator";
            sql_create = "CREATE TABLE `RegLocator` (`Signature_` CHAR(72) NOT NULL, `Root` INT NOT NULL, `Key` CHAR(255) NOT NULL, `Name` CHAR(255), `Type` INT PRIMARY KEY `Signature_`)";
            sql_insert = "INSERT INTO `RegLocator` (`Signature_`, `Root`, `Key`, `Name`, `Type`) VALUES (?, ?, ?, ?, ?)";
        }

        public void add (string Signature, int Root, string Key, string Name, int Type) throws GLib.Error {
            var rec = new Libmsi.Record (5);
            if (!rec.set_string (1, Signature) ||
                !rec.set_int (2, Root) ||
                !rec.set_string (3, Key) ||
                !rec.set_string (4, Name) ||
                !rec.set_int (5, Type))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableValidation: MsiTable {
        static construct {
            name = "_Validation";
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
        }
    }

    class MsiTableStreams: MsiTable {
        static construct {
            name = "_Streams";
        }

        public void add (string name, GLib.InputStream input, size_t count) throws GLib.Error {
            var rec = new Libmsi.Record (2);
            if (!rec.set_string (1, name) ||
                !rec.set_stream (2, input, count))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "INSERT INTO `_Streams` (`Name`, `Data`) VALUES (?, ?)");
            foreach (var r in records)
                query.execute (r);
        }
    }

    class MsiSummaryInfo: Object {
        public Libmsi.SummaryInfo properties;

        construct {
            try {
                properties = new Libmsi.SummaryInfo (null, uint.MAX);
            } catch (GLib.Error error) {
                critical (error.message);
            }
        }

        public MsiSummaryInfo () {
        }

        public new void set_property (Libmsi.Property prop, Value value) throws GLib.Error {
            if (value.type () == typeof (string))
                properties.set_string (prop, (string) value);
            else if (value.type () == typeof (int))
                properties.set_int (prop, (int) value);
            else if (value.type () == typeof (uint64))
                properties.set_filetime (prop, (uint64) value);
            else
                critical ("Unhandled property type");
        }

            public void save (Libmsi.Database db) throws GLib.Error {
            properties.save (db);
        }

        public void set_codepage (int value) throws GLib.Error {
            set_property (Libmsi.Property.CODEPAGE, value);
        }

        public void set_author (string value) throws GLib.Error {
            set_property (Libmsi.Property.AUTHOR, value);
        }

        public void set_keywords (string value) throws GLib.Error {
            set_property (Libmsi.Property.KEYWORDS, value);
        }

        public void set_subject (string value) throws GLib.Error {
            set_property (Libmsi.Property.SUBJECT, value);
        }

        public void set_comments (string value) throws GLib.Error {
            set_property (Libmsi.Property.COMMENTS, value);
        }
    }

    class MsiDatabase: Object {
        public MsiSummaryInfo info;
        public MsiTableProperty table_property;
        public MsiTableIcon table_icon;
        public MsiTableMedia table_media;
        public MsiTableDirectory table_directory;
        public MsiTableComponent table_component;
        public MsiTableFeature table_feature;
        public MsiTableFeatureComponents table_feature_components;
        public MsiTableRemoveFile table_remove_file;
        public MsiTableRegistry table_registry;
        public MsiTableServiceControl table_service_control;
        public MsiTableServiceInstall table_service_install;
        public MsiTableFile table_file;
        public MsiTableAdminExecuteSequence table_admin_execute_sequence;
        public MsiTableAdminUISequence table_admin_ui_sequence;
        public MsiTableAdvtExecuteSequence table_advt_execute_sequence;
        public MsiTableInstallExecuteSequence table_install_execute_sequence;
        public MsiTableInstallUISequence table_install_ui_sequence;
        public MsiTableStreams table_streams;
        public MsiTableShortcut table_shortcut;
        public MsiTableUpgrade table_upgrade;
        public MsiTableLaunchCondition table_launch_condition;
        public MsiTableAppSearch table_app_search;
        public MsiTableCustomAction table_custom_action;
        public MsiTableRegLocator table_reg_locator;

        public HashTable<string, MsiTable> tables;

        construct {
            info = new MsiSummaryInfo ();
            try {
                info.set_property (Libmsi.Property.TITLE, "Installation Database");
                info.set_property (Libmsi.Property.TEMPLATE, "Intel;1033");
                info.set_property (Libmsi.Property.KEYWORDS, "Installer");
                info.set_property (Libmsi.Property.CODEPAGE, 1252);
                info.set_property (Libmsi.Property.UUID, get_uuid ("*"));
                info.set_property (Libmsi.Property.CREATED_TM,
                                   time_to_filetime (now ()));
                info.set_property (Libmsi.Property.LASTSAVED_TM,
                                   time_to_filetime (now ()));
                info.set_property (Libmsi.Property.VERSION, 100);
                info.set_property (Libmsi.Property.SOURCE, 2);
                info.set_property (Libmsi.Property.APPNAME, Config.PACKAGE_STRING);
                info.set_property (Libmsi.Property.SECURITY, 2);
            } catch (GLib.Error error) {
                critical (error.message);
            }

            tables = new HashTable<string, MsiTable> (str_hash, str_equal);
            table_property = new MsiTableProperty ();
            table_icon = new MsiTableIcon ();
            table_media = new MsiTableMedia ();
            table_directory = new MsiTableDirectory ();
            table_component = new MsiTableComponent ();
            table_feature = new MsiTableFeature ();
            table_feature_components = new MsiTableFeatureComponents ();
            table_remove_file = new MsiTableRemoveFile ();
            table_registry = new MsiTableRegistry ();
            table_service_control = new MsiTableServiceControl ();
            table_service_install = new MsiTableServiceInstall ();
            table_file = new MsiTableFile ();
            table_admin_execute_sequence = new MsiTableAdminExecuteSequence ();
            table_admin_ui_sequence = new MsiTableAdminUISequence ();
            table_advt_execute_sequence = new MsiTableAdvtExecuteSequence ();
            table_install_execute_sequence = new MsiTableInstallExecuteSequence ();
            table_install_ui_sequence = new MsiTableInstallUISequence ();
            table_streams = new MsiTableStreams ();
            table_shortcut = new MsiTableShortcut ();
            table_upgrade = new MsiTableUpgrade ();
            table_launch_condition = new MsiTableLaunchCondition ();
            table_app_search = new MsiTableAppSearch ();
            table_custom_action = new MsiTableCustomAction ();
            table_reg_locator = new MsiTableRegLocator ();

            foreach (var t in new MsiTable[] {
                    table_admin_execute_sequence,
                    table_admin_ui_sequence,
                    table_advt_execute_sequence,
                    table_install_execute_sequence,
                    table_install_ui_sequence,
                    table_directory,
                    table_media,
                    table_property,
                    table_icon,
                    table_component,
                    table_feature,
                    table_feature_components,
                    table_remove_file,
                    table_registry,
                    table_service_control,
                    table_service_install,
                    table_file,
                    table_streams,
                    table_shortcut,
                    table_upgrade,
                    table_launch_condition,
                    table_app_search,
                    table_custom_action,
                    table_reg_locator,
                    new MsiTableError (),
                    new MsiTableValidation ()
                }) {
                tables.insert (t.name, t);
            }
        }

        public MsiDatabase () {
            // empty ctor
        }

        public void build (string filename) throws GLib.Error {
            string name;
            MsiTable table;

            var db = new Libmsi.Database (filename, Libmsi.DbFlags.CREATE, null);
            info.save (db);

            var it = HashTableIter <string, MsiTable> (tables);
            while (it.next (out name, out table))
                table.create (db);

            db.commit ();
        }
    }

} // Wixl