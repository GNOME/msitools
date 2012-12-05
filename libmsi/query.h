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

#ifndef __LIBMSI_QUERY_H
#define __LIBMSI_QUERY_H

#include <stdarg.h>

#include "libmsi.h"
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
    const char *data;
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
        const char *column;
        const char *table;
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
        const char *sval;
        union ext_column column;
    } u;
};

unsigned _libmsi_parse_sql( LibmsiDatabase *db, const char *command, LibmsiView **phview,
                   struct list *mem );

unsigned table_view_create( LibmsiDatabase *db, const char *name, LibmsiView **view );

unsigned select_view_create( LibmsiDatabase *db, LibmsiView **view, LibmsiView *table,
                        const column_info *columns );

unsigned distinct_view_create( LibmsiDatabase *db, LibmsiView **view, LibmsiView *table );

unsigned order_view_create( LibmsiDatabase *db, LibmsiView **view, LibmsiView *table,
                       column_info *columns );

unsigned where_view_create( LibmsiDatabase *db, LibmsiView **view, char *tables,
                       struct expr *cond );

unsigned create_view_create( LibmsiDatabase *db, LibmsiView **view, const char *table,
                        column_info *col_info, bool hold );

unsigned insert_view_create( LibmsiDatabase *db, LibmsiView **view, const char *table,
                        column_info *columns, column_info *values, bool temp );

unsigned update_view_create( LibmsiDatabase *db, LibmsiView **view, char *table,
                        column_info *list, struct expr *expr );

unsigned delete_view_create( LibmsiDatabase *db, LibmsiView **view, LibmsiView *table );

unsigned alter_view_create( LibmsiDatabase *db, LibmsiView **view, const char *name, column_info *colinfo, int hold );

unsigned streams_view_create( LibmsiDatabase *db, LibmsiView **view );

unsigned storages_view_create( LibmsiDatabase *db, LibmsiView **view );

unsigned drop_view_create( LibmsiDatabase *db, LibmsiView **view, const char *name );

int sql_get_token(const char *z, int *tokenType, int *skip);

LibmsiRecord *msi_query_merge_record( unsigned fields, const column_info *vl, LibmsiRecord *rec );

unsigned msi_create_table( LibmsiDatabase *db, const char *name, column_info *col_info,
                       LibmsiCondition persistent );

#pragma GCC visibility pop

#endif /* __LIBMSI_QUERY_H */
