using Wixl;

static string cg;
static string dr;
static string prefix;
static string vardir;
[CCode (array_length = false, array_null_terminated = true)]
static string[] exclude;
static bool win64;

private const OptionEntry[] options = {
    { "directory-ref", 0, 0, OptionArg.STRING, ref dr, N_("Directory Ref"), null },
    { "component-group", 0, 0, OptionArg.STRING, ref cg, N_("Component Group"), null },
    { "var", 0, 0, OptionArg.STRING, ref vardir, N_("Variable for source dir"), null },
    { "prefix", 'p', 0, OptionArg.STRING, ref prefix, N_("Prefix"), null },
    { "exclude", 'x', 0, OptionArg.STRING_ARRAY, ref exclude, N_("Exclude prefix"), null },
    { "win64", 0, 0, OptionArg.NONE, ref win64, N_("Add Win64 Component"), null },
    { null }
};

bool filtered (string file) {
    foreach (var f in exclude)
        if (file.has_prefix (f))
            return true;

    return false;
}

string escape_filename(string filename) {
    return filename.replace("$", "$$");
}

public int main (string[] args) {

    var cmdline = string.joinv (" ", args);
    var opt_context = new OptionContext ("");
    opt_context.set_help_enabled (true);
    opt_context.add_main_entries (options, null);

    // default values
    dr = "TARGETDIR";
    var sourcedir = "SourceDir";

    try {
        opt_context.parse (ref args);
    } catch (OptionError.BAD_VALUE err) {
        GLib.stdout.printf (opt_context.get_help (true, null));
        return 1;
    } catch (OptionError error) {
        warning (error.message);
    }

    if (prefix == null) {
        GLib.stderr.printf ("Please specify source dir prefix\n");
        return 1;
    }
    if (vardir != null)
        sourcedir = "$(%s)".printf (vardir);

    stdout.printf ("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    stdout.printf ("<Wix xmlns=\"http://schemas.microsoft.com/wix/2006/wi\">\n");
    stdout.printf ("  <Fragment>\n");
    stdout.printf ("    <DirectoryRef Id=\"%s\">\n".printf (dr));

    string[] last_path = null;
    List<string> cmpref = null;
    var indent = "      ";

    try {
        var dis = new DataInputStream (new UnixInputStream (0));
        string line;

        while ((line = dis.read_line (null)) != null) {
            if (!line.has_prefix (prefix))
                continue;

            var file = line[prefix.length:line.length];
            if (filtered (file))
                continue;

            var gfile = File.new_for_path (line);
            var is_directory = gfile.query_file_type (FileQueryInfoFlags.NONE) == FileType.DIRECTORY;

            var dir = is_directory ? file : Path.get_dirname (file);
            var path = dir.split (Path.DIR_SEPARATOR_S, -1);
            var i = 0;
            if (last_path != null) {
                while ((path[i] != null && last_path[i] != null) &&
                       path[i] == last_path[i])
                    i++;
                for (var j = last_path.length - i; j > 0; j--) {
                    indent = indent[0:-2];
                    stdout.printf (indent + "</Directory>\n");
                }
            }
            for (; i < path.length; i++) {
                stdout.printf (indent + "<Directory Id=\"%s\" Name=\"%s\">\n".printf (random_id ("dir"), path[i]));
                indent += "  ";
            }
            last_path = path;

            if (!is_directory) {
                var id = generate_id ("cmp", 1, file);
                cmpref.append (id);
                if (win64)
                    stdout.printf (indent + "<Component Win64=\"$(var.Win64)\" Id=\"%s\" Guid=\"*\">\n".printf (id));
                else
                    stdout.printf (indent + "<Component Id=\"%s\" Guid=\"*\">\n".printf (id));
                file = sourcedir + Path.DIR_SEPARATOR_S + escape_filename(file);
                stdout.printf (indent + "  <File Id=\"%s\" KeyPath=\"yes\" Source=\"%s\"/>\n".printf (generate_id ("fil", 1, file), file));
                stdout.printf (indent + "</Component>\n");
            }
        }
    } catch (GLib.Error error) {
        warning (error.message);
    }

    indent = indent[0:-2];
    foreach (var l in last_path) {
      stdout.printf (indent + "</Directory>\n");
      indent = indent[0:-2];
    }

    stdout.printf ("    </DirectoryRef>\n");
    stdout.printf ("  </Fragment>\n");
    if (cg != null) {
        stdout.printf ("  <Fragment>\n");
        stdout.printf ("    <ComponentGroup Id=\"%s\">\n".printf (cg));
        foreach (var id in cmpref)
            stdout.printf ("      <ComponentRef Id=\"%s\"/>\n".printf (id));
        stdout.printf ("    </ComponentGroup>\n");
        stdout.printf ("  </Fragment>\n");
    }

    stdout.printf ("</Wix>\n");
    stdout.printf ("<!-- generated with %s -->\n", Config.PACKAGE_STRING);
    stdout.printf ("<!-- %s -->\n", cmdline.replace ("--", "-"));
    return 0;
}
