/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2004 Mike McCormack for CodeWeavers
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

typedef struct _LibmsiUpdateView
{
    LibmsiView          view;
    LibmsiDatabase     *db;
    LibmsiView         *wv;
    column_info     *vals;
} LibmsiUpdateView;

static unsigned update_view_fetch_int( LibmsiView *view, unsigned row, unsigned col, unsigned *val )
{
    LibmsiUpdateView *uv = (LibmsiUpdateView*)view;

    TRACE("%p %d %d %p\n", uv, row, col, val );

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned update_view_execute( LibmsiView *view, LibmsiRecord *record )
{
    LibmsiUpdateView *uv = (LibmsiUpdateView*)view;
    unsigned i, r, col_count = 0, row_count = 0;
    LibmsiRecord *values = NULL;
    LibmsiRecord *where = NULL;
    LibmsiView *wv;
    unsigned cols_count, where_count;
    column_info *col = uv->vals;

    TRACE("%p %p\n", uv, record );

    /* extract the where markers from the record */
    if (record)
    {
        r = libmsi_record_get_field_count(record);

        for (i = 0; col; col = col->next)
            i++;

        cols_count = i;
        where_count = r - i;

        if (where_count > 0)
        {
            where = libmsi_record_new(where_count);

            if (where)
                for (i = 1; i <= where_count; i++)
                    _libmsi_record_copy_field(record, cols_count + i, where, i);
        }
    }

    wv = uv->wv;
    if( !wv )
    {
        r = LIBMSI_RESULT_FUNCTION_FAILED;
        goto done;
    }

    r = wv->ops->execute( wv, where );
    TRACE("tv execute returned %x\n", r);
    if( r )
        goto done;

    r = wv->ops->get_dimensions( wv, &row_count, &col_count );
    if( r )
        goto done;

    values = msi_query_merge_record( col_count, uv->vals, record );
    if (!values)
    {
        r = LIBMSI_RESULT_FUNCTION_FAILED;
        goto done;
    }

    for ( i=0; i<row_count; i++ )
    {
        r = wv->ops->set_row( wv, i, values, (1 << col_count) - 1 );
        if (r != LIBMSI_RESULT_SUCCESS)
            break;
    }

done:
    if ( where ) g_object_unref(where);
    if ( values ) g_object_unref(values);

    return r;
}


static unsigned update_view_close( LibmsiView *view )
{
    LibmsiUpdateView *uv = (LibmsiUpdateView*)view;
    LibmsiView *wv;

    TRACE("%p\n", uv);

    wv = uv->wv;
    if( !wv )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    return wv->ops->close( wv );
}

static unsigned update_view_get_dimensions( LibmsiView *view, unsigned *rows, unsigned *cols )
{
    LibmsiUpdateView *uv = (LibmsiUpdateView*)view;
    LibmsiView *wv;

    TRACE("%p %p %p\n", uv, rows, cols );

    wv = uv->wv;
    if( !wv )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    return wv->ops->get_dimensions( wv, rows, cols );
}

static unsigned update_view_get_column_info( LibmsiView *view, unsigned n, const char **name,
                                    unsigned *type, bool *temporary, const char **table_name )
{
    LibmsiUpdateView *uv = (LibmsiUpdateView*)view;
    LibmsiView *wv;

    TRACE("%p %d %p %p %p %p\n", uv, n, name, type, temporary, table_name );

    wv = uv->wv;
    if( !wv )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    return wv->ops->get_column_info( wv, n, name, type, temporary, table_name );
}

static unsigned update_view_delete( LibmsiView *view )
{
    LibmsiUpdateView *uv = (LibmsiUpdateView*)view;
    LibmsiView *wv;

    TRACE("%p\n", uv );

    wv = uv->wv;
    if( wv )
        wv->ops->delete( wv );
    g_object_unref(uv->db);
    msi_free( uv );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned update_view_find_matching_rows( LibmsiView *view, unsigned col, unsigned val, unsigned *row, MSIITERHANDLE *handle )
{
    TRACE("%p %d %d %p\n", view, col, val, *handle );

    return LIBMSI_RESULT_FUNCTION_FAILED;
}


static const LibmsiViewOps update_ops =
{
    update_view_fetch_int,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    update_view_execute,
    update_view_close,
    update_view_get_dimensions,
    update_view_get_column_info,
    update_view_delete,
    update_view_find_matching_rows,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

unsigned update_view_create( LibmsiDatabase *db, LibmsiView **view, char *table,
                        column_info *columns, struct expr *expr )
{
    LibmsiUpdateView *uv = NULL;
    unsigned r;
    LibmsiView *sv = NULL, *wv = NULL;

    TRACE("%p\n", uv );

    if (expr)
        r = where_view_create( db, &wv, table, expr );
    else
        r = table_view_create( db, table, &wv );

    if( r != LIBMSI_RESULT_SUCCESS )
        return r;

    /* then select the columns we want */
    r = select_view_create( db, &sv, wv, columns );
    if( r != LIBMSI_RESULT_SUCCESS )
    {
        wv->ops->delete( wv );
        return r;
    }

    uv = msi_alloc_zero( sizeof *uv );
    if( !uv )
    {
        wv->ops->delete( wv );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    /* fill the structure */
    uv->view.ops = &update_ops;
    uv->db = g_object_ref(db);
    uv->vals = columns;
    uv->wv = sv;
    *view = (LibmsiView*) uv;

    return LIBMSI_RESULT_SUCCESS;
}
