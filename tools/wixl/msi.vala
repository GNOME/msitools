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

    /* https://docs.microsoft.com/en-us/windows/win32/msi/checkbox-table */
    class MsiTableCheckBox: MsiTable {
        static construct {
            name = "CheckBox";
            sql_create = "CREATE TABLE `CheckBox` (`Property` CHAR(72) NOT NULL, `Value` CHAR(64) PRIMARY KEY `Property`)";
            sql_insert = "INSERT INTO `CheckBox` (`Property`, `Value`) VALUES (?, ?)";
        }

        public void add (string property, string? value) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, property) ||
                (value != null && !rec.set_string (2, value)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    /* https://docs.microsoft.com/en-us/windows/win32/msi/eventmapping-table */
    class MsiTableEventMapping: MsiTable {
        static construct {
            name = "EventMapping";
            sql_create = "CREATE TABLE `EventMapping` (`Dialog_` CHAR(72) NOT NULL, `Control_` CHAR(50) NOT NULL, `Event` CHAR(50) NOT NULL, `Attribute` CHAR(50) NOT NULL PRIMARY KEY `Dialog_`, `Control_`, `Event`)";
            sql_insert = "INSERT INTO `EventMapping` (`Dialog_`, `Control_`, `Event`, `Attribute`) VALUES (?, ?, ?, ?)";
        }

        public void add (string dialog, string control, string event, string attribute) throws GLib.Error {
            var rec = new Libmsi.Record (4);

            if (!rec.set_string (1, dialog) ||
                !rec.set_string (2, control) ||
                !rec.set_string (3, event) ||
                !rec.set_string (4, attribute))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    /* https://docs.microsoft.com/en-us/windows/win32/msi/control-table */
    class MsiTableControl: MsiTable {
        static construct {
            name = "Control";
            sql_create = "CREATE TABLE `Control` (`Dialog_` CHAR(72) NOT NULL, `Control` CHAR(50) NOT NULL, `Type` CHAR(20) NOT NULL, `X` INT NOT NULL, `Y` INT NOT NULL, `Width` INT NOT NULL, `Height` INT NOT NULL, `Attributes` LONG, `Property` CHAR(72), `Text` CHAR(0) LOCALIZABLE, `Control_Next` CHAR(50), `Help` CHAR(50) LOCALIZABLE PRIMARY KEY `Dialog_`, `Control`)";
            sql_insert = "INSERT INTO `Control` (`Dialog_`, `Control`, `Type`, `X`, `Y`, `Width`, `Height`, `Attributes`, `Property`, `Text`, `Control_Next`, `Help`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        }

        // control in tab order needs updating after record has been added
        public bool set_next_control (Libmsi.Record rec, string next_control) {
            return rec.set_string (11, next_control);
        }

        public Libmsi.Record add (string dialog, string control, string type, int x, int y, int width, int height, int attributes, string? property, string? text, string? filename, string? help) throws GLib.Error {
            var rec = new Libmsi.Record (12);

            if (text != null) {
                if (!rec.set_string (10, text)) {
                    throw new Wixl.Error.FAILED ("failed to set record");
                }
            } else if (filename != null) {
                if (!rec.load_stream (10, filename)) {
                    throw new Wixl.Error.FAILED ("failed to set record");
                }
            }

            if (!rec.set_string (1, dialog) ||
                !rec.set_string (2, control) ||
                !rec.set_string (3, type) ||
                !rec.set_int (4, x) ||
                !rec.set_int (5, y) ||
                !rec.set_int (6, width) ||
                !rec.set_int (7, height) ||
                (attributes > 0 && !rec.set_int (8, attributes)) ||
                (property != null && !rec.set_string (9, property)) ||
                (help != null && !rec.set_string (12, help)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);

            // keep the record so that the next control can be updated
            return rec;
        }
    }

    /* https://docs.microsoft.com/en-us/windows/win32/msi/uitext-table */
    class MsiTableUIText: MsiTable {
        static construct {
            name = "UIText";
            sql_create = "CREATE TABLE `UIText` (`Key` CHAR(72) NOT NULL, `Text` CHAR(255) LOCALIZABLE PRIMARY KEY `Key`)";
            sql_insert = "INSERT INTO `UIText` (`Key`, `Text`) VALUES (?, ?)";
        }

        public void add (string key, string? text) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, key) ||
                (text != null && !rec.set_string (2, text)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    /* https://docs.microsoft.com/en-us/windows/win32/msi/textstyle-table */
    class MsiTableTextStyle: MsiTable {
        static construct {
            name = "TextStyle";
            sql_create = "CREATE TABLE `TextStyle` (`TextStyle` CHAR(72) NOT NULL, `FaceName` CHAR(32) NOT NULL, `Size` INT NOT NULL, `Color` LONG, `StyleBits` INT PRIMARY KEY `TextStyle`)";
            sql_insert = "INSERT INTO `TextStyle` (`TextStyle`, `FaceName`, `Size`, `Color`, `StyleBits`) VALUES (?, ?, ?, ?, ?)";
        }

        public void add (string textstyle, string facename, int size, int? color = null, int stylebits = 0) throws GLib.Error {
            var rec = new Libmsi.Record (5);

            if (!rec.set_string (1, textstyle) ||
                !rec.set_string (2, facename) ||
                !rec.set_int (3, size) ||
                (color != null && !rec.set_int (4, color)) ||
                (stylebits > 0 && !rec.set_int (5, stylebits)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    /* https://docs.microsoft.com/en-us/windows/win32/msi/dialog-table */
    class MsiTableDialog: MsiTable {
        static construct {
            name = "Dialog";
            sql_create = "CREATE TABLE `Dialog` (`Dialog` CHAR(72) NOT NULL, `HCentering` INT NOT NULL, `VCentering` INT NOT NULL, `Width` INT NOT NULL, `Height` INT NOT NULL, `Attributes` LONG, `Title` CHAR(128) LOCALIZABLE, `Control_First` CHAR(50) NOT NULL, `Control_Default` CHAR(50), `Control_Cancel` CHAR(50) PRIMARY KEY `Dialog`)";
            sql_insert = "INSERT INTO `Dialog` (`Dialog`, `HCentering`, `VCentering`, `Width`, `Height`, `Attributes`, `Title`, `Control_First`, `Control_Default`, `Control_Cancel`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        }

        public void add (string dialog, int hcenter, int vcenter, int width, int height, int attributes, string? title, string first, string? default, string? cancel) throws GLib.Error {
            var rec = new Libmsi.Record (10);

            if (!rec.set_string (1, dialog) ||
                !rec.set_int (2, hcenter) ||
                !rec.set_int (3, vcenter) ||
                !rec.set_int (4, width) ||
                !rec.set_int (5, height) ||
                !rec.set_int (6, attributes) ||
                (title != null && !rec.set_string (7, title)) ||
                !rec.set_string (8, first) ||
                (default != null && !rec.set_string (9, default)) ||
                (cancel != null && !rec.set_string (10, cancel)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    /* https://docs.microsoft.com/en-us/windows/win32/msi/controlevent-table */
    class MsiTableControlEvent: MsiTable {
        static construct {
            name = "ControlEvent";
            sql_create = "CREATE TABLE `ControlEvent` (`Dialog_` CHAR(72) NOT NULL, `Control_` CHAR(50) NOT NULL, `Event` CHAR(50) NOT NULL, `Argument` CHAR(255) NOT NULL, `Condition` CHAR(255), `Ordering` INT PRIMARY KEY `Dialog_`, `Control_`, `Event`, `Argument`, `Condition`)";
            sql_insert = "INSERT INTO `ControlEvent` (`Dialog_`, `Control_`, `Event`, `Argument`, `Condition`, `Ordering`) VALUES (?, ?, ?, ?, ?, ?)";
        }

        public void add (string dialog, string control, string event, string argument, string? condition, int? ordering) throws GLib.Error {
            var rec = new Libmsi.Record (6);

            if (!rec.set_string (1, dialog) ||
                !rec.set_string (2, control) ||
                !rec.set_string (3, event) ||
                !rec.set_string (4, argument) ||
                (condition != null && !rec.set_string (5, condition)) ||
                (ordering != null && !rec.set_int (6, ordering)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    /* https://docs.microsoft.com/en-us/windows/win32/msi/controlcondition-table */
    class MsiTableControlCondition: MsiTable {
        static construct {
            name = "ControlCondition";
            sql_create = "CREATE TABLE `ControlCondition` (`Dialog_` CHAR(72) NOT NULL, `Control_` CHAR(50) NOT NULL, `Action` CHAR(50) NOT NULL, `Condition` CHAR(255) NOT NULL PRIMARY KEY `Dialog_`, `Control_`, `Action`, `Condition`)";
            sql_insert = "INSERT INTO `ControlCondition` (`Dialog_`, `Control_`, `Action`, `Condition`) VALUES (?, ?, ?, ?)";
        }

        public void add (string dialog, string control, string action, string condition) throws GLib.Error {
            var rec = new Libmsi.Record (4);

            if (!rec.set_string (1, dialog) ||
                !rec.set_string (2, control) ||
                !rec.set_string (3, action) ||
                !rec.set_string (4, condition))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    /* https://docs.microsoft.com/en-us/windows/win32/msi/listbox-table */
    class MsiTableListBox: MsiTable {
        static construct {
            name = "ListBox";
            sql_create = "CREATE TABLE `ListBox` (`Property` CHAR(72) NOT NULL, `Order` INT NOT NULL, `Value` CHAR(64) NOT NULL, `Text` CHAR(64) LOCALIZABLE PRIMARY KEY `Property`, `Order`)";
            sql_insert = "INSERT INTO `ListBox` (`Property`, `Order`, `Value`, `Text`) VALUES (?, ?, ?, ?)";
        }
    }

    /* https://docs.microsoft.com/en-us/windows/win32/msi/radiobutton-table */
    class MsiTableRadioButton: MsiTable {
        static construct {
            name = "RadioButton";
            sql_create = "CREATE TABLE `RadioButton` (`Property` CHAR(72) NOT NULL, `Order` INT NOT NULL, `Value` CHAR(64) NOT NULL, `X` INT NOT NULL, `Y` INT NOT NULL, `Width` INT NOT NULL, `Height` INT NOT NULL, `Text` CHAR(0) LOCALIZABLE, `Help` CHAR(50) LOCALIZABLE PRIMARY KEY `Property`, `Order`)";
            sql_insert = "INSERT INTO `RadioButton` (`Property`, `Order`, `Value`, `X`, `Y`, `Width`, `Height`, `Text`, `Help`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
        }

        public void add (string property, int order, string value, int x, int y, int width, int height, string? text, string? help) throws GLib.Error {
            var rec = new Libmsi.Record (9);

            if (!rec.set_string (1, property) ||
                !rec.set_int (2, order) ||
                !rec.set_string (3, value) ||
                !rec.set_int (4, x) ||
                !rec.set_int (5, y) ||
                !rec.set_int (6, width) ||
                !rec.set_int (7, height) ||
                (text != null && !rec.set_string (8, text)) ||
                (help != null && !rec.set_string (9, help)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableFileHash: MsiTable {
        static construct {
            name = "MsiFileHash";
            sql_create = "CREATE TABLE `MsiFileHash` (`File_` CHAR(72) NOT NULL, `Options` INT NOT NULL, `HashPart1` LONG NOT NULL, `HashPart2` LONG NOT NULL, `HashPart3` LONG NOT NULL, `HashPart4` LONG NOT NULL PRIMARY KEY `File_`)";
            sql_insert = "INSERT INTO `MsiFileHash` (`File_`, `Options`, `HashPart1`, `HashPart2`, `HashPart3`, `HashPart4`) VALUES (?, ?, ?, ?, ?, ?)";
        }

        public void add (string file,
                         int hash1, int hash2, int hash3, int hash4,
                         int? options = 0) throws GLib.Error {
            var rec = new Libmsi.Record (6);

            if (!rec.set_string (1, file) ||
                !rec.set_int (2, options) ||
                !rec.set_int (3, hash1) ||
                !rec.set_int (4, hash2) ||
                !rec.set_int (5, hash3) ||
                !rec.set_int (6, hash4))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }

        public void add_with_file (string FileId, File file) throws GLib.Error {
            int hash1 = 0, hash2 = 0, hash3 = 0, hash4 = 0;

            compute_md5 (file, ref hash1, ref hash2, ref hash3, ref hash4);
            add (FileId, hash1, hash2, hash3, hash4);
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

    class MsiTableBinary: MsiTable {
        static construct {
            name = "Binary";
            sql_create = "CREATE TABLE `Binary` (`Name` CHAR(72) NOT NULL, `Data` OBJECT NOT NULL PRIMARY KEY `Name`)";
            sql_insert = "INSERT INTO `Binary` (`Name`, `Data`) VALUES (?, ?)";
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
            public int sequence = -999;
            public WixAction? action;

            public bool visited = false;
            public bool incoming_deps = false;

            // Use it as a set, so value is not refcounted please vala
            public HashTable<Action, Action*> depends_on = new HashTable<Action, Action*> (direct_hash, direct_equal);

            public void add_dep (Action a) {
                hash_table_add (depends_on, a);
                a.incoming_deps = true;
            }

            public int dep_max() {
                int ret = -1;
                var it = HashTableIter <Action, Action*> (depends_on);
                Action dep;
                while (it.next (null, out dep))
                    if (dep.sequence > ret)
                        ret = dep.sequence;
                return ret;
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

        // before sorting topologically, sort the actions so that the
        // one with the least max sequence dependency comes first.
        List<weak Action> sorted_actions() {
            CompareFunc<Action> cmp = (a, b) => {
                if (a.sequence == b.sequence) {
                    var a_dep = a.dep_max();
                    var b_dep = b.dep_max();
                    return a_dep - b_dep;
                }
                return a.sequence - b.sequence;
            };
            var list = actions.get_values ();
            list.sort(cmp);
            return list;
        }

        List<Action> sort_topological () {
            List<Action> sorted = null;

            foreach (var action in sorted_actions()) {
                if (action.incoming_deps)
                    continue;
                sort_topological_visit (action, ref sorted);
            }

            return sorted;
        }

        void add_implicit_deps () {
            Action? prev = null;
            foreach (var a in sorted_actions()) {
                if (a.sequence == -999)
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
                if (action.sequence == -999)
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
            sql_insert = "INSERT INTO `File` (`File`, `Component_`, `FileName`, `FileSize`, `Version`, `Attributes`, `Sequence`) VALUES (?, ?, ?, ?, ?, ?, ?)";
        }

        public Libmsi.Record add (string File, string Component, string FileName, int FileSize, int Attributes, string? Version = null, int Sequence = 1) throws GLib.Error {
            var rec = new Libmsi.Record (7);

            if (!rec.set_string (1, File) ||
                !rec.set_string (2, Component) ||
                !rec.set_string (3, FileName) ||
                !rec.set_int (4, FileSize) ||
                (Version != null && !rec.set_string (5, Version)) ||
                !rec.set_int (6, Attributes) ||
                !rec.set_int (7, Sequence))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);

            return rec;
        }

        public static bool set_sequence (Libmsi.Record rec, int Sequence) {
            return rec.set_int (7, Sequence);
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

        public void add (string UpgradeCode, string? VersionMin, string? VersionMax, int Attributes, string ActionProperty) throws GLib.Error {
            if (VersionMin == null && VersionMax == null)
                throw new Wixl.Error.FAILED ("VersionMin and VersionMax must not both be null");

            var rec = new Libmsi.Record (5);

            if (!rec.set_string (1, UpgradeCode) ||
                (VersionMin != null && !rec.set_string (2, VersionMin)) ||
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
            sql_insert = "INSERT INTO `Component` (`Component`, `ComponentId`, `Directory_`, `Attributes`, `KeyPath`, `Condition`) VALUES (?, ?, ?, ?, ?, ?)";
        }

        public void add (string Component, string? ComponentId, string Directory, int Attributes, string? KeyPath = null, string? Condition) throws GLib.Error {
            var rec = new Libmsi.Record (6);
            if (!rec.set_string (1, Component) ||
                (ComponentId != null && !rec.set_string (2, ComponentId)) ||
                !rec.set_string (3, Directory) ||
                !rec.set_int (4, Attributes) ||
                !rec.set_string (5, KeyPath) ||
                (Condition != null && !rec.set_string (6, Condition)))
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

    class MsiTableCondition : MsiTable {
        static construct {
            name = "Condition";
            sql_create = "CREATE TABLE `Condition` (`Feature_` CHAR(38) NOT NULL, `Level` INT NOT NULL, `Condition` CHAR(255) PRIMARY KEY `Feature_`, `Level`)";
            sql_insert = "INSERT INTO `Condition` (`Feature_`, `Level`, `Condition`) VALUES (?, ?, ?)";
        }

        public void add (string Feature, int Level, string? Condition) throws GLib.Error {
            var rec = new Libmsi.Record (3);
            if (!rec.set_string (1, Feature) ||
                !rec.set_int (2, Level) ||
                (Condition != null && !rec.set_string (3, Condition)))
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
            sql_insert = "INSERT INTO `Shortcut` (`Shortcut`, `Directory_`, `Name`, `Component_`, `Target`, `Icon_`, `IconIndex`, `WkDir`, `Description`, `Arguments`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
        }

        public Libmsi.Record add (string Shortcut, string Directory, string Name, string Component) throws GLib.Error {
            var rec = new Libmsi.Record (10);

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

        public static void set_arguments (Libmsi.Record rec, string Arguments) throws GLib.Error {
            if (!rec.set_string (10, Arguments))
                throw new Wixl.Error.FAILED ("failed to set record");
        }
    }

    class MsiTableCreateFolder: MsiTable {
        static construct {
            name = "CreateFolder";
            sql_create = "CREATE TABLE `CreateFolder` (`Directory_` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Directory_`, `Component_`)";
            sql_insert = "INSERT INTO `CreateFolder` (`Directory_`, `Component_`) VALUES (?, ?)";
        }

        public void add (string Directory, string Component) throws GLib.Error {
            var rec = new Libmsi.Record (2);
            if (!rec.set_string (1, Directory) ||
                !rec.set_string (2, Component))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableRemoveFile: MsiTable {
        static construct {
            name = "RemoveFile";
            sql_create = "CREATE TABLE `RemoveFile` (`FileKey` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) LOCALIZABLE, `DirProperty` CHAR(72) NOT NULL, `InstallMode` INT NOT NULL PRIMARY KEY `FileKey`)";
            sql_insert = "INSERT INTO `RemoveFile` (`FileKey`, `Component_`, `DirProperty`, `InstallMode`, `FileName`) VALUES (?, ?, ?, ?, ?)";
        }

        public void add (string FileKey, string Component, string DirProperty, int InstallMode, string? FileName) throws GLib.Error {
            var rec = new Libmsi.Record (5);
            if (!rec.set_string (1, FileKey) ||
                !rec.set_string (2, Component) ||
                !rec.set_string (3, DirProperty) ||
                !rec.set_int (4, InstallMode))
                throw new Wixl.Error.FAILED ("failed to add record");
            rec.set_string(5, FileName);

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

        public void add (string ServiceControl, string Name, int Event, string? Arguments, bool? Wait, string Component) throws GLib.Error {
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

        public void add (string ServiceInstall, string Name, string? DisplayName, int ServiceType, int StartType, int ErrorControl, string? LoadOrderGroup, string? Dependencies, string? StartName, string? Password, string? Arguments, string Component, string? Description = null) throws GLib.Error {
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

    class MsiTableSignature : MsiTable {
        static construct {
            name = "Signature";

            sql_create = "CREATE TABLE `Signature` (`Signature` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL, `MinVersion` CHAR(20), `MaxVersion` CHAR(20), `MinSize` LONG, `MaxSize` LONG, `MinDate` LONG, `MaxDate` LONG, `Languages` CHAR(255) PRIMARY KEY `Signature`)";
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
            sql_insert = "INSERT INTO `CustomAction` (`Action`, `Type`, `Source`, `Target`, `ExtendedType`) VALUES (?, ?, ?, ?, ?)";
        }

        public void add (string Action, int Type, string Source, string Target, int? ExtendedType = null) throws GLib.Error {
            var rec = new Libmsi.Record (5);
            if (!rec.set_string (1, Action) ||
                !rec.set_int (2, Type) ||
                !rec.set_string (3, Source) ||
                !rec.set_string (4, Target) ||
                (ExtendedType != null && !rec.set_int (5, ExtendedType)))
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

    /* https://docs.microsoft.com/en-us/windows/win32/msi/inifile-table */
    class MsiTableIniFile: MsiTable {
        static construct {
            name = "IniFile";
            sql_create = "CREATE TABLE `IniFile` (`IniFile` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `DirProperty` CHAR(72), `Section` CHAR(255) NOT NULL LOCALIZABLE, `Key` CHAR(255) NOT NULL LOCALIZABLE, `Value` CHAR(255) NOT NULL LOCALIZABLE, `Action` INT NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `IniFile`)";
            sql_insert = "INSERT INTO `IniFile` (`IniFile`, `FileName`, `DirProperty`, `Section`, `Key`, Value`, `Action`, `Component_`) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        }

        public void add (string IniFile, string FileName, string DirProperty, string Section, string Key, string Value, int Action, string Component) throws GLib.Error {
            var rec = new Libmsi.Record (8);
            if (!rec.set_string (1, IniFile) ||
                !rec.set_string (2, FileName) ||
                !rec.set_string (3, DirProperty) ||
                !rec.set_string (4, Section) ||
                !rec.set_string (5, Key) ||
                !rec.set_string (6, Value) ||
                !rec.set_int (7, Action) ||
                !rec.set_string (8, Component))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }
    /* https://docs.microsoft.com/en-us/windows/win32/msi/removeinifile-table */
    class MsiTableRemoveIniFile: MsiTable {
        static construct {
            name = "RemoveIniFile";
            sql_create = "CREATE TABLE `RemoveIniFile` (`RemoveIniFile` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `DirProperty` CHAR(72), `Section` CHAR(255) NOT NULL LOCALIZABLE, `Key` CHAR(255) NOT NULL LOCALIZABLE, `Value` CHAR(255) LOCALIZABLE, `Action` INT NOT NULL, `Component_` CHAR(72) NOT NULL PRIMARY KEY `RemoveIniFile`)";
            sql_insert = "INSERT INTO `RemoveIniFile` (`RemoveIniFile`, `FileName`, `DirProperty`, `Section`, `Key`, Value`, `Action`, `Component_`) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
        }

        public void add (string IniFile, string FileName, string DirProperty, string Section, string Key, string? Value, int Action, string Component) throws GLib.Error {
            var rec = new Libmsi.Record (8);
            if (!rec.set_string (1, IniFile) ||
                !rec.set_string (2, FileName) ||
                !rec.set_string (3, DirProperty) ||
                !rec.set_string (4, Section) ||
                !rec.set_string (5, Key) ||
                !rec.set_string (6, Value) ||
                !rec.set_int (7, Action) ||
                !rec.set_string (8, Component))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    /* https://docs.microsoft.com/en-us/windows/win32/msi/actiontext-table */
    class MsiTableActionText : MsiTable {
        static construct {
            name = "ActionText";
            sql_create = "CREATE TABLE `ActionText` (`Action` CHAR(72) NOT NULL, `Description` CHAR(255) LOCALIZABLE, `Template` CHAR(255) LOCALIZABLE PRIMARY KEY `Action`)";
            sql_insert = "INSERT INTO `ActionText` (`Action`, `Description`, `Template`) VALUES (?, ?, ?)";
        }

        public void add (string Action, string? Description, string? Template) throws GLib.Error {
            var rec = new Libmsi.Record (3);
            if (!rec.set_string (1, Action) ||
                (Description != null && !rec.set_string (2, Description)) ||
                (Template != null && !rec.set_string (3, Template)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }
    }

    class MsiTableEnvironment: MsiTable {
        static construct {
            name = "Environment";
            sql_create = "CREATE TABLE `Environment` (`Environment` CHAR(72) NOT NULL, `Name` CHAR(64) NOT NULL LOCALIZABLE, `Value` CHAR(255) LOCALIZABLE, `Component_` CHAR(72) NOT NULL PRIMARY KEY `Environment`)";
            sql_insert = "INSERT INTO `Environment` (`Environment`, `Name`, `Value`, `Component_`) VALUES (?, ?, ?, ?)";
        }

        public Libmsi.Record add (string Environment, string Name, string? Value, string Component) throws GLib.Error {
            var rec = new Libmsi.Record (4);

            if (!rec.set_string (1, Environment) ||
                !rec.set_string (2, Name) ||
                (Value != null && !rec.set_string (3, Value)) ||
                !rec.set_string (4, Component))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
            return rec;
        }
    }

    class MsiTableDuplicateFile: MsiTable {
        static construct {
            name = "DuplicateFile";
            sql_create = "CREATE TABLE `DuplicateFile` (`FileKey` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, File_ CHAR(72) NOT NULL, DestName CHAR(255) LOCALIZABLE, DestFolder CHAR(72) PRIMARY KEY `FileKey`)";
            sql_insert = "INSERT INTO `DuplicateFile` (`FileKey`, `Component_`, `File_`, `DestName`, `DestFolder`) VALUES (?, ?, ?, ?, ?)";
        }

        public Libmsi.Record add (string FileKey, string Component, string File, string DestName, string DestFolder) throws GLib.Error {
            var rec = new Libmsi.Record (5);

            if (!rec.set_string (1, FileKey) ||
                !rec.set_string (2, Component) ||
                !rec.set_string (3, File) ||
                (DestName != null && !rec.set_string (4, DestName)) ||
                (DestFolder != null && !rec.set_string (5, DestFolder)))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
            return rec;
        }
    }

    class MsiTableMoveFile: MsiTable {
        static construct {
            name = "MoveFile";
            sql_create = "CREATE TABLE `MoveFile` (`FileKey` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `SourceName` CHAR(255) LOCALIZABLE, `DestName` CHAR(255) LOCALIZABLE, `SourceFolder` CHAR(72), `DestFolder` CHAR(72) NOT NULL, `Options` INTEGER NOT NULL PRIMARY KEY `FileKey`)";
            sql_insert = "INSERT INTO `MoveFile` (`FileKey`, `Component_`, `SourceName`, `DestName`, `SourceFolder`, `DestFolder`, `Options`) VALUES (?, ?, ?, ?, ?, ?, ?)";
        }

        public Libmsi.Record add (string FileKey, string Component, string SourceName, string? DestName, string SourceFolder, string DestFolder, int Options) throws GLib.Error {
            var rec = new Libmsi.Record (7);

            if (!rec.set_string (1, FileKey) ||
                !rec.set_string (2, Component) ||
                (SourceName != null && !rec.set_string (3, SourceName)) ||
                !rec.set_string (4, DestName) ||
                (SourceFolder != null && !rec.set_string (5, SourceFolder)) ||
                !rec.set_string (6, DestFolder) ||
                !rec.set_int(7, Options))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
            return rec;
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

        public string? get_subject () {
            try {
                return properties.get_string (Libmsi.Property.SUBJECT);
            } catch (GLib.Error err) {
                return null;
            }
        }

        public void set_comments (string value) throws GLib.Error {
            set_property (Libmsi.Property.COMMENTS, value);
        }

        public string? get_comments () {
            try {
                return properties.get_string (Libmsi.Property.COMMENTS);
            } catch (GLib.Error err) {
                return null;
            }
        }
    }

    class MsiDatabase: Object {
        public Arch arch;
        public Extension[] extensions;

        public MsiSummaryInfo info;
        public MsiTableProperty table_property;
        public MsiTableIcon table_icon;
        public MsiTableBinary table_binary;
        public MsiTableMedia table_media;
        public MsiTableDirectory table_directory;
        public MsiTableComponent table_component;
        public MsiTableCondition table_condition;
        public MsiTableFeature table_feature;
        public MsiTableFeatureComponents table_feature_components;
        public MsiTableRemoveFile table_remove_file;
        public MsiTableRegistry table_registry;
        public MsiTableServiceControl table_service_control;
        public MsiTableServiceInstall table_service_install;
        public MsiTableFile table_file;
        public MsiTableIniFile table_ini_file;
        public MsiTableRemoveIniFile table_remove_ini_file;
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
        public MsiTableActionText table_action_text;
        public MsiTableRegLocator table_reg_locator;
        public MsiTableCreateFolder table_create_folder;
        public MsiTableSignature table_signature;
        public MsiTableFileHash table_file_hash;
        public MsiTableCheckBox table_check_box;
        public MsiTableEventMapping table_event_mapping;
        public MsiTableControl table_control;
        public MsiTableUIText table_ui_text;
        public MsiTableTextStyle table_text_style;
        public MsiTableDialog table_dialog;
        public MsiTableControlEvent table_control_event;
        public MsiTableControlCondition table_control_condition;
        public MsiTableListBox table_list_box;
        public MsiTableRadioButton table_radio_button;
        public MsiTableEnvironment table_environment;
        public MsiTableDuplicateFile table_duplicate_file;
        public MsiTableMoveFile table_move_file;

        public HashTable<string, MsiTable> tables;

        int get_default_version () {
            if (arch == Arch.X86)
                return 100;
            else
                return 200;
        }

        string get_arch_template () {
            switch (arch) {
            case Arch.ARM64:
                return "Arm64";
            case Arch.X86:
                // case Arch.INTEL:
                return "Intel";
            case Arch.IA64:
            case Arch.X64:
                // case Arch.INTEL64:
                return "x64";
            }
            assert_not_reached ();
        }

        public MsiDatabase (Arch arch, Extension[] extensions) {
            this.arch = arch;
            this.extensions = extensions;

            info = new MsiSummaryInfo ();
            try {
                info.set_property (Libmsi.Property.TITLE, "Installation Database");
                info.set_property (Libmsi.Property.TEMPLATE,
                                   "%s;1033".printf (get_arch_template ()));
                info.set_property (Libmsi.Property.KEYWORDS, "Installer");
                info.set_property (Libmsi.Property.CODEPAGE, 1252);
                info.set_property (Libmsi.Property.UUID, get_uuid ("*"));
                info.set_property (Libmsi.Property.CREATED_TM,
                                   time_to_filetime (now ()));
                info.set_property (Libmsi.Property.LASTSAVED_TM,
                                   time_to_filetime (now ()));
                info.set_property (Libmsi.Property.VERSION, get_default_version ());
                info.set_property (Libmsi.Property.APPNAME, Config.PACKAGE_STRING);
                info.set_property (Libmsi.Property.SECURITY, 2);
            } catch (GLib.Error error) {
                critical (error.message);
            }

            tables = new HashTable<string, MsiTable> (str_hash, str_equal);
            table_property = new MsiTableProperty ();
            table_icon = new MsiTableIcon ();
            table_binary = new MsiTableBinary ();
            table_media = new MsiTableMedia ();
            table_directory = new MsiTableDirectory ();
            table_component = new MsiTableComponent ();
            table_feature = new MsiTableFeature ();
            table_feature_components = new MsiTableFeatureComponents ();
            table_remove_file = new MsiTableRemoveFile ();
            table_registry = new MsiTableRegistry ();
            table_condition = new MsiTableCondition ();
            table_service_control = new MsiTableServiceControl ();
            table_service_install = new MsiTableServiceInstall ();
            table_file = new MsiTableFile ();
            table_ini_file = new MsiTableIniFile ();
            table_remove_ini_file = new MsiTableRemoveIniFile ();
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
            table_signature = new MsiTableSignature ();
            table_custom_action = new MsiTableCustomAction ();
            table_action_text = new MsiTableActionText ();
            table_reg_locator = new MsiTableRegLocator ();
            table_create_folder = new MsiTableCreateFolder ();
            table_file_hash = new MsiTableFileHash ();
            table_environment = new MsiTableEnvironment ();
            table_duplicate_file = new MsiTableDuplicateFile ();
            table_move_file = new MsiTableMoveFile ();

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
                    table_binary,
                    table_component,
                    table_feature,
                    table_feature_components,
                    table_remove_file,
                    table_registry,
                    table_condition,
                    table_service_control,
                    table_service_install,
                    table_file,
                    table_ini_file,
                    table_remove_ini_file,
                    table_streams,
                    table_shortcut,
                    table_upgrade,
                    table_launch_condition,
                    table_app_search,
                    table_signature,
                    table_custom_action,
                    table_action_text,
                    table_reg_locator,
                    table_create_folder,
                    table_file_hash,
                    table_environment,
                    table_duplicate_file,
                    table_move_file,
                    new MsiTableError (),
                    new MsiTableValidation ()
                }) {
                tables.insert (t.name, t);
            }

            if (Extension.UI in extensions) {
                table_check_box = new MsiTableCheckBox ();
                table_control = new MsiTableControl ();
                table_control_condition = new MsiTableControlCondition ();
                table_control_event = new MsiTableControlEvent ();
                table_dialog = new MsiTableDialog ();
                table_event_mapping = new MsiTableEventMapping ();
                table_list_box = new MsiTableListBox ();
                table_radio_button = new MsiTableRadioButton ();
                table_text_style = new MsiTableTextStyle ();
                table_ui_text = new MsiTableUIText ();

                foreach (var t in new MsiTable[] {
                        table_check_box,
                        table_control,
                        table_control_condition,
                        table_control_event,
                        table_dialog,
                        table_event_mapping,
                        table_list_box,
                        table_radio_button,
                        table_text_style,
                        table_ui_text,
                    }) {
                    tables.insert (t.name, t);
                }
            }
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
