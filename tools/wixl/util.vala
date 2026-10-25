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

    // the build time, or SOURCE_DATE_EPOCH for reproducible builds
    // (https://reproducible-builds.org/specs/source-date-epoch/)
    public int64 now () {
        int64 t;
        if (source_date_epoch (out t))
            return t;
        return get_real_time() / TimeSpan.SECOND;
    }

    bool source_date_epoch_warned = false;

    public bool source_date_epoch (out int64 t) {
        t = 0;
        var epoch = Environment.get_variable ("SOURCE_DATE_EPOCH");
        if (epoch == null || epoch == "")
            return false;

        // decimal digits only, up to the end of year 9999
        uint64 v = 0;
        if (!Regex.match_simple ("^[0-9]{1,12}$", epoch) ||
            !uint64.try_parse (epoch, out v, null, 10) || v > 253402300799) {
            if (!source_date_epoch_warned)
                warning ("ignoring invalid SOURCE_DATE_EPOCH: %s", epoch);
            source_date_epoch_warned = true;
            return false;
        }

        t = (int64) v;
        return true;
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

    uint16 read_u16 (uint8[] data, size_t offset) {
        return data[offset] | (data[offset + 1] << 8);
    }

    uint32 read_u32 (uint8[] data, size_t offset) {
        return data[offset] | (data[offset + 1] << 8) |
            (data[offset + 2] << 16) | ((uint32) data[offset + 3] << 24);
    }

    // version resource blocks are aligned relative to the resource start
    size_t align4 (size_t start, size_t offset) {
        return start + ((offset - start + 3) & ~3);
    }

    // Read a version resource block header: returns the end of the block
    // and sets key and the offset of its value, or returns 0 if invalid.
    size_t version_block (uint8[] data, size_t start, size_t offset, size_t end,
                          out string key, out size_t value) {
        key = "";
        value = 0;
        if (offset > end || end - offset < 6)
            return 0;

        var block_end = offset + read_u16 (data, offset);
        if (block_end > end || block_end < offset + 6)
            return 0;

        var builder = new StringBuilder ();
        var pos = offset + 6;
        for (; pos + 2 <= block_end; pos += 2) {
            var c = read_u16 (data, pos);
            if (c == 0)
                break;
            builder.append_c (c < 128 ? (char) c : '?');
        }
        key = builder.str;
        value = align4 (start, pos + 2);

        return block_end;
    }

    /*
     * Get the file version and languages from the version resource of a
     * Windows PE file, like MsiGetFileVersion () does: the version is
     * VS_FIXEDFILEINFO's file version and the languages are the ones listed
     * in VarFileInfo\Translation. Returns false for any other file.
     */
    public bool get_file_version (File file, out string? version, out string? language) {
        version = null;
        language = null;

        MappedFile map;
        try {
            map = new MappedFile (file.get_path (), false);
        } catch (GLib.Error error) {
            return false;
        }
        if (map.get_length () > int.MAX)
            return false;
        unowned uint8[] data = (uint8[]) map.get_contents ();
        data.length = (int) map.get_length ();

        if (data.length < 64 || data[0] != 'M' || data[1] != 'Z')
            return false;
        size_t pe = read_u32 (data, 0x3c);
        if (pe > data.length - 24 || read_u32 (data, pe) != 0x00004550)
            return false;

        var sections = read_u16 (data, pe + 6);
        var optional = pe + 24;
        var optional_size = read_u16 (data, pe + 20);
        if (optional_size < 2 || optional + optional_size + sections * 40 > data.length)
            return false;

        // the resource table is the third data directory
        size_t directories;
        switch (read_u16 (data, optional)) {
        case 0x10b: // PE32
            directories = optional + 96;
            break;
        case 0x20b: // PE32+
            directories = optional + 112;
            break;
        default:
            return false;
        }
        if (directories + 24 > optional + optional_size || read_u32 (data, directories - 4) < 3)
            return false;

        var section_table = optional + optional_size;
        size_t rva_to_offset (uint32 rva, uint32 size) {
            for (var i = 0; i < sections; i++) {
                var section = section_table + i * 40;
                var address = read_u32 (data, section + 12);
                var raw_size = read_u32 (data, section + 16);
                if (rva < address)
                    continue;
                size_t delta = rva - address;
                if (delta >= raw_size)
                    continue;
                size_t raw = read_u32 (data, section + 20);
                if (raw > data.length || delta > data.length - raw)
                    continue;
                size_t offset = raw + delta;
                if (size > data.length - offset || size > raw_size - delta)
                    return 0;
                return offset;
            }
            return 0;
        }

        var resources = rva_to_offset (read_u32 (data, directories + 16), 16);
        if (resources == 0)
            return false;

        // walk the type (RT_VERSION), name (VS_VERSION_INFO, else the
        // first one) and language (the first one) levels
        uint32 entry = 0;
        var directory = resources;
        int[] ids = { 16, 1, -1 };
        for (var level = 0; level < 3; level++) {
            if (directory > data.length || data.length - directory < 16)
                return false;
            var count = read_u16 (data, directory + 12) + read_u16 (data, directory + 14);
            if (data.length - directory - 16 < (size_t) count * 8)
                return false;
            entry = 0;
            for (var i = 0; i < count; i++) {
                var e = directory + 16 + i * 8;
                if (read_u32 (data, e) == ids[level]) {
                    entry = read_u32 (data, e + 4);
                    break;
                }
            }
            if (entry == 0 && level > 0 && count > 0)
                entry = read_u32 (data, directory + 20);
            // only the last level points to data instead of a directory
            if (entry == 0 || ((entry & 0x80000000) != 0) != (level < 2))
                return false;
            size_t relative = entry & 0x7fffffff;
            if (relative > data.length - resources)
                return false;
            directory = resources + relative;
        }
        if (directory > data.length || data.length - directory < 16)
            return false;

        var size = read_u32 (data, directory + 4);
        var info = rva_to_offset (read_u32 (data, directory), size);
        if (info == 0)
            return false;

        // VS_VERSIONINFO, whose value is a VS_FIXEDFILEINFO
        string key;
        size_t value;
        var info_end = version_block (data, info, info, info + size, out key, out value);
        if (info_end == 0 || key != "VS_VERSION_INFO" || value > info_end ||
            info_end - value < 52 || read_u32 (data, value) != 0xfeef04bd)
            return false;

        var ms = read_u32 (data, value + 8);
        var ls = read_u32 (data, value + 12);
        version = "%u.%u.%u.%u".printf (ms >> 16, ms & 0xffff, ls >> 16, ls & 0xffff);

        var languages = new string[] {};
        var child = align4 (info, value + read_u16 (data, info + 2));
        while (child < info_end) {
            var child_end = version_block (data, info, child, info_end, out key, out value);
            if (child_end == 0)
                break;
            if (key == "VarFileInfo") {
                var v = value;
                while (v < child_end) {
                    var var_end = version_block (data, info, v, child_end, out key, out value);
                    if (var_end == 0)
                        break;
                    if (key == "Translation") {
                        for (var t = value; t + 4 <= var_end; t += 4) {
                            var lang = read_u16 (data, t).to_string ();
                            if (!(lang in languages))
                                languages += lang;
                        }
                    }
                    v = align4 (info, var_end);
                }
            }
            child = align4 (info, child_end);
        }
        if (languages.length > 0)
            language = string.joinv (",", languages);

        return true;
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
