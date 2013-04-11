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

#include "debug.h"
#include "libmsi.h"
#include "msipriv.h"

#include "query.h"


typedef struct _LibmsiAlterView
{
    LibmsiView        view;
    LibmsiDatabase   *db;
    LibmsiView       *table;
    column_info   *colinfo;
    int hold;
} LibmsiAlterView;

static unsigned alter_view_fetch_int( LibmsiView *view, unsigned row, unsigned col, unsigned *val )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p %d %d %p\n", av, row, col, val );

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned alter_view_fetch_stream( LibmsiView *view, unsigned row, unsigned col, GsfInput **stm)
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p %d %d %p\n", av, row, col, stm );

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned alter_view_get_row( LibmsiView *view, unsigned row, LibmsiRecord **rec )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p %d %p\n", av, row, rec );

    return av->table->ops->get_row(av->table, row, rec);
}

static unsigned count_iter(LibmsiRecord *row, void *param)
{
    (*(unsigned *)param)++;
    return LIBMSI_RESULT_SUCCESS;
}

static bool check_column_exists(LibmsiDatabase *db, const char *table, const char *column)
{
    LibmsiQuery *view;
    LibmsiRecord *rec;
    unsigned r;

    static const char query[] =
	    "SELECT * FROM `_Columns` WHERE `Table`='%s' AND `Name`='%s'";

    r = _libmsi_query_open(db, &view, query, table, column);
    if (r != LIBMSI_RESULT_SUCCESS)
        return false;

    r = _libmsi_query_execute(view, NULL);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    r = _libmsi_query_fetch(view, &rec);
    if (r == LIBMSI_RESULT_SUCCESS)
        g_object_unref(rec);

done:
    g_object_unref(view);
    return (r == LIBMSI_RESULT_SUCCESS);
}

static unsigned alter_add_column(LibmsiAlterView *av)
{
    unsigned r, colnum = 1;
    LibmsiQuery *view;
    LibmsiView *columns;

    static const char szColumns[] = "_Columns";
    static const char query[] =
        "SELECT * FROM `_Columns` WHERE `Table`='%s' ORDER BY `Number`";

    r = table_view_create(av->db, szColumns, &columns);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    if (check_column_exists(av->db, av->colinfo->table, av->colinfo->column))
    {
        columns->ops->delete(columns);
        return LIBMSI_RESULT_BAD_QUERY_SYNTAX;
    }

    r = _libmsi_query_open(av->db, &view, query, av->colinfo->table);
    if (r == LIBMSI_RESULT_SUCCESS)
    {
        r = _libmsi_query_iterate_records(view, NULL, count_iter, &colnum);
        g_object_unref(view);
        if (r != LIBMSI_RESULT_SUCCESS)
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

static unsigned alter_view_execute( LibmsiView *view, LibmsiRecord *record )
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

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned alter_view_close( LibmsiView *view )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p\n", av );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned alter_view_get_dimensions( LibmsiView *view, unsigned *rows, unsigned *cols )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p %p %p\n", av, rows, cols );

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned alter_view_get_column_info( LibmsiView *view, unsigned n, const char **name,
                                   unsigned *type, bool *temporary, const char **table_name )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p %d %p %p %p %p\n", av, n, name, type, temporary, table_name );

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned alter_view_delete( LibmsiView *view )
{
    LibmsiAlterView *av = (LibmsiAlterView*)view;

    TRACE("%p\n", av );
    if (av->table)
        av->table->ops->delete( av->table );
    msi_free( av );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned alter_view_find_matching_rows( LibmsiView *view, unsigned col,
    unsigned val, unsigned *row, MSIITERHANDLE *handle )
{
    TRACE("%p, %d, %u, %p\n", view, col, val, *handle);

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static const LibmsiViewOps alter_ops =
{
    alter_view_fetch_int,
    alter_view_fetch_stream,
    alter_view_get_row,
    NULL,
    NULL,
    NULL,
    alter_view_execute,
    alter_view_close,
    alter_view_get_dimensions,
    alter_view_get_column_info,
    alter_view_delete,
    alter_view_find_matching_rows,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

unsigned alter_view_create( LibmsiDatabase *db, LibmsiView **view, const char *name, column_info *colinfo, int hold )
{
    LibmsiAlterView *av;
    unsigned r;

    TRACE("%p %p %s %d\n", view, colinfo, debugstr_a(name), hold );

    av = msi_alloc_zero( sizeof *av );
    if( !av )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    r = table_view_create( db, name, &av->table );
    if (r != LIBMSI_RESULT_SUCCESS)
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

    return LIBMSI_RESULT_SUCCESS;
}
