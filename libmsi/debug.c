#include <glib.h>
#include "debug.h"

G_GNUC_INTERNAL
const char *wine_dbg_sprintf( const char *format, ...)
{
    static char *p_ret[10];
    static int i;

    char *ret;
    unsigned len;
    va_list ap;

    va_start(ap, format);
    ret = g_strdup_vprintf(format, ap);
    len = strlen(ret);
    va_end(ap);

    i = (i + 1) % 10;
    p_ret[i] = realloc(p_ret[i], len + 1);
    strcpy(p_ret[i], ret);
    g_free(ret);
    return p_ret[i];
}

