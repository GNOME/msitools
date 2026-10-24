#include <glib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define todo_wine if(0)

#define ok(cond, desc, ...) _ok(cond, #cond, desc, ## __VA_ARGS__ )

static int test_count;
static int test_failures;

static inline void _ok(bool cond, const char *cond_str, const char *str, ...)
{
    va_list ap;

    test_count++;
    if (!cond) {
        test_failures++;
        printf("not ok %d - %s\n", test_count, cond_str);
        printf("# ");
        va_start(ap, str);
        vprintf(str, ap);
        va_end(ap);
    } else {
        printf("ok %d - %s\n", test_count, cond_str);
    }
}

static inline int test_report(void)
{
    printf("1..%d\n", test_count);
    if (test_failures)
        printf("# %d test(s) failed\n", test_failures);
    return test_failures ? 1 : 0;
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
