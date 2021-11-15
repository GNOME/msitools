namespace Wixl {

    class Preprocessor: Object {

        unowned List<File> includedirs;
        HashTable<string, string> variables;
        Arch arch;
        construct {
            variables = new HashTable<string, string> (str_hash, str_equal);
        }

        public Preprocessor (HashTable<string, string> globals, List<File> includedirs, Arch arch) {
            string name, value;
            var it = HashTableIter <string, string> (globals);
            while (it.next (out name, out value))
                define_variable (name, value);

            this.includedirs = includedirs;
            this.arch = arch;
        }

        public void define_variable (string name, string value) {
            variables.insert (name, value);
        }

        public void undefine_variable (string name) {
            variables.remove (name);
        }

        public string? lookup_variable (string name) {
            return variables.lookup (name);
        }

        public string? eval_variable (string str, File? file) throws GLib.Error {
            var var = str.split (".", 2);
            if (var.length != 2)
                throw new Wixl.Error.FAILED ("invalid variable %s", str);

            switch (var[0]) {
            case "var":
                return lookup_variable (var[1]);
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
                case "BUILDARCH":
                    return arch.to_string();
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
                    var val = eval_variable (substring, file);
                    if (val == null)
                        throw new Wixl.Error.FAILED ("Undefined variable %s", substring);
                    result += val;
                    end += closing + 1;
                }
            }

            return result + str[end:str.length];
        }

        class EvalCondition: Object {
            File? file;
            Preprocessor preprocessor;
            Scanner scanner;

            enum Op {
                NONE,
                EQUAL,

                OPEN_VAR,
                EQ,
                EC,
                NE,
                LT,
                LTE,
                GT,
                GTE,
                AND,
                OR,
                NOT,
            }

            construct {
                scanner = new Scanner (null);
                scanner.config.scan_identifier_1char = true;
                scanner.config.cset_identifier_nth =
                    (string)scanner.config.cset_identifier_nth + ".";
                scanner.scope_add_symbol (0, "and", (void*)Op.AND);
                scanner.scope_add_symbol (0, "or", (void*)Op.OR);
                scanner.scope_add_symbol (0, "not", (void*)Op.NOT);
            }

            public EvalCondition (Preprocessor pp, File? file = null) {
                this.file = file;
                this.preprocessor = pp;
            }

            bool to_bool (Value val) {
                Value result = false;
                val.transform (ref result);
                return (bool) result;
            }

            bool accept (int type) {
                var accepted = (int)scanner.peek_next_token () == type;
                if (accepted)
                    scanner.get_next_token ();
                return accepted;
            }

            bool expect (int type) throws GLib.Error {
                if (!accept (type))
                    throw new Wixl.Error.FAILED ("Expected token %d", type);

                return true;
            }

            bool accept_symbol (Op symbol) {
                bool accepted = false;
                if (scanner.peek_next_token () == TokenType.SYMBOL) {
                    accepted = (int)scanner.next_value.symbol == symbol;
                }

                if (accepted)
                    scanner.get_next_token ();
                return accepted;
            }

            Value term () throws GLib.Error {
                if (accept (TokenType.LEFT_PAREN)) {
                    Value val = expr ();
                    expect (TokenType.RIGHT_PAREN);
                    return val;
                } else if (accept (TokenType.INT)) {
                    return scanner.cur_value ().int;
                } else if (accept (TokenType.STRING)) {
                    return scanner.cur_value ().string;
                } else if (accept (TokenType.IDENTIFIER)) {
                    return scanner.cur_value ().identifier;
                } else if (accept ('$')) {
                    if (accept (TokenType.LEFT_PAREN)) {
                        expect (TokenType.IDENTIFIER);
                        var val = preprocessor.eval_variable (scanner.cur_value ().identifier, file);
                        expect (TokenType.RIGHT_PAREN);
                        return val;
                    }
                }

#if 0
                debug ("term %d", scanner.peek_next_token ());
                if (accept (TokenType.SYMBOL))
                    message ("symbol: %d", (int)scanner.cur_value ().symbol);
#endif

                throw new Wixl.Error.FAILED ("Invalid term");
            }

            int to_int (Value val) {
                Value result = 0;
                val.transform (ref result);
                return result.get_int ();
            }

            string to_string (Value val) {
                Value result = "";
                val.transform (ref result);
                return result.get_string ();
            }

            bool cmp_term () throws GLib.Error {
                var op = Op.NONE;
                Value left = term ();

                if (accept ('<'))
                    op = Op.LT;
                else if (accept ('>'))
                    op = Op.GT;
                else if (accept (TokenType.EQUAL_SIGN))
                    return to_string (left) == to_string (term ());
                else if (accept ('!') && expect ('='))
                    return to_string (left) != to_string (term ());
                else if (accept ('~') && expect ('=')) {
                    return to_string (left).ascii_casecmp (to_string (term ())) == 0;
                }

                if (op != Op.NONE) {
                    bool eq = false;
                    if (accept ('='))
                        eq = true;

                    var a = to_int (left);
                    var b = to_int (term ());

                    if (eq && a == b)
                        return true;

                    if (op == Op.GT)
                        return a > b;
                    else if (op == Op.LT)
                        return a < b;
                }

                return to_bool (left);
            }

            bool not_term () throws GLib.Error {
                if (accept_symbol (Op.NOT))
                    return !not_term ();
                else
                    return cmp_term ();
            }

            bool expr () throws GLib.Error {
                bool handled = false;

                var result = not_term ();
                do {
                    // TODO: I think wixl condition are left-evaled
                    handled = true;
                    if (accept_symbol (Op.AND))
                        result = not_term () && result;
                    else if (accept_symbol (Op.OR))
                        result = not_term () || result;
                    else
                        handled = false;
                } while (handled);

                return result;
            }

            public bool eval (string str) throws GLib.Error {
                scanner.input_text (str, str.length);
                var result = expr ();

                if (!accept (TokenType.EOF))
                    warning ("condition invaluation incomplete, next token %d",
                             scanner.peek_next_token ());

                return result;
            }
        }

#if 0
       void test (string v, bool expect = true) {
            var evaluator = new EvalCondition (this);

            message (v);
            if (evaluator.eval (v) != expect) {
                error ("expected %s", yesno (expect));
            }
       }

       void tests () {
           Environment.set_variable ("FOO", "bar", true);
           test ("1");
           test ("0", false);
           test ("not 1", false);
           test ("not not 1");
           test ("((not (not (1))))");
           test ("\"not1\"", false);
           test ("\"foo\"", false);
           test ("\"\"", false);
           test ("\"foo\" = \"foo\"");
           test ("1 = 1");
           test ("1 = 2", false);
           test ("1 != 2");
           test ("1 < 2");
           test ("1 <= 2");
           test ("2 <= 2");
           test ("2 >= 2");
           test ("2 >= 1");
           test ("2 > 1");
           test ("\"foo\" ~= \"fOo\"");
           test ("\"foo\" = \"bar\"", false);
           test ("not (1 != 2)", false);
           test ("0 or 0", false);
           test ("0 or 1", true);
           test ("0 and 1", false);
           test ("1 and 1", true);
           test ("not (1 != 2) or (1 != 2)", true);
           test ("$(env.FOO) = \"bar\"", true);
           test ("$(env.FOO) = bar", true);
           Environment.set_variable ("FOO", "2", true);
           test ("$(env.FOO) < 3", true);
       }
#endif

        public bool eval_condition (string str, File? file) throws GLib.Error {
            var evaluator = new EvalCondition (this, file);
            return evaluator.eval (str);
        }

        class Location: Object {
            public File file;
            public int line;

            public Location (Xml.TextReader reader, File file) {
                this.file = file;
                var node = reader.current_node ();
                this.line = node->line;
            }

            public string to_string () {
                return "%s:%d".printf (file.get_basename (), line);
            }
        }

        void print (string prefix, Location loc, string msg) {
            stderr.printf (loc.to_string () + ": " + prefix + ": " + msg + "\n");
        }

        class IfContext {
            public enum State {
                UNKNOWN,
                IF,
                ELSE,
                ELSEIF;

                public bool in_if () {
                    return this == IF || this == ELSEIF;
                }
            }

            bool _is_true;
            public bool is_true {
                get {
                    return _is_true;
                }
                set {
                    if (value)
                        never_true = false;
                    _is_true = value;
                }
            }

            public bool never_true = true;
            public bool enabled;
            public State state;

            public IfContext (bool enabled = false, bool is_true = false, State state = State.UNKNOWN) {
                this.enabled = enabled;
                this.is_true = is_true;
                this.state = state;
            }
        }

        void preprocess_xml (Xml.TextReader reader, Xml.TextWriter writer, File? file, bool is_include = false) throws GLib.Error {
            IfContext context = new IfContext (true, true);
            var ifstack = new Queue<IfContext> ();
            int status;

            while ((status = reader.read ()) > 0) {
                var loc = new Location (reader, file);
                bool handled = false;

                if (reader.node_type () == Xml.ReaderType.PROCESSING_INSTRUCTION) {
                    handled = true;

                    switch (reader.const_local_name ()) {
                    case "ifdef":
                        ifstack.push_head (context);
                        var value = reader.const_value ().strip ();
                        context = new IfContext (context.enabled && context.is_true, eval_variable (value, file) != null, IfContext.State.IF);
                        break;
                    case "ifndef":
                        ifstack.push_head (context);
                        var value = reader.const_value ().strip ();
                        context = new IfContext (context.enabled && context.is_true, eval_variable (value, file) == null, IfContext.State.IF);
                        break;
                    case "else":
                        if (ifstack.is_empty ())
                            throw new Wixl.Error.FAILED ("Unmatched else");
                        if (!context.state.in_if ())
                            throw new Wixl.Error.FAILED ("Unmatched else");
                        context.state = IfContext.State.ELSE;
                        context.is_true = context.never_true;
                        break;
                    case "endif":
                        if (ifstack.is_empty ())
                            throw new Wixl.Error.FAILED ("Unmatched endif");
                        context = ifstack.pop_head ();
                        break;
                    case "if":
                        ifstack.push_head (context);
                        var value = reader.const_value ().strip ();
                        context = new IfContext (context.enabled && context.is_true, eval_condition (value, file), IfContext.State.IF);
                        break;
                    default:
                        handled = false;
                        break;
                    }
                }

                if (handled || !context.enabled || !context.is_true)
                    continue;

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
                    case "undef":
                        var value = reader.const_value ().strip ();
                        undefine_variable (value);
                        break;
                    case "require":
                        var value = eval (reader.const_value (), file).strip ();
                        include (value, loc, writer, true);
                        break;
                    case "include":
                        var value = eval (reader.const_value (), file).strip ();
                        include (value, loc, writer);
                        break;
                    case "warning":
                        print ("warning", loc, eval (reader.const_value (), file));
                        break;
                    case "error":
                        print ("error", loc, eval (reader.const_value (), file));
                        Posix.exit (1);
                        break;
                    default:
                        warning ("unhandled preprocessor instruction %s", reader.const_local_name ());
                        break;
                    }
                    break;
                case Xml.ReaderType.ELEMENT:
                    var empty = reader.is_empty_element () > 0;
                    if (is_include && reader.depth () == 0 &&
                        reader.const_name () == "Include")
                        break;

                    writer.start_element (reader.const_name ());
                    while (reader.move_to_next_attribute () > 0) {
                        var value = eval (reader.const_value (), file);
                        writer.write_attribute (reader.const_name (), value);
                    }

                    if (empty)
                        writer.end_element ();
                    break;
                case Xml.ReaderType.END_ELEMENT:
                    if (is_include && reader.depth () == 0 &&
                        reader.const_name () == "Include")
                        break;

                    writer.end_element ();
                    break;
                case Xml.ReaderType.TEXT:
                    writer.write_string (eval (reader.const_value (), file));
                    break;
                case Xml.ReaderType.CDATA:
                    writer.write_cdata (eval (reader.const_value (), file));
                    break;
                }
            }

            if (!ifstack.is_empty ())
                throw new Wixl.Error.FAILED ("Missing endif");
            if (status != 0)
                throw new Wixl.Error.FAILED ("Failed to parse XML");
        }

        bool include_try (string filename, Xml.TextWriter writer) throws GLib.Error {
            string data;
            var file = File.new_for_path (filename);

            try {
                FileUtils.get_contents (filename, out data);
            } catch (GLib.FileError error) {
                return false;
            }

            var reader = new Xml.TextReader.for_doc (data, "");
            preprocess_xml (reader, writer, file, true);
            return true;
        }

        // Use it as a set
        HashTable<string,string*> requires = new HashTable<string, string*> (str_hash, str_equal);
        void include (string name, Location loc, Xml.TextWriter writer, bool is_req = false) throws GLib.Error {
            var success = false;

            if (is_req) {
                if (requires.lookup_extended (name, null, null))
                    return;
                hash_table_add (requires, name);
            }

            string[] dirs = {};
            dirs += name;
            foreach (var dir in includedirs)
                dirs += dir.get_child (name).get_path ();
            dirs += loc.file.get_parent ().get_child (name).get_path ();

            foreach (var inc in dirs) {
                success = include_try (inc, writer);
                if (success)
                    break;
            }
            if (!success) {
                print ("error", loc, "Failed to include %s".printf (name));
                Posix.exit (1);
            }
        }

        public Xml.Doc preprocess (string data, File? file) throws GLib.Error {
            Xml.Doc doc;
            Xml.TextWriter writer = new Xml.TextWriter.doc (out doc);
            var reader = new Xml.TextReader.for_doc (data, "");

            writer.start_document ();
            preprocess_xml (reader, writer, file);
            writer.end_document ();

            return doc;
        }
    }
}
