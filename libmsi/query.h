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

#ifndef __WINE_MSI_QUERY_H
#define __WINE_MSI_QUERY_H

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "objidl.h"
#include "msi.h"
#include "msiquery.h"
#include "msipriv.h"
#include "list.h"

#pragma GCC visibility push(hidden)

#define OP_EQ       1
#define OP_AND      2
#define OP_OR       3
#define OP_GT       4
#define OP_LT       5
#define OP_LE       6
#define OP_GE       7
#define OP_NE       8
#define OP_ISNULL   9
#define OP_NOTNULL  10

#define EXPR_COMPLEX  1
#define EXPR_COLUMN   2
#define EXPR_COL_NUMBER 3
#define EXPR_IVAL     4
#define EXPR_SVAL     5
#define EXPR_UVAL     6
#define EXPR_STRCMP   7
#define EXPR_WILDCARD 9
#define EXPR_COL_NUMBER_STRING 10
#define EXPR_COL_NUMBER32 11
#define EXPR_UNARY    12

struct sql_str {
    const WCHAR *data;
    int len;
};

struct complex_expr
{
    unsigned op;
    struct expr *left;
    struct expr *right;
};

struct tagJOINTABLE;
union ext_column
{
    struct
    {
        const WCHAR *column;
        const WCHAR *table;
    } unparsed;
    struct
    {
        unsigned column;
        struct tagJOINTABLE *table;
    } parsed;
};

struct expr
{
    int type;
    union
    {
        struct complex_expr expr;
        int   ival;
        unsigned  uval;
        const WCHAR *sval;
        union ext_column column;
    } u;
};

unsigned MSI_ParseSQL( LibmsiDatabase *db, const WCHAR *command, LibmsiView **phview,
                   struct list *mem );

unsigned TABLE_CreateView( LibmsiDatabase *db, const WCHAR *name, LibmsiView **view );

unsigned SELECT_CreateView( LibmsiDatabase *db, LibmsiView **view, LibmsiView *table,
                        const column_info *columns );

unsigned DISTINCT_CreateView( LibmsiDatabase *db, LibmsiView **view, LibmsiView *table );

unsigned ORDER_CreateView( LibmsiDatabase *db, LibmsiView **view, LibmsiView *table,
                       column_info *columns );

unsigned WHERE_CreateView( LibmsiDatabase *db, LibmsiView **view, WCHAR *tables,
                       struct expr *cond );

unsigned CREATE_CreateView( LibmsiDatabase *db, LibmsiView **view, const WCHAR *table,
                        column_info *col_info, bool hold );

unsigned INSERT_CreateView( LibmsiDatabase *db, LibmsiView **view, const WCHAR *table,
                        column_info *columns, column_info *values, bool temp );

unsigned UPDATE_CreateView( LibmsiDatabase *db, LibmsiView **view, WCHAR *table,
                        column_info *list, struct expr *expr );

unsigned DELETE_CreateView( LibmsiDatabase *db, LibmsiView **view, LibmsiView *table );

unsigned ALTER_CreateView( LibmsiDatabase *db, LibmsiView **view, const WCHAR *name, column_info *colinfo, int hold );

unsigned STREAMS_CreateView( LibmsiDatabase *db, LibmsiView **view );

unsigned STORAGES_CreateView( LibmsiDatabase *db, LibmsiView **view );

unsigned DROP_CreateView( LibmsiDatabase *db, LibmsiView **view, const WCHAR *name );

int sqliteGetToken(const WCHAR *z, int *tokenType, int *skip);

LibmsiRecord *msi_query_merge_record( unsigned fields, const column_info *vl, LibmsiRecord *rec );

unsigned msi_create_table( LibmsiDatabase *db, const WCHAR *name, column_info *col_info,
                       LibmsiCondition persistent );

#pragma GCC visibility pop

#endif /* __WINE_MSI_QUERY_H */
