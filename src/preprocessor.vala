namespace Wixl {

    class Preprocessor: Object {

        HashTable<string, string> globals;
        HashTable<string, string> variables;
        construct {
            variables = new HashTable<string, string> (str_hash, str_equal);
        }

        public Preprocessor (HashTable<string, string> globals) {
            this.globals = globals;
        }

        public void define_variable (string name, string value) {
            variables.insert (name, value);
        }

        public string? lookup_variable (string name) {
            return variables.lookup (name) ?? globals.lookup (name);
        }

        public string eval_variable (string str, File? file) throws GLib.Error {
            var var = str.split (".", 2);
            if (var.length != 2)
                throw new Wixl.Error.FAILED ("invalid variable %s", str);

            switch (var[0]) {
            case "var":
                var val = lookup_variable (var[1]);
                if (val == null)
                    throw new Wixl.Error.FAILED ("Undefined variable %s", var[1]);
                return val;
            case "env":
                return Environment.get_variable (var[1]);
            case "sys":
                switch (var[1]) {
                case "CURRENTDIR":
                    return Environment.get_current_dir ();
                case "SOURCEFILEDIR":
                    return file.get_basename ();
                case "SOURCEFILEPATH":
                    return file.get_path ();
                }
                break;
            }

            throw new Wixl.Error.FIXME ("unhandled variable type %s", str);
        }

        public string eval (string str, File? file) throws GLib.Error {
            var result = "";
            int end = 0;
            int pos = 0;

            while ((pos = str.index_of ("$", end)) != -1) {
                if (end < pos)
                    result += str[end:pos];
                end = pos + 1;
                var remainder = str[end:str.length];
                if (remainder.has_prefix ("$"))
                    result += "$";
                else if (remainder.has_prefix ("(")) {
                    var closing = find_closing_paren (remainder);
                    if (closing == -1)
                        throw new Wixl.Error.FAILED ("no matching closing parenthesis");
                    var substring = remainder[1:closing];
                    if (substring.index_of ("(") != -1)
                        throw new Wixl.Error.FIXME ("unsupported function");
                    result += eval_variable (substring, file);
                    end += closing + 1;
                }
            }

            return result + str[end:str.length];
        }

        public Xml.Doc preprocess (string data, File? file) throws GLib.Error {
            Xml.Doc doc;
            var writer = new Xml.TextWriter.doc (out doc);
            var reader = new Xml.TextReader.for_doc (data, "");

            writer.start_document ();
            while (reader.read () > 0) {
                switch (reader.node_type ()) {
                case Xml.ReaderType.PROCESSING_INSTRUCTION:
                    switch (reader.const_local_name ()) {
                    case "define":
                        MatchInfo info;
                        var r = /^\s*(?P<name>.+?)\s*=\s*(?P<value>.+?)\s*$/;
                        if (r.match (reader.const_value (), 0, out info)) {
                            var name = remove_prefix ("var.", info.fetch_named ("name"));
                            var value = unquote (info.fetch_named ("value"));
                            define_variable (name, value);
                        } else
                            throw new Wixl.Error.FAILED ("invalid define");
                        break;
                    default:
                        warning ("unhandled preprocessor instruction %s", reader.const_local_name ());
                        break;
                    }
                    break;
                case Xml.ReaderType.ELEMENT:
                    var empty = reader.is_empty_element () > 0;

                    writer.start_element (reader.const_name ());
                    while (reader.move_to_next_attribute () > 0) {
                        var value = eval (reader.const_value (), file);
                        writer.write_attribute (reader.const_name (), value);
                    }

                    if (empty)
                        writer.end_element ();
                    break;
                case Xml.ReaderType.END_ELEMENT:
                    writer.end_element ();
                    break;
                case Xml.ReaderType.TEXT:
                    writer.write_string (eval (reader.const_value(), file));
                    break;
                case Xml.ReaderType.CDATA:
                    writer.write_cdata (eval (reader.const_value(), file));
                    break;
                }
            }
            writer.end_document ();

            return doc;
        }
    }
}