/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002 Mike McCormack for CodeWeavers
 * Copyright 2011 Bernhard Loos
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
#include <assert.h>

#include "debug.h"
#include "libmsi.h"
#include "msipriv.h"

#include "query.h"


/* below is the query interface to a table */
typedef struct _LibmsiRowEntry
{
    struct _LibmsiWhereView *wv; /* used during sorting */
    unsigned values[1];
} LibmsiRowEntry;

typedef struct tagJOINTABLE
{
    struct tagJOINTABLE *next;
    LibmsiView *view;
    unsigned col_count;
    unsigned row_count;
    unsigned table_index;
} JOINTABLE;

typedef struct _LibmsiOrderInfo
{
    unsigned col_count;
    unsigned error;
    union ext_column columns[1];
} LibmsiOrderInfo;

typedef struct _LibmsiWhereView
{
    LibmsiView        view;
    LibmsiDatabase   *db;
    JOINTABLE     *tables;
    unsigned           row_count;
    unsigned           col_count;
    unsigned           table_count;
    LibmsiRowEntry  **reorder;
    unsigned           reorder_size; /* number of entries available in reorder */
    struct expr   *cond;
    unsigned           rec_index;
    LibmsiOrderInfo  *order_info;
} LibmsiWhereView;

static unsigned where_view_evaluate( LibmsiWhereView *wv, const unsigned rows[],
                            struct expr *cond, int *val, LibmsiRecord *record );

#define INITIAL_REORDER_SIZE 16

#define INVALID_ROW_INDEX (-1)

static void free_reorder(LibmsiWhereView *wv)
{
    unsigned i;

    if (!wv->reorder)
        return;

    for (i = 0; i < wv->row_count; i++)
        msi_free(wv->reorder[i]);

    msi_free( wv->reorder );
    wv->reorder = NULL;
    wv->reorder_size = 0;
    wv->row_count = 0;
}

static unsigned init_reorder(LibmsiWhereView *wv)
{
    LibmsiRowEntry **new = msi_alloc_zero(sizeof(LibmsiRowEntry *) * INITIAL_REORDER_SIZE);
    if (!new)
        return LIBMSI_RESULT_OUTOFMEMORY;

    free_reorder(wv);

    wv->reorder = new;
    wv->reorder_size = INITIAL_REORDER_SIZE;

    return LIBMSI_RESULT_SUCCESS;
}

static inline unsigned find_row(LibmsiWhereView *wv, unsigned row, unsigned *(values[]))
{
    if (row >= wv->row_count)
        return NO_MORE_ITEMS;

    *values = wv->reorder[row]->values;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned add_row(LibmsiWhereView *wv, unsigned vals[])
{
    LibmsiRowEntry *new;

    if (wv->reorder_size <= wv->row_count)
    {
        LibmsiRowEntry **new_reorder;
        unsigned newsize = wv->reorder_size * 2;

        new_reorder = msi_realloc_zero(wv->reorder, sizeof(LibmsiRowEntry *) * wv->reorder_size,
                                       sizeof(LibmsiRowEntry *) * newsize);
        if (!new_reorder)
            return LIBMSI_RESULT_OUTOFMEMORY;

        wv->reorder = new_reorder;
        wv->reorder_size = newsize;
    }

    new = msi_alloc(offsetof( LibmsiRowEntry, values[wv->table_count] ));

    if (!new)
        return LIBMSI_RESULT_OUTOFMEMORY;

    wv->reorder[wv->row_count++] = new;

    memcpy(new->values, vals, wv->table_count * sizeof(unsigned));
    new->wv = wv;

    return LIBMSI_RESULT_SUCCESS;
}

static JOINTABLE *find_table(LibmsiWhereView *wv, unsigned col, unsigned *table_col)
{
    JOINTABLE *table = wv->tables;

    if(col == 0 || col > wv->col_count)
         return NULL;

    while (col > table->col_count)
    {
        col -= table->col_count;
        table = table->next;
        assert(table);
    }

    *table_col = col;
    return table;
}

static unsigned parse_column(LibmsiWhereView *wv, union ext_column *column,
                         unsigned *column_type)
{
    JOINTABLE *table = wv->tables;
    unsigned i, r;

    do
    {
        const char *table_name;

        if (column->unparsed.table)
        {
            r = table->view->ops->get_column_info(table->view, 1, NULL, NULL,
                                                  NULL, &table_name);
            if (r != LIBMSI_RESULT_SUCCESS)
                return r;
            if (strcmp(table_name, column->unparsed.table) != 0)
                continue;
        }

        for(i = 1; i <= table->col_count; i++)
        {
            const char *col_name;

            r = table->view->ops->get_column_info(table->view, i, &col_name, column_type,
                                                  NULL, NULL);
            if(r != LIBMSI_RESULT_SUCCESS )
                return r;

            if(strcmp(col_name, column->unparsed.column))
                continue;
            column->parsed.column = i;
            column->parsed.table = table;
            return LIBMSI_RESULT_SUCCESS;
        }
    }
    while ((table = table->next));

    g_warning("Couldn't find column %s.%s\n", debugstr_a( column->unparsed.table ), debugstr_a( column->unparsed.column ) );
    return LIBMSI_RESULT_BAD_QUERY_SYNTAX;
}

static unsigned where_view_fetch_int( LibmsiView *view, unsigned row, unsigned col, unsigned *val )
{
    LibmsiWhereView *wv = (LibmsiWhereView*)view;
    JOINTABLE *table;
    unsigned *rows;
    unsigned r;

    TRACE("%p %d %d %p\n", wv, row, col, val );

    if( !wv->tables )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    r = find_row(wv, row, &rows);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    table = find_table(wv, col, &col);
    if (!table)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    return table->view->ops->fetch_int(table->view, rows[table->table_index], col, val);
}

static unsigned where_view_fetch_stream( LibmsiView *view, unsigned row, unsigned col, GsfInput **stm )
{
    LibmsiWhereView *wv = (LibmsiWhereView*)view;
    JOINTABLE *table;
    unsigned *rows;
    unsigned r;

    TRACE("%p %d %d %p\n", wv, row, col, stm );

    if( !wv->tables )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    r = find_row(wv, row, &rows);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    table = find_table(wv, col, &col);
    if (!table)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    return table->view->ops->fetch_stream( table->view, rows[table->table_index], col, stm );
}

static unsigned where_view_get_row( LibmsiView *view, unsigned row, LibmsiRecord **rec )
{
    LibmsiWhereView *wv = (LibmsiWhereView *)view;

    TRACE("%p %d %p\n", wv, row, rec );

    if (!wv->tables)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    return msi_view_get_row( wv->db, view, row, rec );
}

static unsigned where_view_set_row( LibmsiView *view, unsigned row, LibmsiRecord *rec, unsigned mask )
{
    LibmsiWhereView *wv = (LibmsiWhereView*)view;
    unsigned i, r, offset = 0;
    JOINTABLE *table = wv->tables;
    unsigned *rows;
    unsigned mask_copy = mask;

    TRACE("%p %d %p %08x\n", wv, row, rec, mask );

    if( !wv->tables )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    r = find_row(wv, row, &rows);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    if (mask >= 1 << wv->col_count)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    do
    {
        for (i = 0; i < table->col_count; i++) {
            unsigned type;

            if (!(mask_copy & (1 << i)))
                continue;
            r = table->view->ops->get_column_info(table->view, i + 1, NULL,
                                            &type, NULL, NULL );
            if (r != LIBMSI_RESULT_SUCCESS)
                return r;
            if (type & MSITYPE_KEY)
                return LIBMSI_RESULT_FUNCTION_FAILED;
        }
        mask_copy >>= table->col_count;
    }
    while (mask_copy && (table = table->next));

    table = wv->tables;

    do
    {
        const unsigned col_count = table->col_count;
        LibmsiRecord *reduced;
        unsigned reduced_mask = (mask >> offset) & ((1 << col_count) - 1);

        if (!reduced_mask)
        {
            offset += col_count;
            continue;
        }

        reduced = libmsi_record_new(col_count);
        if (!reduced)
            return LIBMSI_RESULT_FUNCTION_FAILED;

        for (i = 1; i <= col_count; i++)
        {
            r = _libmsi_record_copy_field(rec, i + offset, reduced, i);
            if (r != LIBMSI_RESULT_SUCCESS)
                break;
        }

        offset += col_count;

        if (r == LIBMSI_RESULT_SUCCESS)
            r = table->view->ops->set_row(table->view, rows[table->table_index], reduced, reduced_mask);

        g_object_unref(reduced);
    }
    while ((table = table->next));
    return r;
}

static unsigned where_view_delete_row(LibmsiView *view, unsigned row)
{
    LibmsiWhereView *wv = (LibmsiWhereView *)view;
    unsigned r;
    unsigned *rows;

    TRACE("(%p %d)\n", view, row);

    if (!wv->tables)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    r = find_row(wv, row, &rows);
    if ( r != LIBMSI_RESULT_SUCCESS )
        return r;

    if (wv->table_count > 1)
        return LIBMSI_RESULT_CALL_NOT_IMPLEMENTED;

    return wv->tables->view->ops->delete_row(wv->tables->view, rows[0]);
}

static int expr_eval_binary( LibmsiWhereView *wv, const unsigned rows[],
                                const struct complex_expr *expr, int *val, LibmsiRecord *record )
{
    unsigned rl, rr;
    int lval, rval;

    rl = where_view_evaluate(wv, rows, expr->left, &lval, record);
    if (rl != LIBMSI_RESULT_SUCCESS && rl != LIBMSI_RESULT_CONTINUE)
        return rl;
    rr = where_view_evaluate(wv, rows, expr->right, &rval, record);
    if (rr != LIBMSI_RESULT_SUCCESS && rr != LIBMSI_RESULT_CONTINUE)
        return rr;

    if (rl == LIBMSI_RESULT_CONTINUE || rr == LIBMSI_RESULT_CONTINUE)
    {
        if (rl == rr)
        {
            *val = true;
            return LIBMSI_RESULT_CONTINUE;
        }

        if (expr->op == OP_AND)
        {
            if ((rl == LIBMSI_RESULT_CONTINUE && !rval) || (rr == LIBMSI_RESULT_CONTINUE && !lval))
            {
                *val = false;
                return LIBMSI_RESULT_SUCCESS;
            }
        }
        else if (expr->op == OP_OR)
        {
            if ((rl == LIBMSI_RESULT_CONTINUE && rval) || (rr == LIBMSI_RESULT_CONTINUE && lval))
            {
                *val = true;
                return LIBMSI_RESULT_SUCCESS;
            }
        }

        *val = true;
        return LIBMSI_RESULT_CONTINUE;
    }

    switch( expr->op )
    {
    case OP_EQ:
        *val = ( lval == rval );
        break;
    case OP_AND:
        *val = ( lval && rval );
        break;
    case OP_OR:
        *val = ( lval || rval );
        break;
    case OP_GT:
        *val = ( lval > rval );
        break;
    case OP_LT:
        *val = ( lval < rval );
        break;
    case OP_LE:
        *val = ( lval <= rval );
        break;
    case OP_GE:
        *val = ( lval >= rval );
        break;
    case OP_NE:
        *val = ( lval != rval );
        break;
    default:
        g_critical("Unknown operator %d\n", expr->op );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    return LIBMSI_RESULT_SUCCESS;
}

static inline unsigned expr_fetch_value(const union ext_column *expr, const unsigned rows[], unsigned *val)
{
    JOINTABLE *table = expr->parsed.table;

    if( rows[table->table_index] == INVALID_ROW_INDEX )
    {
        *val = 1;
        return LIBMSI_RESULT_CONTINUE;
    }
    return table->view->ops->fetch_int(table->view, rows[table->table_index],
                                        expr->parsed.column, val);
}


static unsigned expr_eval_unary( LibmsiWhereView *wv, const unsigned rows[],
                                const struct complex_expr *expr, int *val, LibmsiRecord *record )
{
    unsigned r;
    unsigned lval;

    r = expr_fetch_value(&expr->left->u.column, rows, &lval);
    if(r != LIBMSI_RESULT_SUCCESS)
        return r;

    switch( expr->op )
    {
    case OP_ISNULL:
        *val = !lval;
        break;
    case OP_NOTNULL:
        *val = lval;
        break;
    default:
        g_critical("Unknown operator %d\n", expr->op );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }
    return LIBMSI_RESULT_SUCCESS;
}

static unsigned expr_eval_string( LibmsiWhereView *wv, const unsigned rows[],
                                     const struct expr *expr,
                                     const LibmsiRecord *record,
                                     const char **str )
{
    unsigned val = 0, r = LIBMSI_RESULT_SUCCESS;

    switch( expr->type )
    {
    case EXPR_COL_NUMBER_STRING:
        r = expr_fetch_value(&expr->u.column, rows, &val);
        if (r == LIBMSI_RESULT_SUCCESS)
            *str =  msi_string_lookup_id(wv->db->strings, val);
        else
            *str = NULL;
        break;

    case EXPR_SVAL:
        *str = expr->u.sval;
        break;

    case EXPR_WILDCARD:
        *str = _libmsi_record_get_string_raw(record, ++wv->rec_index);
        break;

    default:
        g_critical("Invalid expression type\n");
        r = LIBMSI_RESULT_FUNCTION_FAILED;
        *str = NULL;
        break;
    }
    return r;
}

static unsigned expr_eval_strcmp( LibmsiWhereView *wv, const unsigned rows[], const struct complex_expr *expr,
                             int *val, const LibmsiRecord *record )
{
    int sr;
    const char *l_str, *r_str;
    unsigned r;

    *val = true;
    r = expr_eval_string(wv, rows, expr->left, record, &l_str);
    if (r == LIBMSI_RESULT_CONTINUE)
        return r;
    r = expr_eval_string(wv, rows, expr->right, record, &r_str);
    if (r == LIBMSI_RESULT_CONTINUE)
        return r;

    if( l_str == r_str ||
        ((!l_str || !*l_str) && (!r_str || !*r_str)) )
        sr = 0;
    else if( l_str && ! r_str )
        sr = 1;
    else if( r_str && ! l_str )
        sr = -1;
    else
        sr = strcmp( l_str, r_str );

    *val = ( expr->op == OP_EQ && ( sr == 0 ) ) ||
           ( expr->op == OP_NE && ( sr != 0 ) );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned where_view_evaluate( LibmsiWhereView *wv, const unsigned rows[],
                            struct expr *cond, int *val, LibmsiRecord *record )
{
    unsigned r, tval;

    if( !cond )
    {
        *val = true;
        return LIBMSI_RESULT_SUCCESS;
    }

    switch( cond->type )
    {
    case EXPR_COL_NUMBER:
        r = expr_fetch_value(&cond->u.column, rows, &tval);
        if( r != LIBMSI_RESULT_SUCCESS )
            return r;
        *val = tval - 0x8000;
        return LIBMSI_RESULT_SUCCESS;

    case EXPR_COL_NUMBER32:
        r = expr_fetch_value(&cond->u.column, rows, &tval);
        if( r != LIBMSI_RESULT_SUCCESS )
            return r;
        *val = tval - 0x80000000;
        return r;

    case EXPR_UVAL:
        *val = cond->u.uval;
        return LIBMSI_RESULT_SUCCESS;

    case EXPR_COMPLEX:
        return expr_eval_binary(wv, rows, &cond->u.expr, val, record);

    case EXPR_UNARY:
        return expr_eval_unary( wv, rows, &cond->u.expr, val, record );

    case EXPR_STRCMP:
        return expr_eval_strcmp( wv, rows, &cond->u.expr, val, record );

    case EXPR_WILDCARD:
        *val = libmsi_record_get_int( record, ++wv->rec_index );
        return LIBMSI_RESULT_SUCCESS;

    default:
        g_critical("Invalid expression type\n");
        break;
    }

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned check_condition( LibmsiWhereView *wv, LibmsiRecord *record, JOINTABLE **tables,
                             unsigned table_rows[] )
{
    unsigned r = LIBMSI_RESULT_FUNCTION_FAILED;
    int val;

    for (table_rows[(*tables)->table_index] = 0;
         table_rows[(*tables)->table_index] < (*tables)->row_count;
         table_rows[(*tables)->table_index]++)
    {
        val = 0;
        wv->rec_index = 0;
        r = where_view_evaluate( wv, table_rows, wv->cond, &val, record );
        if (r != LIBMSI_RESULT_SUCCESS && r != LIBMSI_RESULT_CONTINUE)
            break;
        if (val)
        {
            if (*(tables + 1))
            {
                r = check_condition(wv, record, tables + 1, table_rows);
                if (r != LIBMSI_RESULT_SUCCESS)
                    break;
            }
            else
            {
                if (r != LIBMSI_RESULT_SUCCESS)
                    break;
                add_row (wv, table_rows);
            }
        }
    }
    table_rows[(*tables)->table_index] = INVALID_ROW_INDEX;
    return r;
}

static int compare_entry( const void *left, const void *right )
{
    const LibmsiRowEntry *le = *(const LibmsiRowEntry**)left;
    const LibmsiRowEntry *re = *(const LibmsiRowEntry**)right;
    const LibmsiWhereView *wv = le->wv;
    LibmsiOrderInfo *order = wv->order_info;
    unsigned i, j, r, l_val, r_val;

    assert(le->wv == re->wv);

    if (order)
    {
        for (i = 0; i < order->col_count; i++)
        {
            const union ext_column *column = &order->columns[i];

            r = column->parsed.table->view->ops->fetch_int(column->parsed.table->view,
                          le->values[column->parsed.table->table_index],
                          column->parsed.column, &l_val);
            if (r != LIBMSI_RESULT_SUCCESS)
            {
                order->error = r;
                return 0;
            }

            r = column->parsed.table->view->ops->fetch_int(column->parsed.table->view,
                          re->values[column->parsed.table->table_index],
                          column->parsed.column, &r_val);
            if (r != LIBMSI_RESULT_SUCCESS)
            {
                order->error = r;
                return 0;
            }

            if (l_val != r_val)
                return l_val < r_val ? -1 : 1;
        }
    }

    for (j = 0; j < wv->table_count; j++)
    {
        if (le->values[j] != re->values[j])
            return le->values[j] < re->values[j] ? -1 : 1;
    }
    return 0;
}

static void add_to_array( JOINTABLE **array, JOINTABLE *elem )
{
    while (*array && *array != elem)
        array++;
    if (!*array)
        *array = elem;
}

G_GNUC_PURE
static bool in_array( JOINTABLE **array, JOINTABLE *elem )
{
    while (*array && *array != elem)
        array++;
    return *array != NULL;
}

#define CONST_EXPR 1 /* comparison to a constant value */
#define JOIN_TO_CONST_EXPR 0x10000 /* comparison to a table involved with
                                      a CONST_EXPR comaprison */

static unsigned reorder_check( const struct expr *expr, JOINTABLE **ordered_tables,
                           bool process_joins, JOINTABLE **lastused )
{
    unsigned res = 0;

    switch (expr->type)
    {
        case EXPR_WILDCARD:
        case EXPR_SVAL:
        case EXPR_UVAL:
            return 0;
        case EXPR_COL_NUMBER:
        case EXPR_COL_NUMBER32:
        case EXPR_COL_NUMBER_STRING:
            if (in_array(ordered_tables, expr->u.column.parsed.table))
                return JOIN_TO_CONST_EXPR;
            *lastused = expr->u.column.parsed.table;
            return CONST_EXPR;
        case EXPR_STRCMP:
        case EXPR_COMPLEX:
            res = reorder_check(expr->u.expr.right, ordered_tables, process_joins, lastused);
            /* fall through */
        case EXPR_UNARY:
            res += reorder_check(expr->u.expr.left, ordered_tables, process_joins, lastused);
            if (res == 0)
                return 0;
            if (res == CONST_EXPR)
                add_to_array(ordered_tables, *lastused);
            if (process_joins && res == JOIN_TO_CONST_EXPR + CONST_EXPR)
                add_to_array(ordered_tables, *lastused);
            return res;
        default:
            g_critical("Unknown expr type: %i\n", expr->type);
            assert(0);
            return 0x1000000;
    }
}

/* reorders the tablelist in a way to evaluate the condition as fast as possible */
static JOINTABLE **ordertables( LibmsiWhereView *wv )
{
    JOINTABLE *table;
    JOINTABLE **tables;

    tables = msi_alloc_zero( (wv->table_count + 1) * sizeof(*tables) );

    if (wv->cond)
    {
        table = NULL;
        reorder_check(wv->cond, tables, false, &table);
        table = NULL;
        reorder_check(wv->cond, tables, true, &table);
    }

    table = wv->tables;
    while (table)
    {
        add_to_array(tables, table);
        table = table->next;
    }
    return tables;
}

static unsigned where_view_execute( LibmsiView *view, LibmsiRecord *record )
{
    LibmsiWhereView *wv = (LibmsiWhereView*)view;
    unsigned r;
    JOINTABLE *table = wv->tables;
    unsigned *rows;
    JOINTABLE **ordered_tables;
    int i = 0;

    TRACE("%p %p\n", wv, record);

    if( !table )
         return LIBMSI_RESULT_FUNCTION_FAILED;

    r = init_reorder(wv);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    do
    {
        table->view->ops->execute(table->view, NULL);

        r = table->view->ops->get_dimensions(table->view, &table->row_count, NULL);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            g_critical("failed to get table dimensions\n");
            return r;
        }

        /* each table must have at least one row */
        if (table->row_count == 0)
            return LIBMSI_RESULT_SUCCESS;
    }
    while ((table = table->next));

    ordered_tables = ordertables( wv );

    rows = msi_alloc( wv->table_count * sizeof(*rows) );
    for (i = 0; i < wv->table_count; i++)
        rows[i] = INVALID_ROW_INDEX;

    r =  check_condition(wv, record, ordered_tables, rows);

    if (wv->order_info)
        wv->order_info->error = LIBMSI_RESULT_SUCCESS;

    qsort(wv->reorder, wv->row_count, sizeof(LibmsiRowEntry *), compare_entry);

    if (wv->order_info)
        r = wv->order_info->error;

    msi_free( rows );
    msi_free( ordered_tables );
    return r;
}

static unsigned where_view_close( LibmsiView *view )
{
    LibmsiWhereView *wv = (LibmsiWhereView*)view;
    JOINTABLE *table = wv->tables;

    TRACE("%p\n", wv );

    if (!table)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    do
        table->view->ops->close(table->view);
    while ((table = table->next));

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned where_view_get_dimensions( LibmsiView *view, unsigned *rows, unsigned *cols )
{
    LibmsiWhereView *wv = (LibmsiWhereView*)view;

    TRACE("%p %p %p\n", wv, rows, cols );

    if(!wv->tables)
         return LIBMSI_RESULT_FUNCTION_FAILED;

    if (rows)
    {
        if (!wv->reorder)
            return LIBMSI_RESULT_FUNCTION_FAILED;
        *rows = wv->row_count;
    }

    if (cols)
        *cols = wv->col_count;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned where_view_get_column_info( LibmsiView *view, unsigned n, const char **name,
                                   unsigned *type, bool *temporary, const char **table_name )
{
    LibmsiWhereView *wv = (LibmsiWhereView*)view;
    JOINTABLE *table;

    TRACE("%p %d %p %p %p %p\n", wv, n, name, type, temporary, table_name );

    if(!wv->tables)
         return LIBMSI_RESULT_FUNCTION_FAILED;

    table = find_table(wv, n, &n);
    if (!table)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    return table->view->ops->get_column_info(table->view, n, name,
                                            type, temporary, table_name);
}

static unsigned where_view_delete( LibmsiView *view )
{
    LibmsiWhereView *wv = (LibmsiWhereView*)view;
    JOINTABLE *table = wv->tables;

    TRACE("%p\n", wv );

    while(table)
    {
        JOINTABLE *next;

        table->view->ops->delete(table->view);
        table->view = NULL;
        next = table->next;
        msi_free(table);
        table = next;
    }
    wv->tables = NULL;
    wv->table_count = 0;

    free_reorder(wv);

    msi_free(wv->order_info);
    wv->order_info = NULL;

    g_object_unref(wv->db);
    msi_free( wv );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned where_view_find_matching_rows( LibmsiView *view, unsigned col,
    unsigned val, unsigned *row, MSIITERHANDLE *handle )
{
    LibmsiWhereView *wv = (LibmsiWhereView*)view;
    unsigned i, row_value;

    TRACE("%p, %d, %u, %p\n", view, col, val, *handle);

    if (!wv->tables)
         return LIBMSI_RESULT_FUNCTION_FAILED;

    if (col == 0 || col > wv->col_count)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    for (i = (uintptr_t)*handle; i < wv->row_count; i++)
    {
        if (view->ops->fetch_int( view, i, col, &row_value ) != LIBMSI_RESULT_SUCCESS)
            continue;

        if (row_value == val)
        {
            *row = i;
            *handle = (MSIITERHANDLE)(uintptr_t)(i + 1);
            return LIBMSI_RESULT_SUCCESS;
        }
    }

    return NO_MORE_ITEMS;
}

static unsigned where_view_sort(LibmsiView *view, column_info *columns)
{
    LibmsiWhereView *wv = (LibmsiWhereView *)view;
    JOINTABLE *table = wv->tables;
    column_info *column = columns;
    LibmsiOrderInfo *orderinfo;
    unsigned r, count = 0;
    int i;

    TRACE("%p %p\n", view, columns);

    if (!table)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    while (column)
    {
        count++;
        column = column->next;
    }

    if (count == 0)
        return LIBMSI_RESULT_SUCCESS;

    orderinfo = msi_alloc(sizeof(LibmsiOrderInfo) + (count - 1) * sizeof(union ext_column));
    if (!orderinfo)
        return LIBMSI_RESULT_OUTOFMEMORY;

    orderinfo->col_count = count;

    column = columns;

    for (i = 0; i < count; i++)
    {
        orderinfo->columns[i].unparsed.column = column->column;
        orderinfo->columns[i].unparsed.table = column->table;

        r = parse_column(wv, &orderinfo->columns[i], NULL);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto error;
    }

    wv->order_info = orderinfo;

    return LIBMSI_RESULT_SUCCESS;
error:
    msi_free(orderinfo);
    return r;
}

static const LibmsiViewOps where_ops =
{
    where_view_fetch_int,
    where_view_fetch_stream,
    where_view_get_row,
    where_view_set_row,
    NULL,
    where_view_delete_row,
    where_view_execute,
    where_view_close,
    where_view_get_dimensions,
    where_view_get_column_info,
    where_view_delete,
    where_view_find_matching_rows,
    NULL,
    NULL,
    NULL,
    NULL,
    where_view_sort,
    NULL,
};

static unsigned where_view_verify_condition( LibmsiWhereView *wv, struct expr *cond,
                                   unsigned *valid )
{
    unsigned r;

    switch( cond->type )
    {
    case EXPR_COLUMN:
    {
        unsigned type;

        *valid = false;

        r = parse_column(wv, &cond->u.column, &type);
        if (r != LIBMSI_RESULT_SUCCESS)
            break;

        if (type&MSITYPE_STRING)
            cond->type = EXPR_COL_NUMBER_STRING;
        else if ((type&0xff) == 4)
            cond->type = EXPR_COL_NUMBER32;
        else
            cond->type = EXPR_COL_NUMBER;

        *valid = true;
        break;
    }
    case EXPR_COMPLEX:
        r = where_view_verify_condition( wv, cond->u.expr.left, valid );
        if( r != LIBMSI_RESULT_SUCCESS )
            return r;
        if( !*valid )
            return LIBMSI_RESULT_SUCCESS;
        r = where_view_verify_condition( wv, cond->u.expr.right, valid );
        if( r != LIBMSI_RESULT_SUCCESS )
            return r;

        /* check the type of the comparison */
        if( ( cond->u.expr.left->type == EXPR_SVAL ) ||
            ( cond->u.expr.left->type == EXPR_COL_NUMBER_STRING ) ||
            ( cond->u.expr.right->type == EXPR_SVAL ) ||
            ( cond->u.expr.right->type == EXPR_COL_NUMBER_STRING ) )
        {
            switch( cond->u.expr.op )
            {
            case OP_EQ:
            case OP_NE:
                break;
            default:
                *valid = false;
                return LIBMSI_RESULT_INVALID_PARAMETER;
            }

            /* FIXME: check we're comparing a string to a column */

            cond->type = EXPR_STRCMP;
        }

        break;
    case EXPR_UNARY:
        if ( cond->u.expr.left->type != EXPR_COLUMN )
        {
            *valid = false;
            return LIBMSI_RESULT_INVALID_PARAMETER;
        }
        r = where_view_verify_condition( wv, cond->u.expr.left, valid );
        if( r != LIBMSI_RESULT_SUCCESS )
            return r;
        break;
    case EXPR_IVAL:
        *valid = 1;
        cond->type = EXPR_UVAL;
        cond->u.uval = cond->u.ival;
        break;
    case EXPR_WILDCARD:
        *valid = 1;
        break;
    case EXPR_SVAL:
        *valid = 1;
        break;
    default:
        g_critical("Invalid expression type\n");
        *valid = 0;
        break;
    }

    return LIBMSI_RESULT_SUCCESS;
}

unsigned where_view_create( LibmsiDatabase *db, LibmsiView **view, char *tables,
                       struct expr *cond )
{
    LibmsiWhereView *wv = NULL;
    unsigned r, valid = 0;
    char *ptr;

    TRACE("(%s)\n", debugstr_a(tables) );

    wv = msi_alloc_zero( sizeof *wv );
    if( !wv )
        return LIBMSI_RESULT_FUNCTION_FAILED;
    
    /* fill the structure */
    wv->view.ops = &where_ops;
    wv->db = g_object_ref(db);
    wv->cond = cond;

    while (*tables)
    {
        JOINTABLE *table;

        if ((ptr = strchr(tables, ' ')))
            *ptr = '\0';

        table = msi_alloc(sizeof(JOINTABLE));
        if (!table)
        {
            r = LIBMSI_RESULT_OUTOFMEMORY;
            goto end;
        }

        r = table_view_create(db, tables, &table->view);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            g_warning("can't create table: %s\n", debugstr_a(tables));
            msi_free(table);
            r = LIBMSI_RESULT_BAD_QUERY_SYNTAX;
            goto end;
        }

        r = table->view->ops->get_dimensions(table->view, NULL,
                                             &table->col_count);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            g_critical("can't get table dimensions\n");
            goto end;
        }

        wv->col_count += table->col_count;
        table->table_index = wv->table_count++;

        table->next = wv->tables;
        wv->tables = table;

        if (!ptr)
            break;

        tables = ptr + 1;
    }

    if( cond )
    {
        r = where_view_verify_condition( wv, cond, &valid );
        if( r != LIBMSI_RESULT_SUCCESS )
            goto end;
        if( !valid ) {
            r = LIBMSI_RESULT_FUNCTION_FAILED;
            goto end;
        }
    }

    *view = (LibmsiView*) wv;

    return LIBMSI_RESULT_SUCCESS;
end:
    where_view_delete(&wv->view);

    return r;
}
