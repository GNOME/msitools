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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>

G_BEGIN_DECLS

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

const char *wine_dbg_sprintf( const char *format, ...) G_GNUC_PRINTF (1, 2);;

#define wine_dbg_printf(format,...) (printf(format, ## __VA_ARGS__), fflush(stdout))
#define WINE_DPRINTF(class, function, format, ...) \
     wine_dbg_printf(#class ":%s:" format, function, ## __VA_ARGS__)

static inline const char *wine_dbgstr_a( const char *s )
{
    return wine_dbgstr_an( s, -1 );
}

static inline const char *wine_dbgstr_guid( const uint8_t *id )
{
    return wine_dbg_sprintf( "{%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
             id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7],
             id[8], id[9], id[10], id[11], id[12], id[13], id[14], id[15]);
}

static inline const char *wine_dbgstr_longlong( unsigned long long ll )
{
    if (sizeof(ll) > sizeof(unsigned long) && ll >> 32)
        return wine_dbg_sprintf( "%lx%08lx", (unsigned long)(ll >> 32), (unsigned long)ll );
    else return wine_dbg_sprintf( "%lx", (unsigned long)ll );
}

/* Wine uses shorter names that are very likely to conflict with other software */

static inline const char *debugstr_an( const char * s, int n ) { return wine_dbgstr_an( s, n ); }
static inline const char *debugstr_guid( const uint8_t *id )  { return wine_dbgstr_guid( id ); }
static inline const char *debugstr_a( const char *s )  { return wine_dbgstr_an( s, -1 ); }

#ifndef TRACE_ON
#define TRACE_ON  0
#endif

#define TRACE(fmt, ...)     G_STMT_START{       \
  if (TRACE_ON) {                               \
     g_debug(G_STRLOC " " fmt, ## __VA_ARGS__); \
  }                                             \
}G_STMT_END

G_END_DECLS

#endif  /* __WINE_WINE_DEBUG_H */
