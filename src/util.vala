namespace Wixl {

    public errordomain Error {
        FAILED
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

    string add_braces (string str) {
        return "{" + str + "}";
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

} // Wixl
