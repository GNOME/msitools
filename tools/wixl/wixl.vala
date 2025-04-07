using Posix;

namespace Wixl {

    static bool version;
    static bool verbose;
    static bool preproc;
    static string output;
    [CCode (array_length = false, array_null_terminated = true)]
    static string[] files;
    [CCode (array_length = false, array_null_terminated = true)]
    static string[] defines;
    [CCode (array_length = false, array_null_terminated = true)]
    static string[] opt_includedirs;
    [CCode (array_length = false, array_null_terminated = true)]
    static string[] opt_extensions;

    static string[] includedirs;
    static string wxidir;
    static string extdir;
    static Arch arch = Arch.X86;
    static Extension[] extensions;

    private const OptionEntry[] options = {
        { "version", 0, 0, OptionArg.NONE, ref version, N_("Display version number"), null },
        { "verbose", 'v', 0, OptionArg.NONE, ref verbose, N_("Verbose output"), null },
        { "output", 'o', 0, OptionArg.FILENAME, ref output, N_("Output file"), null },
        { "define", 'D', 0, OptionArg.STRING_ARRAY, ref defines, N_("Define variable"), null },
        { "arch", 'a', 0, OptionArg.CALLBACK, (void*)parse_arch, N_("Target architecture"), null },
        { "includedir", 'I', 0, OptionArg.STRING_ARRAY, ref opt_includedirs, N_("Include directory"), null },
        { "extdir", 0, 0, OptionArg.STRING, ref extdir, N_("System extension directory"), null },
        { "wxidir", 0, 0, OptionArg.STRING, ref wxidir, N_("System include directory"), null },
        { "only-preproc", 'E', 0, OptionArg.NONE, ref preproc, N_("Stop after the preprocessing stage"), null },
        { "ext", 0, 0, OptionArg.STRING_ARRAY, ref opt_extensions, N_("Specify an extension"), null },
        { "", 0, 0, OptionArg.FILENAME_ARRAY, ref files, null, N_("INPUT_FILE1 [INPUT_FILE2]...") },
        { null }
    };

    bool parse_arch (string option_name, string value, void *data) throws OptionError {
        try {
            arch = Arch.from_string (value);
        } catch (GLib.Error e) {
            throw new OptionError.BAD_VALUE (_("arch of type '%s' is not supported").printf (value));
        }

        return true;
    }

    int main (string[] args) {
        Intl.bindtextdomain (Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
        Intl.bind_textdomain_codeset (Config.GETTEXT_PACKAGE, "UTF-8");
        Intl.textdomain (Config.GETTEXT_PACKAGE);
        GLib.Environment.set_application_name ("wixl");

        var parameter_string = _("- a msi building tool");
        var opt_context = new OptionContext (parameter_string);
        opt_context.set_help_enabled (true);
        opt_context.add_main_entries (options, null);

        wxidir = Path.build_filename (Config.DATADIR, "wixl-" + Config.PACKAGE_VERSION, "include");
        extdir = Path.build_filename (Config.DATADIR, "wixl-" + Config.PACKAGE_VERSION, "ext");

        try {
            opt_context.parse (ref args);
        } catch (OptionError.BAD_VALUE err) {
            GLib.stderr.printf (err.message + "\n");
            exit (1);
        } catch (OptionError error) {
            warning (error.message);
        }

        /* fixme vala, does not support += on arrays without length.  */
        includedirs = opt_includedirs;
        includedirs += wxidir;

        if (version) {
            GLib.stdout.printf ("%s\n", Config.PACKAGE_VERSION);
            exit (0);
        }

        if (files.length < 1) {
            GLib.stderr.printf (_("Please specify input files.\n"));
            exit (1);
        }

        if (output == null && !preproc) {
            if (files[0].has_suffix (".wxs"))
                output = files[0].slice (0, -4) + ".msi";
            else {
                GLib.stderr.printf (_("Please specify the output file.\n"));
                exit (1);
            }
        }

        foreach (var i in opt_extensions) {
            try {
                var extension = Extension.from_string(i);
                extensions += extension;
            } catch (GLib.Error e) {
                GLib.stderr.printf (_("Extension of type '%s' is not supported.\n").printf (i));
                exit (1);
            }
        }

        try {
            var builder = new WixBuilder (includedirs, arch, extensions, extdir);

            foreach (var d in defines) {
                var def = d.split ("=", 2);
                var name = def[0];
                var value = def.length == 2 ? def[1] : "1";
                builder.define_variable (name, value);
            }

            foreach (var arg in files) {
                if (verbose)
                    print (_("Loading %s...\n"), arg);
                var file = File.new_for_commandline_arg (arg);
                builder.load_file (file, preproc);
                builder.add_path (file.get_parent ().get_path ());
            }

            if (preproc)
                return 0;

            if (verbose)
                print (_("Building %s...\n"), output);
            var msi = builder.build ();
            if (verbose)
                print (_("Writing %s...\n"), output);
            msi.build (output);
        } catch (GLib.Error error) {
            if (verbose)
                print (_("A GLib error has occurred\n"));
            printerr (error.domain.to_string() + " " + error.code.to_string() + " " + error.message + "\n");
            return 1;
        }

        return 0;
    }

} // Wixl
