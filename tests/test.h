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
