/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002-2005 Mike McCormack for CodeWeavers
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



/*
 * Code to delete rows from a table.
 *
 * We delete rows by blanking them out rather than trying to remove the row.
 * This appears to be what the native MSI does (or tries to do). For the query:
 *
 * delete from Property
 *
 * some non-zero entries are left in the table by native MSI.  I'm not sure if
 * that's a bug in the way I'm running the query, or a just a bug.
 */

typedef struct _LibmsiDeleteView
{
    LibmsiView        view;
    LibmsiDatabase   *db;
    LibmsiView       *table;
} LibmsiDeleteView;

static unsigned delete_view_fetch_int( LibmsiView *view, unsigned row, unsigned col, unsigned *val )
{
    LibmsiDeleteView *dv = (LibmsiDeleteView*)view;

    TRACE("%p %d %d %p\n", dv, row, col, val );

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned delete_view_fetch_stream( LibmsiView *view, unsigned row, unsigned col, GsfInput **stm)
{
    LibmsiDeleteView *dv = (LibmsiDeleteView*)view;

    TRACE("%p %d %d %p\n", dv, row, col, stm );

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned delete_view_execute( LibmsiView *view, LibmsiRecord *record )
{
    LibmsiDeleteView *dv = (LibmsiDeleteView*)view;
    unsigned r, i, rows = 0, cols = 0;

    TRACE("%p %p\n", dv, record);

    if( !dv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    r = dv->table->ops->execute( dv->table, record );
    if( r != LIBMSI_RESULT_SUCCESS )
        return r;

    r = dv->table->ops->get_dimensions( dv->table, &rows, &cols );
    if( r != LIBMSI_RESULT_SUCCESS )
        return r;

    TRACE("deleting %d rows\n", rows);

    /* blank out all the rows that match */
    for ( i=0; i<rows; i++ )
        dv->table->ops->delete_row( dv->table, i );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned delete_view_close( LibmsiView *view )
{
    LibmsiDeleteView *dv = (LibmsiDeleteView*)view;

    TRACE("%p\n", dv );

    if( !dv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    return dv->table->ops->close( dv->table );
}

static unsigned delete_view_get_dimensions( LibmsiView *view, unsigned *rows, unsigned *cols )
{
    LibmsiDeleteView *dv = (LibmsiDeleteView*)view;

    TRACE("%p %p %p\n", dv, rows, cols );

    if( !dv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    *rows = 0;

    return dv->table->ops->get_dimensions( dv->table, NULL, cols );
}

static unsigned delete_view_get_column_info( LibmsiView *view, unsigned n, const char **name,
                                    unsigned *type, bool *temporary, const char **table_name )
{
    LibmsiDeleteView *dv = (LibmsiDeleteView*)view;

    TRACE("%p %d %p %p %p %p\n", dv, n, name, type, temporary, table_name );

    if( !dv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    return dv->table->ops->get_column_info( dv->table, n, name,
                                            type, temporary, table_name);
}

static unsigned delete_view_delete( LibmsiView *view )
{
    LibmsiDeleteView *dv = (LibmsiDeleteView*)view;

    TRACE("%p\n", dv );

    if( dv->table )
        dv->table->ops->delete( dv->table );

    msi_free( dv );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned delete_view_find_matching_rows( LibmsiView *view, unsigned col,
    unsigned val, unsigned *row, MSIITERHANDLE *handle )
{
    TRACE("%p, %d, %u, %p\n", view, col, val, *handle);

    return LIBMSI_RESULT_FUNCTION_FAILED;
}


static const LibmsiViewOps delete_ops =
{
    delete_view_fetch_int,
    delete_view_fetch_stream,
    NULL,
    NULL,
    NULL,
    NULL,
    delete_view_execute,
    delete_view_close,
    delete_view_get_dimensions,
    delete_view_get_column_info,
    delete_view_delete,
    delete_view_find_matching_rows,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

unsigned delete_view_create( LibmsiDatabase *db, LibmsiView **view, LibmsiView *table )
{
    LibmsiDeleteView *dv = NULL;

    TRACE("%p\n", dv );

    dv = msi_alloc_zero( sizeof *dv );
    if( !dv )
        return LIBMSI_RESULT_FUNCTION_FAILED;
    
    /* fill the structure */
    dv->view.ops = &delete_ops;
    dv->db = db;
    dv->table = table;

    *view = &dv->view;

    return LIBMSI_RESULT_SUCCESS;
}
