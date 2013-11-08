namespace Wixl {

    public errordomain Error {
        FAILED,
        FIXME,
    }

    namespace UUID {
        [CCode (cname = "uuid_generate", cheader_filename = "uuid/uuid.h")]
        internal extern static void generate ([CCode (array_length = false)] uchar[] uuid);
        [CCode (cname = "uuid_unparse", cheader_filename = "uuid/uuid.h")]
        internal extern static void unparse ([CCode (array_length = false)] uchar[] uuid,
                                             [CCode (array_length = false)] uchar[] output);
    }

    public string uuid_generate () {
        var udn = new uchar[50];
        var id = new uchar[16];

        UUID.generate (id);
        UUID.unparse (id, udn);

        return (string) udn;
    }

    public G enum_from_string<G> (string str) throws GLib.Error {
        var k = (EnumClass)typeof(G).class_ref ();
        var v = k.get_value_by_nick (str);

        if (v == null)
            throw new Wixl.Error.FAILED ("Can't convert string to enum");
        return v.value;
    }

    public void hash_table_add<G> (HashTable<G, G*> h, G o) {
        h.replace (o, o);
    }

    public string add_braces (string str) {
        if (str[0] == '{')
            return str;

        return "{" + str + "}";
    }

    public string get_uuid (owned string uuid) throws GLib.Error {
        if (uuid == "*")
            uuid = uuid_generate ();
        uuid = add_braces (uuid);
        uuid = uuid.up ();
        // FIXME: validate
        return uuid;
    }

    public long now () {
        var tv = TimeVal ();
        tv.get_current_time ();
        return tv.tv_sec;
    }

    public uint64 time_to_filetime (long t) {
        return (t + 134774ULL * 86400ULL) * 10000000ULL;
    }

    public string indent (string space, string text) {
        var indented = "";

        foreach (var l in text.split ("\n")) {
            if (indented.length != 0)
                indented += "\n";

            if (l.length != 0)
                indented += space + l;
        }

        return indented;
    }

    public string generate_id (string prefix, uint n, ...) {
        var l = va_list ();
        var args = new string[n];

        for (var i = 0; n > 0; n--) {
            string? val = l.arg ();
            if (val == null)
                continue;
            args[i] = val; // FIXME: misc vala bug when +=
            i += 1;
        }
        var data = string.joinv ("|", args);
        var hash = Checksum.compute_for_string (ChecksumType.SHA1, data);
        var str = prefix + hash[0:32].up ();

        return str;
    }

    public string random_id (string prefix) {
        var data = new uint32[8] {};
        for (var i = 0; i < 8; i++)
            data[i] = Random.next_int ();

        var hash = Checksum.compute_for_data (ChecksumType.SHA1, (uint8[])data);

        return prefix + hash[0:32].up ();
    }

    public string yesno (bool yes) {
        return yes ? "yes" : "no";
    }

    public bool parse_yesno (string? str, bool default = false) {
        if (str == null)
            return default;

        return (str[0] == 'Y' || str[0] == 'y');
    }

    public string unquote (string str) {
        if ((str[0] == '\'' && str[str.length-1] == '\'') ||
            (str[0] == '"' && str[str.length-1] == '"'))
            return str[1:-1];

        return str;
    }

    public string remove_prefix (string prefix, string str) {
        if (str.has_prefix (prefix))
            return str[prefix.length:str.length];

        return str;
    }

    public int find_closing_paren (string str) {
        return_val_if_fail (str[0] == '(', -1);

        var open_count = 1;
        var close_count = 0;
        for (var pos = 1;  pos < str.length; pos++) {
            if (str[pos] == '(')
                open_count++;
            else if (str[pos] == ')') {
                close_count++;
                if (open_count == close_count)
                    return pos;
            }
        }

        return -1;
    }

    public class UnixInputStream : GLib.InputStream {
        public int fd { get; set construct; }

        public UnixInputStream(int fd_) {
            fd = fd_;
        }

        public override ssize_t read ([CCode (array_length_type = "gsize")] uint8[] buffer, GLib.Cancellable? cancellable = null) throws GLib.IOError {
            return Posix.read(fd, buffer, buffer.length);
        }

        public override bool close(GLib.Cancellable? cancellable = null) throws GLib.IOError {
            return true;
        }
    }
} // Wixl
