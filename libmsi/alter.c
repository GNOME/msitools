/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2006 Mike McCormack
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

#include "query.h"


typedef struct LibmsiAlterView
{
    LibmsiView        view;
    LibmsiDatabase   *db;
    LibmsiView       *table;
    column_info   *colinfo;
    int hold;
} LibmsiAlterView;

static unsigned ALTER_fetch_int( LibmsiView *view, unsigned row, unsigned col, unsigned *val )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p %d %d %p\n", av, row, col, val );

    return ERROR_FUNCTION_FAILED;
}

static unsigned ALTER_fetch_stream( LibmsiView *view, unsigned row, unsigned col, IStream **stm)
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p %d %d %p\n", av, row, col, stm );

    return ERROR_FUNCTION_FAILED;
}

static unsigned ALTER_get_row( LibmsiView *view, unsigned row, LibmsiRecord **rec )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p %d %p\n", av, row, rec );

    return av->table->ops->get_row(av->table, row, rec);
}

static unsigned ITERATE_columns(LibmsiRecord *row, void *param)
{
    (*(unsigned *)param)++;
    return ERROR_SUCCESS;
}

static bool check_column_exists(LibmsiDatabase *db, const WCHAR *table, const WCHAR *column)
{
    LibmsiQuery *view;
    LibmsiRecord *rec;
    unsigned r;

    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        '`','_','C','o','l','u','m','n','s','`',' ','W','H','E','R','E',' ',
        '`','T','a','b','l','e','`','=','\'','%','s','\'',' ','A','N','D',' ',
        '`','N','a','m','e','`','=','\'','%','s','\'',0
    };

    r = MSI_OpenQuery(db, &view, query, table, column);
    if (r != ERROR_SUCCESS)
        return false;

    r = MSI_QueryExecute(view, NULL);
    if (r != ERROR_SUCCESS)
        goto done;

    r = MSI_QueryFetch(view, &rec);
    if (r == ERROR_SUCCESS)
        msiobj_release(&rec->hdr);

done:
    msiobj_release(&view->hdr);
    return (r == ERROR_SUCCESS);
}

static unsigned alter_add_column(LibmsiAlterView *av)
{
    unsigned r, colnum = 1;
    LibmsiQuery *view;
    LibmsiView *columns;

    static const WCHAR szColumns[] = {'_','C','o','l','u','m','n','s',0};
    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        '`','_','C','o','l','u','m','n','s','`',' ','W','H','E','R','E',' ',
        '`','T','a','b','l','e','`','=','\'','%','s','\'',' ','O','R','D','E','R',' ',
        'B','Y',' ','`','N','u','m','b','e','r','`',0
    };

    r = TABLE_CreateView(av->db, szColumns, &columns);
    if (r != ERROR_SUCCESS)
        return r;

    if (check_column_exists(av->db, av->colinfo->table, av->colinfo->column))
    {
        columns->ops->delete(columns);
        return ERROR_BAD_QUERY_SYNTAX;
    }

    r = MSI_OpenQuery(av->db, &view, query, av->colinfo->table, av->colinfo->column);
    if (r == ERROR_SUCCESS)
    {
        r = MSI_IterateRecords(view, NULL, ITERATE_columns, &colnum);
        msiobj_release(&view->hdr);
        if (r != ERROR_SUCCESS)
        {
            columns->ops->delete(columns);
            return r;
        }
    }
    r = columns->ops->add_column(columns, av->colinfo->table,
                                 colnum, av->colinfo->column,
                                 av->colinfo->type, (av->hold == 1));

    columns->ops->delete(columns);
    return r;
}

static unsigned ALTER_execute( LibmsiView *view, LibmsiRecord *record )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;
    unsigned ref;

    TRACE("%p %p\n", av, record);

    if (av->hold == 1)
        av->table->ops->add_ref(av->table);
    else if (av->hold == -1)
    {
        ref = av->table->ops->release(av->table);
        if (ref == 0)
            av->table = NULL;
    }

    if (av->colinfo)
        return alter_add_column(av);

    return ERROR_SUCCESS;
}

static unsigned ALTER_close( LibmsiView *view )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p\n", av );

    return ERROR_SUCCESS;
}

static unsigned ALTER_get_dimensions( LibmsiView *view, unsigned *rows, unsigned *cols )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p %p %p\n", av, rows, cols );

    return ERROR_FUNCTION_FAILED;
}

static unsigned ALTER_get_column_info( LibmsiView *view, unsigned n, const WCHAR **name,
                                   unsigned *type, bool *temporary, const WCHAR **table_name )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p %d %p %p %p %p\n", av, n, name, type, temporary, table_name );

    return ERROR_FUNCTION_FAILED;
}

static unsigned ALTER_modify( LibmsiView *view, LibmsiModify eModifyMode,
                          LibmsiRecord *rec, unsigned row )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p %d %p\n", av, eModifyMode, rec );

    return ERROR_FUNCTION_FAILED;
}

static unsigned ALTER_delete( LibmsiView *view )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p\n", av );
    if (av->table)
        av->table->ops->delete( av->table );
    msi_free( av );

    return ERROR_SUCCESS;
}

static unsigned ALTER_find_matching_rows( LibmsiView *view, unsigned col,
    unsigned val, unsigned *row, MSIITERHANDLE *handle )
{
    TRACE("%p, %d, %u, %p\n", view, col, val, *handle);

    return ERROR_FUNCTION_FAILED;
}

static const LibmsiViewOPS alter_ops =
{
    ALTER_fetch_int,
    ALTER_fetch_stream,
    ALTER_get_row,
    NULL,
    NULL,
    NULL,
    ALTER_execute,
    ALTER_close,
    ALTER_get_dimensions,
    ALTER_get_column_info,
    ALTER_modify,
    ALTER_delete,
    ALTER_find_matching_rows,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

unsigned ALTER_CreateView( LibmsiDatabase *db, LibmsiView **view, const WCHAR *name, column_info *colinfo, int hold )
{
    LibmsiAlterView *av;
    unsigned r;

    TRACE("%p %p %s %d\n", view, colinfo, debugstr_w(name), hold );

    av = msi_alloc_zero( sizeof *av );
    if( !av )
        return ERROR_FUNCTION_FAILED;

    r = TABLE_CreateView( db, name, &av->table );
    if (r != ERROR_SUCCESS)
    {
        msi_free( av );
        return r;
    }

    if (colinfo)
        colinfo->table = name;

    /* fill the structure */
    av->view.ops = &alter_ops;
    av->db = db;
    av->hold = hold;
    av->colinfo = colinfo;

    *view = &av->view;

    return ERROR_SUCCESS;
}
