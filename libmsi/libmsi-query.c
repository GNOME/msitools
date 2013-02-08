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

#include "libmsi-query.h"

#include "debug.h"
#include "libmsi.h"
#include "msipriv.h"
#include "query.h"

enum
{
    PROP_0,

    PROP_DATABASE,
    PROP_QUERY,
};

G_DEFINE_TYPE (LibmsiQuery, libmsi_query, G_TYPE_OBJECT);

static void
libmsi_query_init (LibmsiQuery *self)
{
    list_init (&self->mem);
}

static void
libmsi_query_finalize (GObject *object)
{
    LibmsiQuery *self = LIBMSI_QUERY (object);
    struct list *ptr, *t;

    if (self->view && self->view->ops->delete)
        self->view->ops->delete (self->view);

    if (self->database)
        g_object_unref (self->database);

    LIST_FOR_EACH_SAFE (ptr, t, &self->mem) {
        msi_free (ptr);
    }

    g_free (self->query);

    G_OBJECT_CLASS (libmsi_query_parent_class)->finalize (object);
}

static void
libmsi_query_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (LIBMSI_IS_QUERY (object));
    LibmsiQuery *self = LIBMSI_QUERY (object);

    switch (prop_id) {
    case PROP_DATABASE:
        g_return_if_fail (self->database == NULL);
        self->database = g_value_dup_object (value);
        break;
    case PROP_QUERY:
        g_return_if_fail (self->query == NULL);
        self->query = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
libmsi_query_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (LIBMSI_IS_QUERY (object));
    LibmsiQuery *self = LIBMSI_QUERY (object);

    switch (prop_id) {
    case PROP_DATABASE:
        g_value_set_object (value, self->database);
        break;
    case PROP_QUERY:
        g_value_set_string (value, self->query);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
libmsi_query_constructed (GObject *object)
{
    if (G_OBJECT_CLASS (libmsi_query_parent_class)->constructed)
        G_OBJECT_CLASS (libmsi_query_parent_class)->constructed (object);
}

static void
libmsi_query_class_init (LibmsiQueryClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = libmsi_query_finalize;
    object_class->set_property = libmsi_query_set_property;
    object_class->get_property = libmsi_query_get_property;
    object_class->constructed = libmsi_query_constructed;

    g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DATABASE,
        g_param_spec_object ("database", "database", "database", LIBMSI_TYPE_DATABASE,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_QUERY,
        g_param_spec_string ("query", "query", "query", NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));
}

/* TODO: use GInitable */
static gboolean
init (LibmsiQuery *self, GError **error)
{
    unsigned r;

    r = _libmsi_parse_sql (self->database, self->query, &self->view, &self->mem);

    if (r != LIBMSI_RESULT_SUCCESS)
        g_set_error_literal (error, LIBMSI_RESULT_ERROR, r, G_STRFUNC);

    return r == LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_view_find_column( LibmsiView *table, const char *name, const char *table_name, unsigned *n )
{
    const char *col_name;
    const char *haystack_table_name;
    unsigned i, count, r;

    r = table->ops->get_dimensions( table, NULL, &count );
    if( r != LIBMSI_RESULT_SUCCESS )
        return r;

    for( i=1; i<=count; i++ )
    {
        int x;

        r = table->ops->get_column_info( table, i, &col_name, NULL,
                                         NULL, &haystack_table_name );
        if( r != LIBMSI_RESULT_SUCCESS )
            return r;
        x = strcmp( name, col_name );
        if( table_name )
            x |= strcmp( table_name, haystack_table_name );
        if( !x )
        {
            *n = i;
            return LIBMSI_RESULT_SUCCESS;
        }
    }
    return LIBMSI_RESULT_INVALID_PARAMETER;
}

unsigned _libmsi_database_open_query(LibmsiDatabase *db,
              const char *szQuery, LibmsiQuery **pView)
{
    TRACE("%s %p\n", debugstr_a(szQuery), pView);

    *pView = libmsi_query_new (db, szQuery, NULL);

    return *pView ? LIBMSI_RESULT_SUCCESS : LIBMSI_RESULT_BAD_QUERY_SYNTAX;
}

unsigned _libmsi_query_open( LibmsiDatabase *db, LibmsiQuery **view, const char *fmt, ... )
{
    GError *err = NULL;
    unsigned r = LIBMSI_RESULT_SUCCESS;
    char *query;
    va_list va;

    va_start(va, fmt);
    query = g_strdup_vprintf(fmt, va);
    va_end(va);

    *view = libmsi_query_new (db, query, &err);
    if (err)
        r = err->code;
    g_clear_error (&err);

    g_free(query);
    return r;
}

unsigned _libmsi_query_iterate_records( LibmsiQuery *view, unsigned *count,
                         record_func func, void *param )
{
    LibmsiRecord *rec = NULL;
    unsigned r, n = 0, max = 0;
    GError *error = NULL; // FIXME: move error handling to caller

    r = _libmsi_query_execute( view, NULL );
    if( r != LIBMSI_RESULT_SUCCESS )
        return r;

    if( count )
        max = *count;

    /* iterate a query */
    for( n = 0; (max == 0) || (n < max); n++ )
    {
        r = _libmsi_query_fetch( view, &rec );
        if( r != LIBMSI_RESULT_SUCCESS )
            break;
        if (func)
            r = func( rec, param );
        g_object_unref(rec);
        if( r != LIBMSI_RESULT_SUCCESS )
            break;
    }

    libmsi_query_close( view, &error );
    if (error) {
        g_critical ("%s", error->message);
        g_clear_error (&error);
    }

    if( count )
        *count = n;

    if( r == NO_MORE_ITEMS )
        r = LIBMSI_RESULT_SUCCESS;

    return r;
}

/* return a single record from a query */
LibmsiRecord *_libmsi_query_get_record( LibmsiDatabase *db, const char *fmt, ... )
{
    LibmsiRecord *rec = NULL;
    LibmsiQuery *view = NULL;
    unsigned r = LIBMSI_RESULT_SUCCESS;
    va_list va;
    char *query;
    GError *error = NULL; // FIXME: move error to caller

    va_start(va, fmt);
    query = g_strdup_vprintf(fmt, va);
    va_end(va);

    view = libmsi_query_new (db, query, &error);
    if (error)
        r = error->code;
    g_clear_error (&error);
    g_free(query);

    if( r == LIBMSI_RESULT_SUCCESS )
    {
        _libmsi_query_execute( view, NULL );
        _libmsi_query_fetch( view, &rec );
        libmsi_query_close( view, &error );
        if (error) {
            g_critical ("%s", error->message);
            g_clear_error (&error);
        }
        g_object_unref(view);
    }
    return rec;
}

unsigned msi_view_get_row(LibmsiDatabase *db, LibmsiView *view, unsigned row, LibmsiRecord **rec)
{
    unsigned row_count = 0, col_count = 0, i, ival, ret, type;

    TRACE("%p %p %d %p\n", db, view, row, rec);

    ret = view->ops->get_dimensions(view, &row_count, &col_count);
    if (ret)
        return ret;

    if (!col_count)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if (row >= row_count)
        return NO_MORE_ITEMS;

    *rec = libmsi_record_new (col_count);
    if (!*rec)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    for (i = 1; i <= col_count; i++)
    {
        ret = view->ops->get_column_info(view, i, NULL, &type, NULL, NULL);
        if (ret)
        {
            g_critical("Error getting column type for %d\n", i);
            continue;
        }

        if (MSITYPE_IS_BINARY(type))
        {
            GsfInput *stm = NULL;

            ret = view->ops->fetch_stream(view, row, i, &stm);
            if ((ret == LIBMSI_RESULT_SUCCESS) && stm)
            {
                _libmsi_record_set_gsf_input(*rec, i, stm);
                g_object_unref(G_OBJECT(stm));
            }
            else
                g_warning("failed to get stream\n");

            continue;
        }

        ret = view->ops->fetch_int(view, row, i, &ival);
        if (ret)
        {
            g_critical("Error fetching data for %d\n", i);
            continue;
        }

        if (! (type & MSITYPE_VALID))
            g_critical("Invalid type!\n");

        /* check if it's nul (0) - if so, don't set anything */
        if (!ival)
            continue;

        if (type & MSITYPE_STRING)
        {
            const char *sval;

            sval = msi_string_lookup_id(db->strings, ival);
            libmsi_record_set_string(*rec, i, sval);
        }
        else
        {
            if ((type & MSI_DATASIZEMASK) == 2)
                libmsi_record_set_int(*rec, i, ival - (1<<15));
            else
                libmsi_record_set_int(*rec, i, ival - (1<<31));
        }
    }

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiResult _libmsi_query_fetch(LibmsiQuery *query, LibmsiRecord **prec)
{
    LibmsiView *view;
    LibmsiResult r;

    TRACE("%p %p\n", query, prec );

    view = query->view;
    if( !view )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    r = msi_view_get_row(query->database, view, query->row, prec);
    if (r == LIBMSI_RESULT_SUCCESS)
        query->row ++;

    return r;
}

/**
 * libmsi_query_fetch:
 * @query: a #LibmsiQuery
 * @error: (allow-none): return location for the error
 *
 * Return the next query result. %NULL is returned when there
 * is no more results.
 *
 * Returns: (transfer full) (allow-none): a newly allocated
 *     #LibmsiRecord or %NULL when no results or failure.
 **/
LibmsiRecord *
libmsi_query_fetch (LibmsiQuery *query, GError **error)
{
    LibmsiRecord *record = NULL;
    unsigned ret;

    TRACE("%p\n", query);

    g_return_val_if_fail (LIBMSI_IS_QUERY (query), NULL);
    g_return_val_if_fail (!error || *error == NULL, NULL);

    g_object_ref(query);
    ret = _libmsi_query_fetch( query, &record );
    g_object_unref(query);

    /* FIXME: raise error when it happens */
    if (ret != LIBMSI_RESULT_SUCCESS &&
        ret != NO_MORE_ITEMS)
        g_set_error_literal (error, LIBMSI_RESULT_ERROR, ret, G_STRFUNC);

    return record;
}

/**
 * libmsi_query_close:
 * @query: a #LibmsiQuery
 * @error: (allow-none): return location for the error
 *
 * Release the current result set.
 *
 * Returns: %TRUE on success
 **/
gboolean
libmsi_query_close (LibmsiQuery *query, GError **error)
{
    LibmsiView *view;
    unsigned ret;

    TRACE("%p\n", query );

    g_return_val_if_fail (LIBMSI_IS_QUERY (query), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    g_object_ref(query);
    view = query->view;
    if( !view )
        return LIBMSI_RESULT_FUNCTION_FAILED;
    if( !view->ops->close )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    ret = view->ops->close( view );
    g_object_unref(query);

    /* FIXME: raise error when it happens */
    if (ret != LIBMSI_RESULT_SUCCESS)
        g_set_error_literal (error, LIBMSI_RESULT_ERROR, ret, G_STRFUNC);

    return ret == LIBMSI_RESULT_SUCCESS;
}

LibmsiResult _libmsi_query_execute(LibmsiQuery *query, LibmsiRecord *rec )
{
    LibmsiView *view;

    TRACE("%p %p\n", query, rec);

    view = query->view;
    if( !view )
        return LIBMSI_RESULT_FUNCTION_FAILED;
    if( !view->ops->execute )
        return LIBMSI_RESULT_FUNCTION_FAILED;
    query->row = 0;

    return view->ops->execute( view, rec );
}

/**
 * libmsi_query_execute:
 * @query: a #LibmsiQuery
 * @rec: (allow-none): a #LibmsiRecord containing query arguments, or
 *     %NULL if no arguments needed
 * @error: (allow-none): return location for the error
 *
 * Execute the @query with the arguments from @rec.
 *
 * Returns: %TRUE on success
 **/
gboolean
libmsi_query_execute (LibmsiQuery *query, LibmsiRecord *rec, GError **error)
{
    LibmsiResult ret;

    TRACE("%p %p\n", query, rec);

    g_return_val_if_fail (LIBMSI_IS_QUERY (query), FALSE);
    g_return_val_if_fail (!rec || LIBMSI_IS_RECORD (rec), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    if( !query )
        return LIBMSI_RESULT_INVALID_HANDLE;
    g_object_ref(query);

    if( rec )
        g_object_ref(rec);

    ret = _libmsi_query_execute( query, rec );

    g_object_unref(query);
    if( rec )
        g_object_unref(rec);

    /* FIXME: raise error when it happens */
    if (ret != LIBMSI_RESULT_SUCCESS)
        g_set_error_literal (error, LIBMSI_RESULT_ERROR, ret, G_STRFUNC);

    return ret == LIBMSI_RESULT_SUCCESS;
}

static void msi_set_record_type_string( LibmsiRecord *rec, unsigned field,
                                        unsigned type, bool temporary )
{
    static const char fmt[] = "%d";
    char szType[0x10];

    if (MSITYPE_IS_BINARY(type))
        szType[0] = 'v';
    else if (type & MSITYPE_LOCALIZABLE)
        szType[0] = 'l';
    else if (type & MSITYPE_UNKNOWN)
        szType[0] = 'f';
    else if (type & MSITYPE_STRING)
    {
        if (temporary)
            szType[0] = 'g';
        else
          szType[0] = 's';
    }
    else
    {
        if (temporary)
            szType[0] = 'j';
        else
            szType[0] = 'i';
    }

    if (type & MSITYPE_NULLABLE)
        szType[0] &= ~0x20;

    sprintf( &szType[1], fmt, (type&0xff) );

    TRACE("type %04x -> %s\n", type, debugstr_a(szType) );

    libmsi_record_set_string( rec, field, szType );
}

LibmsiResult _libmsi_query_get_column_info( LibmsiQuery *query, LibmsiColInfo info, LibmsiRecord **prec )
{
    unsigned r = LIBMSI_RESULT_FUNCTION_FAILED, i, count = 0, type;
    LibmsiRecord *rec;
    LibmsiView *view = query->view;
    const char *name;
    bool temporary;

    if( !view )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    if( !view->ops->get_dimensions )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    r = view->ops->get_dimensions( view, NULL, &count );
    if( r != LIBMSI_RESULT_SUCCESS )
        return r;
    if( !count )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    rec = libmsi_record_new (count);
    if( !rec )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    for( i=0; i<count; i++ )
    {
        name = NULL;
        r = view->ops->get_column_info( view, i+1, &name, &type, &temporary, NULL );
        if( r != LIBMSI_RESULT_SUCCESS )
            continue;
        if (info == LIBMSI_COL_INFO_NAMES)
            libmsi_record_set_string( rec, i+1, name );
        else
            msi_set_record_type_string( rec, i+1, type, temporary );
    }
    *prec = rec;
    return LIBMSI_RESULT_SUCCESS;
}

/**
 * libmsi_query_get_column_info:
 * @query: a #LibmsiQuery
 * @info: a #LibmsiColInfo specifying the type of information to return
 * @error: (allow-none): return location for the error
 *
 * Get column informations, returned as record string fields.
 *
 * Returns: (transfer full): a newly allocated #LibmsiRecord
 * containing informations or %NULL on error.
 **/
LibmsiRecord *
libmsi_query_get_column_info (LibmsiQuery *query, LibmsiColInfo info, GError **error)
{
    LibmsiRecord *rec;
    unsigned r;

    TRACE("%p %d\n", query, info);

    g_return_val_if_fail (LIBMSI_IS_QUERY (query), NULL);
    g_return_val_if_fail (info == LIBMSI_COL_INFO_NAMES ||
                          info == LIBMSI_COL_INFO_TYPES, NULL);
    g_return_val_if_fail (!error || *error == NULL, NULL);

    g_object_ref(query);
    r = _libmsi_query_get_column_info( query, info, &rec);
    g_object_unref(query);

    if (r != LIBMSI_RESULT_SUCCESS)
        g_set_error_literal (error, LIBMSI_RESULT_ERROR, r, G_STRFUNC);

    return rec;
}

/**
 * libmsi_query_get_error:
 * @query: a #LibmsiQuery
 * @column: (out) (allow-none): location to store the allocated column name
 * @error: (allow-none): return location for the error
 *
 * Call this to get more information on the last query error.
 **/
void
libmsi_query_get_error (LibmsiQuery *query, gchar **column, GError **error)
{
    LibmsiView *v;

    g_return_if_fail (LIBMSI_IS_QUERY (query));
    g_return_if_fail (!column || *column == NULL);
    g_return_if_fail (!error || *error == NULL);

    v = query->view;
    if (v->error != LIBMSI_DB_ERROR_SUCCESS) {
        /* FIXME: view could have a GError with message? */
        g_set_error (error, LIBMSI_DB_ERROR, v->error, G_STRFUNC);
        if (column)
            *column = g_strdup (v->error_column);
    }
}

/**
 * libmsi_query_new:
 * @database: a #LibmsiDatabase
 * @query: a SQL query
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Create a SQL query for @database.
 *
 * Returns: a new %LibmsiQuery on success, %NULL on failure
 **/
LibmsiQuery *
libmsi_query_new (LibmsiDatabase *database, const char *query, GError **error)
{
    LibmsiQuery *self;

    g_return_val_if_fail (LIBMSI_IS_DATABASE (database), NULL);
    g_return_val_if_fail (query != NULL, NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    self = g_object_new (LIBMSI_TYPE_QUERY,
                         "database", database,
                         "query", query,
                         NULL);

    if (!init (self, error)) {
        g_object_unref (self);
        return NULL;
    }

    return self;
}
