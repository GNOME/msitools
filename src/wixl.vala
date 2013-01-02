namespace Wixl {

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
            root.load_xml (doc);
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
