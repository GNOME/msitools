/*
 * libFuzzer harness for wixl_get_file_version_data().
 *
 * Build and run with:
 *   CC=clang meson setup build-fuzz -Dfuzzing=true
 *   ninja -C build-fuzz tests/fuzz-version
 *   build-fuzz/tests/fuzz-version tests/data/wixl/Versioned.dll
 *
 * The harness checks that a successful parse returns a canonical four-part
 * version and, when present, a comma-separated list of canonical LANGIDs.
 */

#include <glib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "libcommon.h"

static gboolean
valid_component (const gchar *start,
                 const gchar *end)
{
    guint value = 0;
    gsize len = end - start;

    if (len == 0 || len > 5)
        return FALSE;
    /* the parser uses %u, so leading zeroes are never produced */
    if (len > 1 && start[0] == '0')
        return FALSE;

    for (const gchar *p = start; p < end; p++) {
        if (*p < '0' || *p > '9')
            return FALSE;
        value = value * 10 + (*p - '0');
    }

    return value <= 0xffff;
}

static void
check_version (const gchar *version)
{
    const gchar *p = version;

    g_assert_nonnull (version);

    for (guint i = 0; i < 4; i++) {
        const gchar *dot = strchr (p, '.');

        if (i < 3) {
            g_assert_nonnull (dot);
            g_assert_true (valid_component (p, dot));
            p = dot + 1;
        } else {
            g_assert_null (dot);
            g_assert_true (valid_component (p, p + strlen (p)));
        }
    }
}

static void
check_language (const gchar *language)
{
    const gchar *p = language;

    if (language == NULL)
        return;

    g_assert_true (*p != '\0');

    for (;;) {
        const gchar *comma = strchr (p, ',');
        const gchar *end = comma != NULL ? comma : p + strlen (p);

        g_assert_true (valid_component (p, end));

        if (comma == NULL)
            break;
        p = comma + 1;
    }
}

int
LLVMFuzzerTestOneInput (const uint8_t *data,
                        size_t size)
{
    gchar *version = NULL;
    gchar *language = NULL;
    gboolean found;

    if (size > G_MAXINT)
        return 0;

    found = wixl_get_file_version_data ((guint8 *) data, (gint) size,
                                        &version, &language);

    if (!found) {
        g_assert_null (version);
        g_assert_null (language);
        return 0;
    }

    check_version (version);
    check_language (language);

    g_free (version);
    g_free (language);

    return 0;
}
