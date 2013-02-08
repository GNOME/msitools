/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002-2004 Mike McCormack for CodeWeavers
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

#include <stdarg.h>

#include "debug.h"
#include "libmsi.h"
#include "msipriv.h"

#include "query.h"



/* below is the query interface to a table */

typedef struct _LibmsiCreateView
{
    LibmsiView          view;
    LibmsiDatabase     *db;
    const char *         name;
    bool             bIsTemp;
    bool             hold;
    column_info     *col_info;
} LibmsiCreateView;

static unsigned create_view_fetch_int( LibmsiView *view, unsigned row, unsigned col, unsigned *val )
{
    LibmsiCreateView *cv = (LibmsiCreateView*)view;

    TRACE("%p %d %d %p\n", cv, row, col, val );

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned create_view_execute( LibmsiView *view, LibmsiRecord *record )
{
    LibmsiCreateView *cv = (LibmsiCreateView*)view;
    bool persist = (cv->bIsTemp) ? LIBMSI_CONDITION_FALSE : LIBMSI_CONDITION_TRUE;

    TRACE("%p Table %s (%s)\n", cv, debugstr_a(cv->name),
          cv->bIsTemp?"temporary":"permanent");

    if (cv->bIsTemp && !cv->hold)
        return LIBMSI_RESULT_SUCCESS;

    return msi_create_table( cv->db, cv->name, cv->col_info, persist );
}

static unsigned create_view_close( LibmsiView *view )
{
    LibmsiCreateView *cv = (LibmsiCreateView*)view;

    TRACE("%p\n", cv);

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned create_view_get_dimensions( LibmsiView *view, unsigned *rows, unsigned *cols )
{
    LibmsiCreateView *cv = (LibmsiCreateView*)view;

    TRACE("%p %p %p\n", cv, rows, cols );

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned create_view_get_column_info( LibmsiView *view, unsigned n, const char **name,
                                    unsigned *type, bool *temporary, const char **table_name )
{
    LibmsiCreateView *cv = (LibmsiCreateView*)view;

    TRACE("%p %d %p %p %p %p\n", cv, n, name, type, temporary, table_name );

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned create_view_delete( LibmsiView *view )
{
    LibmsiCreateView *cv = (LibmsiCreateView*)view;

    TRACE("%p\n", cv );

    g_object_unref(cv->db);
    msi_free( cv );

    return LIBMSI_RESULT_SUCCESS;
}

static const LibmsiViewOps create_ops =
{
    create_view_fetch_int,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    create_view_execute,
    create_view_close,
    create_view_get_dimensions,
    create_view_get_column_info,
    create_view_delete,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

G_GNUC_PURE
static unsigned check_columns( const column_info *col_info )
{
    const column_info *c1, *c2;

    /* check for two columns with the same name */
    for( c1 = col_info; c1; c1 = c1->next )
        for( c2 = c1->next; c2; c2 = c2->next )
            if (!strcmp( c1->column, c2->column ))
                return LIBMSI_RESULT_BAD_QUERY_SYNTAX;

    return LIBMSI_RESULT_SUCCESS;
}

unsigned create_view_create( LibmsiDatabase *db, LibmsiView **view, const char *table,
                        column_info *col_info, bool hold )
{
    LibmsiCreateView *cv = NULL;
    unsigned r;
    column_info *col;
    bool temp = true;
    bool tempprim = false;

    TRACE("%p\n", cv );

    r = check_columns( col_info );
    if( r != LIBMSI_RESULT_SUCCESS )
        return r;

    cv = msi_alloc_zero( sizeof *cv );
    if( !cv )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    for( col = col_info; col; col = col->next )
    {
        if (!col->table)
            col->table = table;

        if( !col->temporary )
            temp = false;
        else if ( col->type & MSITYPE_KEY )
            tempprim = true;
    }

    if ( !temp && tempprim )
    {
        msi_free( cv );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    /* fill the structure */
    cv->view.ops = &create_ops;
    cv->db = g_object_ref(db);
    cv->name = table;
    cv->col_info = col_info;
    cv->bIsTemp = temp;
    cv->hold = hold;
    *view = (LibmsiView*) cv;

    return LIBMSI_RESULT_SUCCESS;
}
