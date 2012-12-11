#include <glib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define todo_wine if(0)

#define ok(cond, desc, ...) _ok(cond, #cond, desc, ## __VA_ARGS__ )

static inline void _ok(bool cond, const char *cond_str, const char *str, ...)
{
    va_list ap;
    va_start(ap, str);
    if (!cond) {
        printf("FAIL: %s\n  ", cond_str);
        vprintf(str, ap);
    } else {
        printf("ok: %s\n", cond_str);
    }
    va_end(ap);
}

static inline void check_record_string(LibmsiRecord *rec, unsigned field, const gchar *val)
{
    gchar *str;

    str = libmsi_record_get_string (rec, field);
    if (val == NULL) {
        ok (!str, "Should return null\n");
    } else {
        ok (str, "expected string\n", str);
        if (str)
            ok (g_str_equal (str, val), "got %s != %s expected\n", str, val);
    }

    g_free (str);
}
