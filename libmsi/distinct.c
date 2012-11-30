/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002 Mike McCormack for CodeWeavers
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

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "debug.h"
#include "libmsi.h"
#include "objbase.h"
#include "objidl.h"
#include "msipriv.h"
#include "winnls.h"

#include "query.h"


typedef struct LibmsiDistinctSet
{
    unsigned val;
    unsigned count;
    unsigned row;
    struct LibmsiDistinctSet *nextrow;
    struct LibmsiDistinctSet *nextcol;
} LibmsiDistinctSet;

typedef struct LibmsiDistinctView
{
    LibmsiView        view;
    LibmsiDatabase   *db;
    LibmsiView       *table;
    unsigned           row_count;
    unsigned          *translation;
} LibmsiDistinctView;

static LibmsiDistinctSet ** distinct_insert( LibmsiDistinctSet **x, unsigned val, unsigned row )
{
    /* horrible O(n) find */
    while( *x )
    {
        if( (*x)->val == val )
        {
            (*x)->count++;
            return x;
        }
        x = &(*x)->nextrow;
    }

    /* nothing found, so add one */
    *x = msi_alloc( sizeof (LibmsiDistinctSet) );
    if( *x )
    {
        (*x)->val = val;
        (*x)->count = 1;
        (*x)->row = row;
        (*x)->nextrow = NULL;
        (*x)->nextcol = NULL;
    }
    return x;
}

static void distinct_free( LibmsiDistinctSet *x )
{
    while( x )
    {
        LibmsiDistinctSet *next = x->nextrow;
        distinct_free( x->nextcol );
        msi_free( x );
        x = next;
    }
}

static unsigned distinct_view_fetch_int( LibmsiView *view, unsigned row, unsigned col, unsigned *val )
{
    LibmsiDistinctView *dv = (LibmsiDistinctView*)view;

    TRACE("%p %d %d %p\n", dv, row, col, val );

    if( !dv->table )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    if( row >= dv->row_count )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    row = dv->translation[ row ];

    return dv->table->ops->fetch_int( dv->table, row, col, val );
}

static unsigned distinct_view_execute( LibmsiView *view, LibmsiRecord *record )
{
    LibmsiDistinctView *dv = (LibmsiDistinctView*)view;
    unsigned r, i, j, r_count, c_count;
    LibmsiDistinctSet *rowset = NULL;

    TRACE("%p %p\n", dv, record);

    if( !dv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    r = dv->table->ops->execute( dv->table, record );
    if( r != LIBMSI_RESULT_SUCCESS )
        return r;

    r = dv->table->ops->get_dimensions( dv->table, &r_count, &c_count );
    if( r != LIBMSI_RESULT_SUCCESS )
        return r;

    dv->translation = msi_alloc( r_count*sizeof(unsigned) );
    if( !dv->translation )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    /* build it */
    for( i=0; i<r_count; i++ )
    {
        LibmsiDistinctSet **x = &rowset;

        for( j=1; j<=c_count; j++ )
        {
            unsigned val = 0;
            r = dv->table->ops->fetch_int( dv->table, i, j, &val );
            if( r != LIBMSI_RESULT_SUCCESS )
            {
                ERR("Failed to fetch int at %d %d\n", i, j );
                distinct_free( rowset );
                return r;
            }
            x = distinct_insert( x, val, i );
            if( !*x )
            {
                ERR("Failed to insert at %d %d\n", i, j );
                distinct_free( rowset );
                return LIBMSI_RESULT_FUNCTION_FAILED;
            }
            if( j != c_count )
                x = &(*x)->nextcol;
        }

        /* check if it was distinct and if so, include it */
        if( (*x)->row == i )
        {
            TRACE("Row %d -> %d\n", dv->row_count, i);
            dv->translation[dv->row_count++] = i;
        }
    }

    distinct_free( rowset );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned distinct_view_close( LibmsiView *view )
{
    LibmsiDistinctView *dv = (LibmsiDistinctView*)view;

    TRACE("%p\n", dv );

    if( !dv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    msi_free( dv->translation );
    dv->translation = NULL;
    dv->row_count = 0;

    return dv->table->ops->close( dv->table );
}

static unsigned distinct_view_get_dimensions( LibmsiView *view, unsigned *rows, unsigned *cols )
{
    LibmsiDistinctView *dv = (LibmsiDistinctView*)view;

    TRACE("%p %p %p\n", dv, rows, cols );

    if( !dv->table )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    if( rows )
    {
        if( !dv->translation )
            return LIBMSI_RESULT_FUNCTION_FAILED;
        *rows = dv->row_count;
    }

    return dv->table->ops->get_dimensions( dv->table, NULL, cols );
}

static unsigned distinct_view_get_column_info( LibmsiView *view, unsigned n, const WCHAR **name,
                                      unsigned *type, bool *temporary, const WCHAR **table_name )
{
    LibmsiDistinctView *dv = (LibmsiDistinctView*)view;

    TRACE("%p %d %p %p %p %p\n", dv, n, name, type, temporary, table_name );

    if( !dv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    return dv->table->ops->get_column_info( dv->table, n, name,
                                            type, temporary, table_name );
}

static unsigned distinct_view_modify( LibmsiView *view, LibmsiModify eModifyMode,
                             LibmsiRecord *rec, unsigned row )
{
    LibmsiDistinctView *dv = (LibmsiDistinctView*)view;

    TRACE("%p %d %p\n", dv, eModifyMode, rec );

    if( !dv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    return dv->table->ops->modify( dv->table, eModifyMode, rec, row );
}

static unsigned distinct_view_delete( LibmsiView *view )
{
    LibmsiDistinctView *dv = (LibmsiDistinctView*)view;

    TRACE("%p\n", dv );

    if( dv->table )
        dv->table->ops->delete( dv->table );

    msi_free( dv->translation );
    msiobj_release( &dv->db->hdr );
    msi_free( dv );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned distinct_view_find_matching_rows( LibmsiView *view, unsigned col,
    unsigned val, unsigned *row, MSIITERHANDLE *handle )
{
    LibmsiDistinctView *dv = (LibmsiDistinctView*)view;
    unsigned r;

    TRACE("%p, %d, %u, %p\n", view, col, val, *handle);

    if( !dv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    r = dv->table->ops->find_matching_rows( dv->table, col, val, row, handle );

    if( *row > dv->row_count )
        return LIBMSI_RESULT_NO_MORE_ITEMS;

    *row = dv->translation[ *row ];

    return r;
}

static const LibmsiViewOps distinct_ops =
{
    distinct_view_fetch_int,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    distinct_view_execute,
    distinct_view_close,
    distinct_view_get_dimensions,
    distinct_view_get_column_info,
    distinct_view_modify,
    distinct_view_delete,
    distinct_view_find_matching_rows,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

unsigned distinct_view_create( LibmsiDatabase *db, LibmsiView **view, LibmsiView *table )
{
    LibmsiDistinctView *dv = NULL;
    unsigned count = 0, r;

    TRACE("%p\n", dv );

    r = table->ops->get_dimensions( table, NULL, &count );
    if( r != LIBMSI_RESULT_SUCCESS )
    {
        ERR("can't get table dimensions\n");
        return r;
    }

    dv = alloc_msiobject( sizeof *dv, NULL );
    if( !dv )
        return LIBMSI_RESULT_FUNCTION_FAILED;
    
    /* fill the structure */
    dv->view.ops = &distinct_ops;
    msiobj_addref( &db->hdr );
    dv->db = db;
    dv->table = table;
    dv->translation = NULL;
    dv->row_count = 0;
    *view = (LibmsiView*) dv;

    return LIBMSI_RESULT_SUCCESS;
}
