using Posix;

namespace Wixl {

    static bool version;
    [CCode (array_length = false, array_null_terminated = true)]
    static string[] files;

    private const OptionEntry[] options = {
        { "version", 0, 0, OptionArg.NONE, ref version, N_("Display version number"), null },
        { "", 0, 0, OptionArg.FILENAME_ARRAY, ref files, null, N_("OUTPUT_FILE INPUT_FILE...") },
        { null }
    };

    int main (string[] args) {
        Intl.bindtextdomain (Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
        Intl.bind_textdomain_codeset (Config.GETTEXT_PACKAGE, "UTF-8");
        Intl.textdomain (Config.GETTEXT_PACKAGE);
        GLib.Environment.set_application_name (Config.PACKAGE_NAME);

        var parameter_string = _("- a msi building tool");
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

        if (files.length < 2) {
            GLib.stderr.printf (_("Please specify output and input files.\n"));
            exit (1);
        }

        try {
            var builder = new WixBuilder ();
            foreach (var arg in files[1:files.length]) {
                print ("Loading %s...\n", arg);
                var file = File.new_for_commandline_arg (arg);
                string data;
                FileUtils.get_contents (file.get_path (), out data);
                builder.load_xml (data);
                builder.add_path (file.get_parent ().get_path ());
            }

            print ("Building %s...\n", files[0]);
            var msi = builder.build ();
            msi.build (files[0]);
        } catch (GLib.Error error) {
            printerr (error.message + "\n");
            return 1;
        }

        return 0;
    }

} // Wixl
