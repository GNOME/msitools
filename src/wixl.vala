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

        public void add (string filename) throws GLib.Error {
            var rec = new Libmsi.Record (2);

            if (!rec.set_string (1, filename) ||
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
                add_sequence ("CostInitialize", 800);
                add_sequence ("FileCost", 900);
                add_sequence ("CostFinalize", 1000);
                add_sequence ("InstallValidate", 1400);
                add_sequence ("InstallInitialize", 1500);
                add_sequence ("InstallAdminPackage", 3900);
                add_sequence ("InstallFiles", 4000);
                add_sequence ("InstallFinalize", 6600);
            } catch (GLib.Error e) {
            }
        }

        public void add_sequence (string action, int sequence) throws GLib.Error {
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
                add_sequence ("CostInitialize", 800);
                add_sequence ("FileCost", 900);
                add_sequence ("CostFinalize", 1000);
                add_sequence ("ExecuteAction", 1300);
            } catch (GLib.Error e) {
            }
        }

        public void add_sequence (string action, int sequence) throws GLib.Error {
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
                add_sequence ("CostInitialize", 800);
                add_sequence ("CostFinalize", 1000);
                add_sequence ("InstallValidate", 1400);
                add_sequence ("InstallInitialize", 1500);
                add_sequence ("InstallFinalize", 6600);
                add_sequence ("PublishFeatures", 6300);
                add_sequence ("PublishProduct", 6400);
            } catch (GLib.Error e) {
            }
        }

        public void add_sequence (string action, int sequence) throws GLib.Error {
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
                add_sequence ("CostInitialize", 800);
                add_sequence ("FileCost", 900);
                add_sequence ("CostFinalize", 1000);
                add_sequence ("InstallValidate", 1400);
                add_sequence ("InstallInitialize", 1500);
                add_sequence ("InstallFinalize", 6600);
                add_sequence ("PublishFeatures", 6300);
                add_sequence ("PublishProduct", 6400);
                add_sequence ("ValidateProductID", 700);
                add_sequence ("ProcessComponents", 1600);
                add_sequence ("UnpublishFeatures", 1800);
                add_sequence ("RegisterUser", 6000);
                add_sequence ("RegisterProduct", 6100);
            } catch (GLib.Error e) {
            }
        }

        public void add_sequence (string action, int sequence) throws GLib.Error {
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
                add_sequence ("CostInitialize", 800);
                add_sequence ("FileCost", 900);
                add_sequence ("CostFinalize", 1000);
                add_sequence ("ExecuteAction", 1300);
                add_sequence ("ValidateProductID", 700);
            } catch (GLib.Error e) {
            }
        }

        public void add_sequence (string action, int sequence) throws GLib.Error {
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

        public override void create (Libmsi.Database db) throws GLib.Error {
            var query = new Libmsi.Query (db, "CREATE TABLE `Media` (`DiskId` INT NOT NULL, `LastSequence` LONG NOT NULL, `DiskPrompt` CHAR(64) LOCALIZABLE, `Cabinet` CHAR(255), `VolumeLabel` CHAR(32), `Source` CHAR(72) PRIMARY KEY `DiskId`)");
            query.execute (null);
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

            foreach (var t in new MsiTable[] {
                    new MsiTableAdminExecuteSequence (),
                    new MsiTableAdminUISequence (),
                    new MsiTableAdvtExecuteSequence (),
                    new MsiTableError (),
                    new MsiTableFile (),
                    new MsiTableInstallExecuteSequence (),
                    new MsiTableInstallUISequence (),
                    new MsiTableMedia (),
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



    public abstract class WixElementVisitor: Object {
        public abstract void visit_product (WixProduct product) throws GLib.Error;
        public abstract void visit_icon (WixIcon icon) throws GLib.Error;
        public abstract void visit_package (WixPackage package) throws GLib.Error;
    }

    public class WixElement: Object {
        public string Id { get; set; }

        static construct {
            Value.register_transform_func (typeof (WixElement), typeof (string), (ValueTransform)WixElement.value_to_string);
        }

        public string to_string () {
            var type = get_type ();
            var klass = (ObjectClass)type.class_ref ();
            var str = type.name () + " {";

            var i = 0;
            foreach (var p in klass.list_properties ()) {
                var value = Value (p.value_type);
                get_property (p.name, ref value);
                var valstr = value.holds (typeof (string)) ?
                    (string)value : value.strdup_contents ();
                str += (i == 0 ? "" : ", ") + p.name + ": " + valstr;
                i += 1;
            }

            return str + "}";
        }

        public static void value_to_string (Value src, out Value dest) {
            WixElement e = value_get_element (src);

            dest = e.to_string ();
        }

        public static WixElement? value_get_element (Value value) {
            if (! value.holds (typeof (WixElement)))
                return null;

            return (WixElement)value.get_object ();
        }

        public virtual void accept (WixElementVisitor visitor) throws GLib.Error {
        }
    }

    public class WixPackage: WixElement {
        public string Keywords { get; set; }
        public string InstallerDescription { get; set; }
        public string InstallerComments { get; set; }
        public string Manufacturer { get; set; }
        public string InstallerVersion { get; set; }
        public string Languages { get; set; }
        public string Compressed { get; set; }
        public string SummaryCodepage { get; set; }
        public string Comments { get; set; }
        public string Description { get; set; }

        public void load (Xml.Node *node) throws Wixl.Error {
            if (node->name != "Package")
                throw new Error.FAILED ("invalid node");

            for (var prop = node->properties; prop != null; prop = prop->next) {
                if (prop->type == Xml.ElementType.ATTRIBUTE_NODE)
                    set_property (prop->name, get_attribute_content (prop));
            }

            for (var child = node->children; child != null; child = child->next) {
                switch (child->type) {
                case Xml.ElementType.COMMENT_NODE:
                case Xml.ElementType.TEXT_NODE:
                    continue;
                case Xml.ElementType.ELEMENT_NODE:
                    switch (child->name) {
                    }
                    break;
                }
                debug ("unhandled node %s", child->name);
            }
        }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            visitor.visit_package (this);
        }
    }

    public class WixIcon: WixElement {
        public string SourceFile { get; set; }

        public void load (Xml.Node *node) throws Wixl.Error {
            if (node->name != "Icon")
                throw new Error.FAILED ("invalid node");

            for (var prop = node->properties; prop != null; prop = prop->next) {
                if (prop->type == Xml.ElementType.ATTRIBUTE_NODE)
                    set_property (prop->name, get_attribute_content (prop));
            }
        }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            visitor.visit_icon (this);
        }
    }

    public class WixProduct: WixElement {
        public string Name { get; set; }
        public string UpgradeCode { get; set; }
        public string Language { get; set; }
        public string Codepage { get; set; }
        public string Version { get; set; }
        public string Manufacturer { get; set; }

        public WixPackage package { get; set; }
        public WixIcon icon { get; set; }

        public WixProduct () {
        }

        public void load (Xml.Node *node) throws Wixl.Error {
            if (node->name != "Product")
                throw new Error.FAILED ("invalid node");

            for (var prop = node->properties; prop != null; prop = prop->next) {
                if (prop->type == Xml.ElementType.ATTRIBUTE_NODE)
                    set_property (prop->name, get_attribute_content (prop));
            }

            for (var child = node->children; child != null; child = child->next) {
                switch (child->type) {
                case Xml.ElementType.COMMENT_NODE:
                case Xml.ElementType.TEXT_NODE:
                    continue;
                case Xml.ElementType.ELEMENT_NODE:
                    switch (child->name) {
                    case "Package":
                        package = new WixPackage ();
                        package.load (child);
                        continue;
                    case "Icon":
                        icon = new WixIcon ();
                        icon.load (child);
                        continue;
                    }
                    break;
                }
                debug ("unhandled node %s", child->name);
            }
        }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            visitor.visit_product (this);

            package.accept (visitor);
            icon.accept (visitor);
        }
    }

    class WixRoot: WixElement {
        public string xmlns { get; set; }
        public WixProduct product { get; set; }

        public WixRoot () {
        }

        public void load (Xml.Doc *doc) throws Wixl.Error {
            var root = doc->children;

            if (root->name != "Wix")
                throw new Error.FAILED ("invalid XML document");

            if (root->ns != null)
                xmlns = root->ns->href;

            for (var child = root->children; child != null; child = child->next) {
                switch (child->type) {
                case Xml.ElementType.COMMENT_NODE:
                case Xml.ElementType.TEXT_NODE:
                    continue;
                case Xml.ElementType.ELEMENT_NODE:
                    switch (child->name) {
                    case "Product":
                        product = new WixProduct ();
                        product.load (child);
                        continue;
                    }
                    break;
                }

                debug ("unhandled node %s", child->name);
            }
        }

        public override void accept (WixElementVisitor visitor) throws GLib.Error {
            product.accept (visitor);
        }
    }

    class WixBuilder: WixElementVisitor {
        WixRoot root;
        MsiDatabase db;

        public WixBuilder (WixRoot root) {
            this.root = root;
        }

        public MsiDatabase build () throws GLib.Error {
            db = new MsiDatabase ();
            root.accept (this);
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
            db.table_icon.add (icon.SourceFile);
        }
    }

    int main (string[] args) {
        Intl.bindtextdomain (Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
        Intl.bind_textdomain_codeset (Config.GETTEXT_PACKAGE, "UTF-8");
        Intl.textdomain (Config.GETTEXT_PACKAGE);

        try {
            var file = File.new_for_commandline_arg (args[1]);
            string data;
            FileUtils.get_contents (file.get_path (), out data);
            var doc = Xml.Parser.read_memory (data, data.length);
            var root = new WixRoot ();
            root.load (doc);
            var builder = new WixBuilder (root);
            var msi = builder.build ();
            msi.build ("foo.msi");
        } catch (GLib.Error error) {
            printerr (error.message + "\n");
            return 1;
        }

        return 0;
    }

} // Wixl
