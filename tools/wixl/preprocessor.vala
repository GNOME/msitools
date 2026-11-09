namespace Wixl {

    class Preprocessor: Object {

        unowned List<File> includedirs;
        HashTable<string, string> variables;
        Arch arch;
        public bool buildarch_used { get; private set; }
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

        public string? eval_defined (string str, File? file) throws GLib.Error {
            if (str.index_of (".") == -1)
                return lookup_variable (str);

            return eval_variable (str, file);
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
                    buildarch_used = true;
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
                    accepted = (int) (intptr) scanner.next_value.symbol == symbol;
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

        public bool eval_condition (string str, File? file) throws GLib.Error {
            var evaluator = new EvalCondition (this, file);
            return evaluator.eval (str);
        }

        class Event: Object {
            public Xml.ReaderType type;
            public string name;
            public string? value;
            public string[] attr_names = {};
            public string[] attr_values = {};
            public bool empty;
            public int depth;
            public int line;

            public Event (Xml.TextReader reader) {
                type = (Xml.ReaderType) reader.node_type ();
                depth = reader.depth ();
                line = reader.current_node ()->line;

                switch (type) {
                case Xml.ReaderType.ELEMENT:
                    name = reader.const_name ();
                    empty = reader.is_empty_element () > 0;
                    while (reader.move_to_next_attribute () > 0) {
                        attr_names += reader.const_name ();
                        attr_values += reader.const_value ();
                    }
                    break;
                case Xml.ReaderType.END_ELEMENT:
                    name = reader.const_name ();
                    break;
                case Xml.ReaderType.PROCESSING_INSTRUCTION:
                    name = reader.const_local_name ();
                    value = reader.const_value ();
                    break;
                case Xml.ReaderType.TEXT:
                case Xml.ReaderType.CDATA:
                    value = reader.const_value ();
                    break;
                default:
                    break;
                }
            }
        }

        class Location: Object {
            public File? file;
            public int line;

            public Location (File? file, int line) {
                this.file = file;
                this.line = line;
            }

            public string to_string () {
                var name = file != null ? file.get_basename () : "<input>";
                return "%s:%d".printf (name, line);
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

        class PreprocessState: Object {
            public IfContext context;
            public Queue<IfContext> ifstack;

            public PreprocessState () {
                context = new IfContext (true, true);
                ifstack = new Queue<IfContext> ();
            }
        }

        HashTable<string, string> copy_variables () {
            var copy = new HashTable<string, string> (str_hash, str_equal);
            string name, value;
            var it = HashTableIter<string, string> (variables);
            while (it.next (out name, out value))
                copy.insert (name, value);
            return copy;
        }

        void restore_variables (HashTable<string, string> copy) {
            variables.remove_all ();
            string name, value;
            var it = HashTableIter<string, string> (copy);
            while (it.next (out name, out value))
                variables.insert (name, value);
        }

        string[] foreach_values (string directive, File? file, out string variable) throws GLib.Error {
            MatchInfo info;
            var r = /^\s*(?P<name>\S+)\s+in\s+(?P<list>.*?)\s*$/;
            if (!r.match (directive, 0, out info))
                throw new Wixl.Error.FAILED ("invalid foreach");

            variable = remove_prefix ("var.", info.fetch_named ("name"));
            var list = eval (info.fetch_named ("list").strip (), file);
            var values = list.split (";");
            for (var i = 0; i < values.length; i++)
                values[i] = unquote (values[i].strip ());
            return values;
        }

        int find_endforeach (GenericArray<Event> events, uint start, uint end) throws GLib.Error {
            int depth = 0;
            for (uint i = start; i < end; i++) {
                var event = events.get (i);
                if (event.type != Xml.ReaderType.PROCESSING_INSTRUCTION)
                    continue;
                if (event.name == "foreach") {
                    depth++;
                } else if (event.name == "endforeach") {
                    if (depth == 0)
                        return (int) i;
                    depth--;
                }
            }

            throw new Wixl.Error.FAILED ("Missing endforeach");
        }

        void preprocess_events (GenericArray<Event> events, uint start, uint end,
                                Xml.TextWriter writer, File? file, PreprocessState state,
                                bool is_include) throws GLib.Error {
            for (uint i = start; i < end; i++) {
                var event = events.get (i);
                var loc = new Location (file, event.line);
                bool handled = false;

                if (event.type == Xml.ReaderType.PROCESSING_INSTRUCTION && event.name == "foreach") {
                    var endloc = find_endforeach (events, i + 1, end);
                    if (state.context.enabled && state.context.is_true) {
                        string variable;
                        var values = foreach_values (event.value ?? "", file, out variable);
                        var saved = copy_variables ();
                        foreach (var value in values) {
                            restore_variables (saved);
                            define_variable (variable, value);
                            preprocess_events (events, i + 1, (uint) endloc, writer, file, state, is_include);
                        }
                        restore_variables (saved);
                    }
                    i = (uint) endloc;
                    continue;
                }

                if (event.type == Xml.ReaderType.PROCESSING_INSTRUCTION) {
                    handled = true;

                    switch (event.name) {
                    case "ifdef":
                        state.ifstack.push_head (state.context);
                        var value = (event.value ?? "").strip ();
                        state.context = new IfContext (state.context.enabled && state.context.is_true, eval_defined (value, file) != null, IfContext.State.IF);
                        break;
                    case "ifndef":
                        state.ifstack.push_head (state.context);
                        var value = (event.value ?? "").strip ();
                        state.context = new IfContext (state.context.enabled && state.context.is_true, eval_defined (value, file) == null, IfContext.State.IF);
                        break;
                    case "else":
                        if (state.ifstack.is_empty ())
                            throw new Wixl.Error.FAILED ("Unmatched else");
                        if (!state.context.state.in_if ())
                            throw new Wixl.Error.FAILED ("Unmatched else");
                        state.context.state = IfContext.State.ELSE;
                        state.context.is_true = state.context.never_true;
                        break;
                    case "endif":
                        if (state.ifstack.is_empty ())
                            throw new Wixl.Error.FAILED ("Unmatched endif");
                        state.context = state.ifstack.pop_head ();
                        break;
                    case "if":
                        state.ifstack.push_head (state.context);
                        var value = (event.value ?? "").strip ();
                        state.context = new IfContext (state.context.enabled && state.context.is_true, eval_condition (value, file), IfContext.State.IF);
                        break;
                    case "endforeach":
                        throw new Wixl.Error.FAILED ("Unmatched endforeach");
                    default:
                        handled = false;
                        break;
                    }
                }

                if (handled || !state.context.enabled || !state.context.is_true)
                    continue;

                switch (event.type) {
                case Xml.ReaderType.PROCESSING_INSTRUCTION:
                    switch (event.name) {
                    case "define":
                        MatchInfo info;
                        var r = /^\s*(?P<name>.+?)\s*=\s*(?P<value>.+?)\s*$/;
                        if (r.match (event.value ?? "", 0, out info)) {
                            var name = remove_prefix ("var.", info.fetch_named ("name"));
                            var value = unquote (info.fetch_named ("value"));
                            define_variable (name, value);
                        } else
                            throw new Wixl.Error.FAILED ("invalid define");
                        break;
                    case "undef":
                        var value = (event.value ?? "").strip ();
                        undefine_variable (value);
                        break;
                    case "require":
                        var value = unquote (eval (event.value ?? "", file).strip ());
                        include (value, loc, writer, true);
                        break;
                    case "include":
                        var value = unquote (eval (event.value ?? "", file).strip ());
                        include (value, loc, writer);
                        break;
                    case "warning":
                        print ("warning", loc, eval (event.value ?? "", file));
                        break;
                    case "ignore":
                        // wixl-heat writes its version and command line as <?ignore ...?>
                        break;
                    case "error":
                        print ("error", loc, eval (event.value ?? "", file));
                        Posix.exit (1);
                        break;
                    default:
                        warning ("unhandled preprocessor instruction %s", event.name);
                        break;
                    }
                    break;
                case Xml.ReaderType.ELEMENT:
                    if (is_include && event.depth == 0 && event.name == "Include")
                        break;

                    writer.start_element (event.name);
                    for (var j = 0; j < event.attr_names.length; j++)
                        writer.write_attribute (event.attr_names[j], eval (event.attr_values[j], file));

                    if (event.empty)
                        writer.end_element ();
                    break;
                case Xml.ReaderType.END_ELEMENT:
                    if (is_include && event.depth == 0 && event.name == "Include")
                        break;

                    writer.end_element ();
                    break;
                case Xml.ReaderType.TEXT:
                    writer.write_string (eval (event.value ?? "", file));
                    break;
                case Xml.ReaderType.CDATA:
                    writer.write_cdata (eval (event.value ?? "", file));
                    break;
                default:
                    break;
                }
            }
        }

        void preprocess_xml (Xml.TextReader reader, Xml.TextWriter writer, File? file, bool is_include = false) throws GLib.Error {
            var events = new GenericArray<Event> ();
            int status;

            while ((status = reader.read ()) > 0)
                events.add (new Event (reader));

            if (status != 0)
                throw new Wixl.Error.FAILED ("Failed to parse XML");

            var state = new PreprocessState ();
            preprocess_events (events, 0, (uint) events.length, writer, file, state, is_include);

            if (!state.ifstack.is_empty ())
                throw new Wixl.Error.FAILED ("Missing endif");
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
            if (loc.file != null)
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
