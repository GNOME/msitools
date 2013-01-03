namespace Wixl {

    abstract class MsiTable: Object {
        public string name;
        public List<Libmsi.Record> records;

        public abstract void create (Libmsi.Database db) throws GLib.Error;
    }

    class MsiTableIcon: MsiTable {
        construct {
            name = "Icon";
        }

        public void add (string id, string filename) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, id) ||
                !rec.load_stream (2, filename))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `Icon` (`Name` CHAR(72) NOT NULL, `Data` OBJECT NOT NULL PRIMARY KEY `Name`)");
            query.execute (null);
            query = new Libmsi.Query (db, "INSERT INTO `Icon` (`Name`, `Data`) VALUES (?, ?)");
            foreach (var r in records)
                query.execute (r);
        }
    }

    class MsiTableAdminExecuteSequence: MsiTable {
        construct {
            name = "AdminExecuteSequence";
            try {
                add ("CostInitialize", 800);
                add ("FileCost", 900);
                add ("CostFinalize", 1000);
                add ("InstallValidate", 1400);
                add ("InstallInitialize", 1500);
                add ("InstallAdminPackage", 3900);
                add ("InstallFiles", 4000);
                add ("InstallFinalize", 6600);
            } catch (GLib.Error e) {
            }
        }

        public void add (string action, int sequence) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, action) ||
                !rec.set_int (2, sequence))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `AdminExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` INT PRIMARY KEY `Action`)");
            query.execute (null);

            query = new Libmsi.Query (db, "INSERT INTO `AdminExecuteSequence` (`Action`, `Sequence`) VALUES (?, ?)");
            foreach (var r in records)
                query.execute (r);
        }
    }

    class MsiTableAdminUISequence: MsiTable {
        construct {
            name = "AdminUISequence";

            try {
                add ("CostInitialize", 800);
                add ("FileCost", 900);
                add ("CostFinalize", 1000);
                add ("ExecuteAction", 1300);
            } catch (GLib.Error e) {
            }
        }

        public void add (string action, int sequence) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, action) ||
                !rec.set_int (2, sequence))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `AdminUISequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` INT PRIMARY KEY `Action`)");
            query.execute (null);

            query = new Libmsi.Query (db, "INSERT INTO `AdminUISequence` (`Action`, `Sequence`) VALUES (?, ?)");
            foreach (var r in records)
                query.execute (r);
        }
    }

    class MsiTableAdvtExecuteSequence: MsiTable {
        construct {
            name = "AdvtExecuteSequence";

            try {
                add ("CostInitialize", 800);
                add ("CostFinalize", 1000);
                add ("InstallValidate", 1400);
                add ("InstallInitialize", 1500);
                add ("InstallFinalize", 6600);
                add ("PublishFeatures", 6300);
                add ("PublishProduct", 6400);
            } catch (GLib.Error e) {
            }
        }

        public void add (string action, int sequence) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, action) ||
                !rec.set_int (2, sequence))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `AdvtExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` INT PRIMARY KEY `Action`)");
            query.execute (null);

            query = new Libmsi.Query (db, "INSERT INTO `AdvtExecuteSequence` (`Action`, `Sequence`) VALUES (?, ?)");
            foreach (var r in records)
                query.execute (r);
        }
    }

    class MsiTableError: MsiTable {
        construct {
            name = "Error";
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `Error` (`Error` INT NOT NULL, `Message` CHAR(0) LOCALIZABLE PRIMARY KEY `Error`)");
            query.execute (null);
        }
    }

    class MsiTableFile: MsiTable {
        construct {
            name = "File";
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `File` (`File` CHAR(72) NOT NULL, `Component_` CHAR(72) NOT NULL, `FileName` CHAR(255) NOT NULL LOCALIZABLE, `FileSize` LONG NOT NULL, `Version` CHAR(72), `Language` CHAR(20), `Attributes` INT, `Sequence` LONG NOT NULL PRIMARY KEY `File`)");
            query.execute (null);
        }
    }

    class MsiTableInstallExecuteSequence: MsiTable {
        construct {
            name = "InstallExecuteSequence";

            try {
                add ("CostInitialize", 800);
                add ("FileCost", 900);
                add ("CostFinalize", 1000);
                add ("InstallValidate", 1400);
                add ("InstallInitialize", 1500);
                add ("InstallFinalize", 6600);
                add ("PublishFeatures", 6300);
                add ("PublishProduct", 6400);
                add ("ValidateProductID", 700);
                add ("ProcessComponents", 1600);
                add ("UnpublishFeatures", 1800);
                add ("RegisterUser", 6000);
                add ("RegisterProduct", 6100);
            } catch (GLib.Error e) {
            }
        }

        public void add (string action, int sequence) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, action) ||
                !rec.set_int (2, sequence))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `InstallExecuteSequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` INT PRIMARY KEY `Action`)");
            query.execute (null);

            query = new Libmsi.Query (db, "INSERT INTO `InstallExecuteSequence` (`Action`, `Sequence`) VALUES (?, ?)");
            foreach (var r in records)
                query.execute (r);
        }
    }

    class MsiTableInstallUISequence: MsiTable {
        construct {
            name = "InstallUISequence";

            try {
                add ("CostInitialize", 800);
                add ("FileCost", 900);
                add ("CostFinalize", 1000);
                add ("ExecuteAction", 1300);
                add ("ValidateProductID", 700);
            } catch (GLib.Error e) {
            }
        }

        public void add (string action, int sequence) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, action) ||
                !rec.set_int (2, sequence))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `InstallUISequence` (`Action` CHAR(72) NOT NULL, `Condition` CHAR(255), `Sequence` INT PRIMARY KEY `Action`)");
            query.execute (null);

            query = new Libmsi.Query (db, "INSERT INTO `InstallUISequence` (`Action`, `Sequence`) VALUES (?, ?)");
            foreach (var r in records)
                query.execute (r);

        }
    }

    class MsiTableMedia: MsiTable {
        construct {
            name = "Media";
        }

        public void add (string DiskId, string LastSequence, string DiskPrompt, string Cabinet) throws GLib.Error {
            var rec = new Libmsi.Record (4);

            if (!rec.set_int (1, int.parse (DiskId)) ||
                !rec.set_int (2, int.parse (LastSequence)) ||
                !rec.set_string (3, DiskPrompt) ||
                !rec.set_string (4, Cabinet))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);

        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `Media` (`DiskId` INT NOT NULL, `LastSequence` LONG NOT NULL, `DiskPrompt` CHAR(64) LOCALIZABLE, `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`)");
            query.execute (null);

            query = new Libmsi.Query (db, "INSERT INTO `Media` (`DiskId`, `LastSequence`, `DiskPrompt`, `Cabinet`) VALUES (?, ?, ?, ?)");
            foreach (var r in records)
                query.execute (r);
        }
    }

    class MsiTableProperty: MsiTable {
        construct {
            name = "Property";
        }

        public void add (string prop, string value) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, prop) ||
                !rec.set_string (2, value))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `Property` (`Property` CHAR(72) NOT NULL, `Value` CHAR(0) NOT NULL LOCALIZABLE PRIMARY KEY `Property`)");
            query.execute (null);

            query = new Libmsi.Query (db, "INSERT INTO `Property` (`Property`, `Value`) VALUES (?, ?)");
            foreach (var r in records)
                query.execute (r);
        }
    }

    class MsiTableDirectory: MsiTable {
        construct {
            name = "Directory";
        }

        public void add (string Directory, string? Parent, string DefaultDir) throws GLib.Error {
            var rec = new Libmsi.Record (3);
            if (!rec.set_string (1, Directory) ||
                !rec.set_string (2, Parent) ||
                !rec.set_string (3, DefaultDir))
                throw new Wixl.Error.FAILED ("failed to add record");

            records.append (rec);
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `Directory` (`Directory` CHAR(72) NOT NULL, `Directory_Parent` CHAR(72), `DefaultDir` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `Directory`)");
            query.execute (null);

            query = new Libmsi.Query (db, "INSERT INTO `Directory` (`Directory`, `Directory_Parent`, `DefaultDir`) VALUES (?, ?, ?)");
            foreach (var r in records)
                query.execute (r);
        }
    }

    class MsiTable_Validation: MsiTable {
        construct {
            name = "_Validation";
        }

        public override void create (Libmsi.Database db) throws GLib.Error {
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

        HashTable<string, MsiTable> tables;

        construct {
            info = new MsiSummaryInfo ();
            try {
                info.set_property (Libmsi.Property.TITLE, "Installation Database");
                info.set_property (Libmsi.Property.TEMPLATE, "Intel;1033");
                info.set_property (Libmsi.Property.UUID, uuid_generate ());
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

            foreach (var t in new MsiTable[] {
                    new MsiTableAdminExecuteSequence (),
                    new MsiTableAdminUISequence (),
                    new MsiTableAdvtExecuteSequence (),
                    new MsiTableError (),
                    new MsiTableFile (),
                    new MsiTableInstallExecuteSequence (),
                    new MsiTableInstallUISequence (),
                    table_directory,
                    table_media,
                    table_property,
                    table_icon,
                    new MsiTable_Validation ()
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

            var db = new Libmsi.Database (filename, (string)2);
            info.save (db);

            var it = HashTableIter <string, MsiTable> (tables);
            while (it.next (out name, out table))
                table.create (db);

            db.commit ();
        }
    }

} // Wixl