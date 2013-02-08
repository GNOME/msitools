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

typedef struct _LibmsiSelectView
{
    LibmsiView        view;
    LibmsiDatabase   *db;
    LibmsiView       *table;
    unsigned           num_cols;
    unsigned           max_cols;
    unsigned           cols[1];
} LibmsiSelectView;

static unsigned select_view_fetch_int( LibmsiView *view, unsigned row, unsigned col, unsigned *val )
{
    LibmsiSelectView *sv = (LibmsiSelectView*)view;

    TRACE("%p %d %d %p\n", sv, row, col, val );

    if( !sv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    if( !col || col > sv->num_cols )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    col = sv->cols[ col - 1 ];
    if( !col )
    {
        *val = 0;
        return LIBMSI_RESULT_SUCCESS;
    }
    return sv->table->ops->fetch_int( sv->table, row, col, val );
}

static unsigned select_view_fetch_stream( LibmsiView *view, unsigned row, unsigned col, GsfInput **stm)
{
    LibmsiSelectView *sv = (LibmsiSelectView*)view;

    TRACE("%p %d %d %p\n", sv, row, col, stm );

    if( !sv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    if( !col || col > sv->num_cols )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    col = sv->cols[ col - 1 ];
    if( !col )
    {
        *stm = NULL;
        return LIBMSI_RESULT_SUCCESS;
    }
    return sv->table->ops->fetch_stream( sv->table, row, col, stm );
}

static unsigned select_view_get_row( LibmsiView *view, unsigned row, LibmsiRecord **rec )
{
    LibmsiSelectView *sv = (LibmsiSelectView *)view;

    TRACE("%p %d %p\n", sv, row, rec );

    if( !sv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    return msi_view_get_row(sv->db, view, row, rec);
}

static unsigned select_view_set_row( LibmsiView *view, unsigned row, LibmsiRecord *rec, unsigned mask )
{
    LibmsiSelectView *sv = (LibmsiSelectView*)view;
    unsigned i, expanded_mask = 0, r = LIBMSI_RESULT_SUCCESS, col_count = 0;
    LibmsiRecord *expanded;

    TRACE("%p %d %p %08x\n", sv, row, rec, mask );

    if ( !sv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    /* test if any of the mask bits are invalid */
    if ( mask >= (1<<sv->num_cols) )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    /* find the number of columns in the table below */
    r = sv->table->ops->get_dimensions( sv->table, NULL, &col_count );
    if( r )
        return r;

    /* expand the record to the right size for the underlying table */
    expanded = libmsi_record_new( col_count );
    if ( !expanded )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    /* move the right fields across */
    for ( i=0; i<sv->num_cols; i++ )
    {
        r = _libmsi_record_copy_field( rec, i+1, expanded, sv->cols[ i ] );
        if (r != LIBMSI_RESULT_SUCCESS)
            break;
        expanded_mask |= (1<<(sv->cols[i]-1));
    }

    /* set the row in the underlying table */
    if (r == LIBMSI_RESULT_SUCCESS)
        r = sv->table->ops->set_row( sv->table, row, expanded, expanded_mask );

    g_object_unref(expanded);
    return r;
}

static unsigned select_view_insert_row( LibmsiView *view, LibmsiRecord *record, unsigned row, bool temporary )
{
    LibmsiSelectView *sv = (LibmsiSelectView*)view;
    unsigned i, table_cols, r;
    LibmsiRecord *outrec;

    TRACE("%p %p\n", sv, record );

    if ( !sv->table )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    /* rearrange the record to suit the table */
    r = sv->table->ops->get_dimensions( sv->table, NULL, &table_cols );
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    outrec = libmsi_record_new( table_cols + 1 );

    for (i=0; i<sv->num_cols; i++)
    {
        r = _libmsi_record_copy_field( record, i+1, outrec, sv->cols[i] );
        if (r != LIBMSI_RESULT_SUCCESS)
            goto fail;
    }

    r = sv->table->ops->insert_row( sv->table, outrec, row, temporary );

fail:
    g_object_unref(outrec);

    return r;
}

static unsigned select_view_execute( LibmsiView *view, LibmsiRecord *record )
{
    LibmsiSelectView *sv = (LibmsiSelectView*)view;

    TRACE("%p %p\n", sv, record);

    if( !sv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    return sv->table->ops->execute( sv->table, record );
}

static unsigned select_view_close( LibmsiView *view )
{
    LibmsiSelectView *sv = (LibmsiSelectView*)view;

    TRACE("%p\n", sv );

    if( !sv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    return sv->table->ops->close( sv->table );
}

static unsigned select_view_get_dimensions( LibmsiView *view, unsigned *rows, unsigned *cols )
{
    LibmsiSelectView *sv = (LibmsiSelectView*)view;

    TRACE("%p %p %p\n", sv, rows, cols );

    if( !sv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    if( cols )
        *cols = sv->num_cols;

    return sv->table->ops->get_dimensions( sv->table, rows, NULL );
}

static unsigned select_view_get_column_info( LibmsiView *view, unsigned n, const char **name,
                                    unsigned *type, bool *temporary, const char **table_name )
{
    LibmsiSelectView *sv = (LibmsiSelectView*)view;

    TRACE("%p %d %p %p %p %p\n", sv, n, name, type, temporary, table_name );

    if( !sv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    if( !n || n > sv->num_cols )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    n = sv->cols[ n - 1 ];
    if( !n )
    {
        if (name) *name = szEmpty;
        if (type) *type = MSITYPE_UNKNOWN | MSITYPE_VALID;
        if (temporary) *temporary = false;
        if (table_name) *table_name = szEmpty;
        return LIBMSI_RESULT_SUCCESS;
    }
    return sv->table->ops->get_column_info( sv->table, n, name,
                                            type, temporary, table_name );
}

static unsigned select_view_delete( LibmsiView *view )
{
    LibmsiSelectView *sv = (LibmsiSelectView*)view;

    TRACE("%p\n", sv );

    if( sv->table )
        sv->table->ops->delete( sv->table );
    sv->table = NULL;

    msi_free( sv );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned select_view_find_matching_rows( LibmsiView *view, unsigned col,
    unsigned val, unsigned *row, MSIITERHANDLE *handle )
{
    LibmsiSelectView *sv = (LibmsiSelectView*)view;

    TRACE("%p, %d, %u, %p\n", view, col, val, *handle);

    if( !sv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    if( (col==0) || (col>sv->num_cols) )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    col = sv->cols[ col - 1 ];

    return sv->table->ops->find_matching_rows( sv->table, col, val, row, handle );
}


static const LibmsiViewOps select_ops =
{
    select_view_fetch_int,
    select_view_fetch_stream,
    select_view_get_row,
    select_view_set_row,
    select_view_insert_row,
    NULL,
    select_view_execute,
    select_view_close,
    select_view_get_dimensions,
    select_view_get_column_info,
    select_view_delete,
    select_view_find_matching_rows,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static unsigned select_view_add_column( LibmsiSelectView *sv, const char *name,
                              const char *table_name )
{
    unsigned r, n;
    LibmsiView *table;

    TRACE("%p adding %s.%s\n", sv, debugstr_a( table_name ),
          debugstr_a( name ));

    if( sv->view.ops != &select_ops )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    table = sv->table;
    if( !table )
        return LIBMSI_RESULT_FUNCTION_FAILED;
    if( !table->ops->get_dimensions )
        return LIBMSI_RESULT_FUNCTION_FAILED;
    if( !table->ops->get_column_info )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    if( sv->num_cols >= sv->max_cols )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    if ( !name[0] ) n = 0;
    else
    {
        r = _libmsi_view_find_column( table, name, table_name, &n );
        if( r != LIBMSI_RESULT_SUCCESS )
            return r;
    }

    sv->cols[sv->num_cols] = n;
    TRACE("Translating column %s from %d -> %d\n", 
          debugstr_a( name ), sv->num_cols, n);

    sv->num_cols++;

    return LIBMSI_RESULT_SUCCESS;
}

G_GNUC_PURE
static int select_count_columns( const column_info *col )
{
    int n;
    for (n = 0; col; col = col->next)
        n++;
    return n;
}

unsigned select_view_create( LibmsiDatabase *db, LibmsiView **view, LibmsiView *table,
                        const column_info *columns )
{
    LibmsiSelectView *sv = NULL;
    unsigned count = 0, r = LIBMSI_RESULT_SUCCESS;

    TRACE("%p\n", sv );

    count = select_count_columns( columns );

    sv = msi_alloc_zero( sizeof *sv + count*sizeof (unsigned) );
    if( !sv )
        return LIBMSI_RESULT_FUNCTION_FAILED;
    
    /* fill the structure */
    sv->view.ops = &select_ops;
    sv->db = db;
    sv->table = table;
    sv->num_cols = 0;
    sv->max_cols = count;

    while( columns )
    {
        r = select_view_add_column( sv, columns->column, columns->table );
        if( r )
            break;
        columns = columns->next;
    }

    if( r == LIBMSI_RESULT_SUCCESS )
        *view = &sv->view;
    else
        msi_free( sv );

    return r;
}
