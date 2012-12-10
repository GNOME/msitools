/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2008 James Hawkins
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


typedef struct _LibmsiDropView
{
    LibmsiView view;
    LibmsiDatabase *db;
    LibmsiView *table;
    column_info *colinfo;
    int hold;
} LibmsiDropView;

static unsigned drop_view_execute(LibmsiView *view, LibmsiRecord *record)
{
    LibmsiDropView *dv = (LibmsiDropView*)view;
    unsigned r;

    TRACE("%p %p\n", dv, record);

    if( !dv->table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    r = dv->table->ops->execute(dv->table, record);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    return dv->table->ops->drop(dv->table);
}

static unsigned drop_view_close(LibmsiView *view)
{
    LibmsiDropView *dv = (LibmsiDropView*)view;

    TRACE("%p\n", dv);

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned drop_view_get_dimensions(LibmsiView *view, unsigned *rows, unsigned *cols)
{
    LibmsiDropView *dv = (LibmsiDropView*)view;

    TRACE("%p %p %p\n", dv, rows, cols);

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned drop_view_delete( LibmsiView *view )
{
    LibmsiDropView *dv = (LibmsiDropView*)view;

    TRACE("%p\n", dv );

    if( dv->table )
        dv->table->ops->delete( dv->table );

    msi_free( dv );

    return LIBMSI_RESULT_SUCCESS;
}

static const LibmsiViewOps drop_ops =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    drop_view_execute,
    drop_view_close,
    drop_view_get_dimensions,
    NULL,
    drop_view_delete,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

unsigned drop_view_create(LibmsiDatabase *db, LibmsiView **view, const char *name)
{
    LibmsiDropView *dv;
    unsigned r;

    TRACE("%p %s\n", view, debugstr_a(name));

    dv = msi_alloc_zero(sizeof *dv );
    if(!dv)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    r = table_view_create(db, name, &dv->table);
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        msi_free( dv );
        return r;
    }

    dv->view.ops = &drop_ops;
    dv->db = db;

    *view = (LibmsiView *)dv;

    return LIBMSI_RESULT_SUCCESS;
}
