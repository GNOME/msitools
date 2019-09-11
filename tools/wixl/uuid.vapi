namespace UUID {
    [CCode (cname = "uuid_generate", cheader_filename = "uuid/uuid.h")]
    internal extern static void generate ([CCode (array_length = false)] uchar[] uuid);
    [CCode (cname = "uuid_unparse", cheader_filename = "uuid/uuid.h")]
    internal extern static void unparse ([CCode (array_length = false)] uchar[] uuid,
                                         [CCode (array_length = false)] uchar[] output);
}
