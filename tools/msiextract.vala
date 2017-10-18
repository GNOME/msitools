using Posix;

static bool version;
static bool list_only;
[CCode (array_length = false, array_null_terminated = true)]
static string[] files;
static string? directory = null;

private const OptionEntry[] options = {
    { "version", 0, 0, OptionArg.NONE, ref version, N_("Display version number"), null },
    { "directory", 'C', 0, OptionArg.FILENAME, ref directory, N_("Extract to directory"), null },
    { "list", 'l', 0, OptionArg.NONE, ref list_only, N_("List files only"), null },
    { "", 0, 0, OptionArg.FILENAME_ARRAY, ref files, null, N_("MSI_FILE...") },
    { null }
};

public string get_long_name (string str) {
    var names = str.split ("|", 2);
    if (names.length == 2)
        return names[1];

    return str;
}

// Look for a case insensitive match of cab in the directory dir.
public string lookup_cab (string dir, string cab) throws GLib.Error
{
    var path = File.new_for_path (dir);
    var children = path.enumerate_children ("standard::*", FileQueryInfoFlags.NONE);

    string CAB = cab.up ();
    FileInfo info = null;
    while ((info = children.next_file ()) != null) {
        if (info.get_file_type () == FileType.DIRECTORY)
            continue;

        if (info.get_name ().up () == CAB)
            return path.get_child (info.get_name ()).get_path ();
    }

    return path.get_child (cab).get_path ();
}

public void extract_cab (Libmsi.Database db, string cab,
                         HashTable<string, string> cab_to_name) throws GLib.Error
{
    var cabinet = new GCab.Cabinet ();

    if (cab.has_prefix ("#")) {
        var name = cab.substring (1);
        var query = new Libmsi.Query (db, "SELECT `Data` FROM `_Streams` WHERE `Name` = '%s'".printf (name));
        query.execute ();
        var rec = query.fetch ();
        cabinet.load (rec.get_stream (1));
    }
    else {
        // Look for the cab file in the directory the MSI file resides in.
        var dbpath = File.new_for_path (db.path).get_parent ();
        cab = lookup_cab (dbpath.get_path (), cab);

        try {
            cabinet.load (File.new_for_path (cab).read ());
        } catch (GLib.Error err) {
            throw new GLib.Error (err.domain, err.code, cab + ": " + err.message);
        }
    }

    var path = File.new_for_path (directory);
    cabinet.extract_simple (path, (current) => {
            var extname = cab_to_name.lookup (current.get_name ());
            if (extname == null) {
                extname = current.get_name ();
                warning ("couldn't lookup MSI name, fallback on cab name %s", extname);
            }
            current.set_extract_name (extname);
            GLib.stdout.printf ("%s\n", extname);
            return true;
        }, null);
}

public string? get_directory_name (Libmsi.Record rec) throws GLib.Error {
    var name = get_long_name (rec.get_string (3));

    // only by intuition...
    if (rec.get_string (1) == "ProgramFilesFolder")
        return "Program Files";
    else if (name == "." || name == "SourceDir")
        return "";

    return name;
}

public void extract (string filename) throws GLib.Error {
    Libmsi.Record? rec;
    var db = new Libmsi.Database (filename, Libmsi.DbFlags.READONLY, null);

    var directories = new HashTable<string, Libmsi.Record> (str_hash, str_equal);
    var query = new Libmsi.Query (db, "SELECT * FROM `Directory`");
    query.execute ();
    while ((rec = query.fetch ()) != null)
        directories.insert (rec.get_string (1), rec);

    var components_dir = new HashTable<string, string> (str_hash, str_equal);
    query = new Libmsi.Query (db, "SELECT * FROM `Component`");
    query.execute ();
    while ((rec = query.fetch ()) != null) {
        var dir_id = rec.get_string (3);
        var dir_rec = directories.lookup (dir_id);
        var dir = get_directory_name (dir_rec);

        do {
            var parent = dir_rec.get_string (2);
            dir_rec = directories.lookup (parent);
            if (dir_rec == null)
                break;
            parent = get_directory_name (dir_rec);
            if (parent == null)
                break;
            if (parent == "")
                continue;

            dir = Path.build_filename (parent, dir);
        } while (true);

        components_dir.insert (rec.get_string (1), dir);
    }

    var cab_to_name = new HashTable<string, string> (str_hash, str_equal);
    query = new Libmsi.Query (db, "SELECT * FROM `File`");
    query.execute ();
    while ((rec = query.fetch ()) != null) {
        var dir = components_dir.lookup (rec.get_string (2));
        var file = Path.build_filename (dir, get_long_name (rec.get_string (3)));
        if (list_only)
            GLib.stdout.printf ("%s\n", file);
        cab_to_name.insert (rec.get_string (1), file);
    }

    if (list_only)
        exit (0);

    query = new Libmsi.Query (db, "SELECT * FROM `Media`");
    query.execute ();
    while ((rec = query.fetch ()) != null) {
        var cab = rec.get_string (4);
        if (cab == "") {
            // Ignore empty cab names
            continue;
        }
        extract_cab (db, cab, cab_to_name);
    }
}

public int main (string[] args) {
    Intl.bindtextdomain (Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
    Intl.bind_textdomain_codeset (Config.GETTEXT_PACKAGE, "UTF-8");
    Intl.textdomain (Config.GETTEXT_PACKAGE);
    GLib.Environment.set_application_name ("msiextract");

    var parameter_string = _("- a msi files extracting tool");
    var opt_context = new OptionContext (parameter_string);
    opt_context.set_help_enabled (true);
    opt_context.add_main_entries (options, null);

    try {
        opt_context.parse (ref args);
    } catch (OptionError.BAD_VALUE err) {
        GLib.stdout.printf (opt_context.get_help (true, null));
        exit (1);
    } catch (OptionError error) {
        warning (error.message);
    }

    if (version) {
        GLib.stdout.printf ("%s\n", Config.PACKAGE_VERSION);
        exit (0);
    }

    if (files.length < 1) {
        GLib.stderr.printf (_("Please specify input files.\n"));
        exit (1);
    }

    if (directory == null)
        directory = Environment.get_current_dir ();

    try {
        foreach (var file in files)
            extract (file);
    } catch (GLib.Error error) {
        GLib.stderr.printf ("%s\n", error.message);
        exit (1);
    }

    return 0;
}
