namespace Wixl {

    public errordomain Error {
        FAILED,
        FIXME,
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
            uuid = Uuid.string_random ();
        uuid = add_braces (uuid);
        uuid = uuid.up ();
        // FIXME: validate
        return uuid;
    }

    public int64 now () {
        return get_real_time() / TimeSpan.SECOND;
    }

    public uint64 time_to_filetime (int64 t) {
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

    /* Namespace UUID: {de73ba5a-ed96-4a66-ba1b-fbb44e659ad7} */
    const string uuid_namespace =
        "\xde\x73\xba\x5a\xed\x96\x4a\x66\xba\x1b\xfb\xb4\x4e\x65\x9a\xd7";

    public static string uuid_from_name(string s) {
        var cs = new Checksum (ChecksumType.SHA1);
        uint8 buffer[20];
        size_t buflen = buffer.length;

        cs.update (uuid_namespace.data, 16);
        cs.update (s.data, s.length);
        cs.get_digest (buffer, ref buflen);

        return "{%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X}".
            printf(buffer[0], buffer[1], buffer[2], buffer[3],
                   buffer[4], buffer[5], (buffer[6] & 15) | 0x50, buffer[7],
                   (buffer[8] & 0x3F) | 0x80, buffer[9], buffer[10], buffer[11],
                   buffer[12], buffer[13], buffer[14], buffer[15]);
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

    public void compute_md5 (File file, ref int hash1, ref int hash2, ref int hash3, ref int hash4) throws GLib.Error {
        var checksum = new Checksum (ChecksumType.MD5);
        var stream = file.read ();
        uint8 fbuf[4096];
        size_t size;

        while ((size = stream.read (fbuf)) > 0) {
            checksum.update (fbuf, size);
        }

        int buffer[4];
        size_t buflen = 16;
        checksum.get_digest ((uint8[])buffer, ref buflen);
        hash1 = buffer[0];
        hash2 = buffer[1];
        hash3 = buffer[2];
        hash4 = buffer[3];
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
