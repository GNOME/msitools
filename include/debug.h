/*
 * Wine debugging interface
 *
 * Copyright 1999 Patrik Stridvall
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __WINE_WINE_DEBUG_H
#define __WINE_WINE_DEBUG_H

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <windef.h>
#include <winbase.h>
#include <winnls.h>
#ifndef GUID_DEFINED
#include <guiddef.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal definitions (do not use these directly)
 */

enum __wine_debug_class
{
    DBCL_FIXME,
    DBCL_ERR,
    DBCL_WARN,
    DBCL_TRACE,

    DBCL_INIT = 7  /* lazy init flag */
};

/*
 * Exported definitions and macros
 */

/* These functions return a printable version of a string, including
   quotes.  The string will be valid for some time, but not indefinitely
   as strings are re-used.  */
static inline const char *wine_dbgstr_an( const char * s, int n )
{
    if (!s) return "";
    return s;
}
static inline const char *wine_dbgstr_wn( const WCHAR *s, int n )
{
    static CHAR *p_ret[10];
    static int i;

    CHAR *ret;
    unsigned len;

    if (!s) return "";
    i = (i + 1) % 10;
    ret = p_ret[i];
    len = WideCharToMultiByte( CP_ACP, 0, s, -1, NULL, 0, NULL, NULL);
    ret = realloc( ret, len );
    if (ret)
        WideCharToMultiByte( CP_ACP, 0, s, -1, ret, len, NULL, NULL );
    return ret;
}

static inline const char *wine_dbg_sprintf( const char *format, ...)
{
    static CHAR *p_ret[10];
    static int i;

    CHAR *ret;
    unsigned len;
    va_list ap;

    va_start(ap, format);
    len = _vscprintf(format, ap);
    va_end(ap);

    i = (i + 1) % 10;
    ret = p_ret[i];
    ret = realloc(ret, len + 1);

    va_start(ap, format);
    vsprintf(ret, format, ap);
    va_end(ap);
    return ret;
}

#define wine_dbg_printf(format,...) (printf(format, ## __VA_ARGS__), fflush(stdout))
#define WINE_DPRINTF(class, function, format, ...) \
     wine_dbg_printf(#class ":%s:" format, function, ## __VA_ARGS__)

static inline const char *wine_dbgstr_a( const char *s )
{
    return wine_dbgstr_an( s, -1 );
}

static inline const char *wine_dbgstr_w( const WCHAR *s )
{
    return wine_dbgstr_wn( s, -1 );
}

static inline const char *wine_dbgstr_guid( const GUID *id )
{
    if (!id) return "(null)";
    if (!((uintptr_t)id >> 16)) return wine_dbg_sprintf( "<guid-0x%04hx>", (WORD)(uintptr_t)id );
    return wine_dbg_sprintf( "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                             id->Data1, id->Data2, id->Data3,
                             id->Data4[0], id->Data4[1], id->Data4[2], id->Data4[3],
                             id->Data4[4], id->Data4[5], id->Data4[6], id->Data4[7] );
}

static inline const char *wine_dbgstr_point( const POINT *pt )
{
    if (!pt) return "(null)";
    return wine_dbg_sprintf( "(%d,%d)", pt->x, pt->y );
}

static inline const char *wine_dbgstr_size( const SIZE *size )
{
    if (!size) return "(null)";
    return wine_dbg_sprintf( "(%d,%d)", size->cx, size->cy );
}

static inline const char *wine_dbgstr_rect( const RECT *rect )
{
    if (!rect) return "(null)";
    return wine_dbg_sprintf( "(%d,%d)-(%d,%d)", rect->left, rect->top,
                             rect->right, rect->bottom );
}

static inline const char *wine_dbgstr_longlong( unsigned long long ll )
{
    if (sizeof(ll) > sizeof(unsigned long) && ll >> 32)
        return wine_dbg_sprintf( "%lx%08lx", (unsigned long)(ll >> 32), (unsigned long)ll );
    else return wine_dbg_sprintf( "%lx", (unsigned long)ll );
}

/* Wine uses shorter names that are very likely to conflict with other software */

static inline const char *debugstr_an( const char * s, int n ) { return wine_dbgstr_an( s, n ); }
static inline const char *debugstr_wn( const WCHAR *s, int n ) { return wine_dbgstr_wn( s, n ); }
static inline const char *debugstr_guid( const GUID *id )  { return wine_dbgstr_guid( id ); }
static inline const char *debugstr_a( const char *s )  { return wine_dbgstr_an( s, -1 ); }
static inline const char *debugstr_w( const WCHAR *s ) { return wine_dbgstr_wn( s, -1 ); }

#undef ERR  /* Solaris got an 'ERR' define in <sys/reg.h> */
#define TRACE(fmt, ...)     (void)0 // WINE_DPRINTF(TRACE, __func__, fmt, ## __VA_ARGS__)
#define TRACE_ON(channel)   0
#define WARN(fmt, ...)      (void)0 // WINE_DPRINTF(WARN, __func__, fmt, ## __VA_ARGS__)
#define WARN_ON(channel)    0
#define FIXME(fmt, ...)     (void)0 // WINE_DPRINTF(FIXME, __func__, fmt, ## __VA_ARGS__)
#define FIXME_ON(channel)   0
#define ERR(fmt, ...)       (void)0 // WINE_DPRINTF(ERR, __func__, fmt, ## __VA_ARGS__)
#define ERR_ON(channel)     0


#ifdef __cplusplus
}
#endif

#endif  /* __WINE_WINE_DEBUG_H */
