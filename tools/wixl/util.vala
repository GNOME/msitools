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

    string uuid_generate () {
        var udn = new uchar[50];
        var id = new uchar[16];

        UUID.generate (id);
        UUID.unparse (id, udn);

        return (string) udn;
    }

    int enum_from_string (Type t, string str) throws GLib.Error {
        var k = (EnumClass)t.class_ref ();
        var v = k.get_value_by_nick (str);

        if (v == null)
            throw new Wixl.Error.FAILED ("Can't convert string to enum");
        return v.value;
    }

    string add_braces (string str) {
        if (str[0] == '{')
            return str;

        return "{" + str + "}";
    }

    string get_uuid (owned string uuid) throws GLib.Error {
        if (uuid == "*")
            uuid = uuid_generate ();
        uuid = add_braces (uuid);
        uuid = uuid.up ();
        // FIXME: validate
        return uuid;
    }

    long now () {
        var tv = TimeVal ();
        tv.get_current_time ();
        return tv.tv_sec;
    }

    uint64 time_to_filetime (long t) {
        return (t + 134774ULL * 86400ULL) * 10000000ULL;
    }

    string get_attribute_content (Xml.Attr *attr) {
        if (attr->children == null)
            return "";

        return attr->children->content;
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
        var hash = Checksum.compute_for_string (ChecksumType.MD5, data);
        var str = prefix + hash[0:32].up ();

        return str;
    }

    bool parse_yesno (string? str, bool default = false) {
        if (str == null)
            return default;

        return (str[0] == 'Y' || str[0] == 'y');
    }

    string unquote (string str) {
        if ((str[0] == '\'' && str[str.length-1] == '\'') ||
            (str[0] == '"' && str[str.length-1] == '"'))
            return str[1:-1];

        return str;
    }

    string remove_prefix (string prefix, string str) {
        if (str.has_prefix (prefix))
            return str[prefix.length:str.length];

        return str;
    }

    int find_closing_paren (string str) {
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
} // Wixl
