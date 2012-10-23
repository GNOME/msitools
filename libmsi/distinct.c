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


typedef struct tagDISTINCTSET
{
    unsigned val;
    unsigned count;
    unsigned row;
    struct tagDISTINCTSET *nextrow;
    struct tagDISTINCTSET *nextcol;
} DISTINCTSET;

typedef struct tagMSIDISTINCTVIEW
{
    MSIVIEW        view;
    MSIDATABASE   *db;
    MSIVIEW       *table;
    unsigned           row_count;
    unsigned          *translation;
} MSIDISTINCTVIEW;

static DISTINCTSET ** distinct_insert( DISTINCTSET **x, unsigned val, unsigned row )
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
    *x = msi_alloc( sizeof (DISTINCTSET) );
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

static void distinct_free( DISTINCTSET *x )
{
    while( x )
    {
        DISTINCTSET *next = x->nextrow;
        distinct_free( x->nextcol );
        msi_free( x );
        x = next;
    }
}

static unsigned DISTINCT_fetch_int( MSIVIEW *view, unsigned row, unsigned col, unsigned *val )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p %d %d %p\n", dv, row, col, val );

    if( !dv->table )
        return ERROR_FUNCTION_FAILED;

    if( row >= dv->row_count )
        return ERROR_INVALID_PARAMETER;

    row = dv->translation[ row ];

    return dv->table->ops->fetch_int( dv->table, row, col, val );
}

static unsigned DISTINCT_execute( MSIVIEW *view, MSIRECORD *record )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;
    unsigned r, i, j, r_count, c_count;
    DISTINCTSET *rowset = NULL;

    TRACE("%p %p\n", dv, record);

    if( !dv->table )
         return ERROR_FUNCTION_FAILED;

    r = dv->table->ops->execute( dv->table, record );
    if( r != ERROR_SUCCESS )
        return r;

    r = dv->table->ops->get_dimensions( dv->table, &r_count, &c_count );
    if( r != ERROR_SUCCESS )
        return r;

    dv->translation = msi_alloc( r_count*sizeof(unsigned) );
    if( !dv->translation )
        return ERROR_FUNCTION_FAILED;

    /* build it */
    for( i=0; i<r_count; i++ )
    {
        DISTINCTSET **x = &rowset;

        for( j=1; j<=c_count; j++ )
        {
            unsigned val = 0;
            r = dv->table->ops->fetch_int( dv->table, i, j, &val );
            if( r != ERROR_SUCCESS )
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
                return ERROR_FUNCTION_FAILED;
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

    return ERROR_SUCCESS;
}

static unsigned DISTINCT_close( MSIVIEW *view )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p\n", dv );

    if( !dv->table )
         return ERROR_FUNCTION_FAILED;

    msi_free( dv->translation );
    dv->translation = NULL;
    dv->row_count = 0;

    return dv->table->ops->close( dv->table );
}

static unsigned DISTINCT_get_dimensions( MSIVIEW *view, unsigned *rows, unsigned *cols )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p %p %p\n", dv, rows, cols );

    if( !dv->table )
        return ERROR_FUNCTION_FAILED;

    if( rows )
    {
        if( !dv->translation )
            return ERROR_FUNCTION_FAILED;
        *rows = dv->row_count;
    }

    return dv->table->ops->get_dimensions( dv->table, NULL, cols );
}

static unsigned DISTINCT_get_column_info( MSIVIEW *view, unsigned n, const WCHAR **name,
                                      unsigned *type, BOOL *temporary, const WCHAR **table_name )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p %d %p %p %p %p\n", dv, n, name, type, temporary, table_name );

    if( !dv->table )
         return ERROR_FUNCTION_FAILED;

    return dv->table->ops->get_column_info( dv->table, n, name,
                                            type, temporary, table_name );
}

static unsigned DISTINCT_modify( MSIVIEW *view, MSIMODIFY eModifyMode,
                             MSIRECORD *rec, unsigned row )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p %d %p\n", dv, eModifyMode, rec );

    if( !dv->table )
         return ERROR_FUNCTION_FAILED;

    return dv->table->ops->modify( dv->table, eModifyMode, rec, row );
}

static unsigned DISTINCT_delete( MSIVIEW *view )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;

    TRACE("%p\n", dv );

    if( dv->table )
        dv->table->ops->delete( dv->table );

    msi_free( dv->translation );
    msiobj_release( &dv->db->hdr );
    msi_free( dv );

    return ERROR_SUCCESS;
}

static unsigned DISTINCT_find_matching_rows( MSIVIEW *view, unsigned col,
    unsigned val, unsigned *row, MSIITERHANDLE *handle )
{
    MSIDISTINCTVIEW *dv = (MSIDISTINCTVIEW*)view;
    unsigned r;

    TRACE("%p, %d, %u, %p\n", view, col, val, *handle);

    if( !dv->table )
         return ERROR_FUNCTION_FAILED;

    r = dv->table->ops->find_matching_rows( dv->table, col, val, row, handle );

    if( *row > dv->row_count )
        return ERROR_NO_MORE_ITEMS;

    *row = dv->translation[ *row ];

    return r;
}

static const MSIVIEWOPS distinct_ops =
{
    DISTINCT_fetch_int,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    DISTINCT_execute,
    DISTINCT_close,
    DISTINCT_get_dimensions,
    DISTINCT_get_column_info,
    DISTINCT_modify,
    DISTINCT_delete,
    DISTINCT_find_matching_rows,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

unsigned DISTINCT_CreateView( MSIDATABASE *db, MSIVIEW **view, MSIVIEW *table )
{
    MSIDISTINCTVIEW *dv = NULL;
    unsigned count = 0, r;

    TRACE("%p\n", dv );

    r = table->ops->get_dimensions( table, NULL, &count );
    if( r != ERROR_SUCCESS )
    {
        ERR("can't get table dimensions\n");
        return r;
    }

    dv = msi_alloc_zero( sizeof *dv );
    if( !dv )
        return ERROR_FUNCTION_FAILED;
    
    /* fill the structure */
    dv->view.ops = &distinct_ops;
    msiobj_addref( &db->hdr );
    dv->db = db;
    dv->table = table;
    dv->translation = NULL;
    dv->row_count = 0;
    *view = (MSIVIEW*) dv;

    return ERROR_SUCCESS;
}
