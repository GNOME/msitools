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

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winerror.h"

#include "libmsi-query.h"

#include "debug.h"
#include "unicode.h"
#include "libmsi.h"
#include "objbase.h"
#include "objidl.h"
#include "msipriv.h"
#include "winnls.h"

#include "query.h"

enum
{
    PROP_0,

    PROP_DATABASE,
    PROP_QUERY,
};

G_DEFINE_TYPE (LibmsiQuery, libmsi_query, G_TYPE_OBJECT);

static void
libmsi_query_init (LibmsiQuery *p)
{
    list_init (&p->mem);
}

static void
libmsi_query_finalize (GObject *object)
{
    LibmsiQuery *self = LIBMSI_QUERY (object);
    LibmsiQuery *p = self;
    struct list *ptr, *t;

    if (p->view && p->view->ops->delete)
        p->view->ops->delete (p->view);

    if (p->database)
        g_object_unref (p->database);

    LIST_FOR_EACH_SAFE (ptr, t, &p->mem) {
        msi_free (ptr);
    }

    g_free (p->query);

    G_OBJECT_CLASS (libmsi_query_parent_class)->finalize (object);
}

static void
libmsi_query_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    LibmsiQuery *p = LIBMSI_QUERY (object);
    g_return_if_fail (LIBMSI_IS_QUERY (object));

    switch (prop_id) {
    case PROP_DATABASE:
        g_return_if_fail (p->database == NULL);
        p->database = g_value_dup_object (value);
        break;
    case PROP_QUERY:
        g_return_if_fail (p->query == NULL);
        p->query = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
libmsi_query_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    LibmsiQuery *p = LIBMSI_QUERY (object);
    g_return_if_fail (LIBMSI_IS_QUERY (object));

    switch (prop_id) {
    case PROP_DATABASE:
        g_value_set_object (value, p->database);
        break;
    case PROP_QUERY:
        g_value_set_string (value, p->query);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
libmsi_query_constructed (GObject *object)
{
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
    LibmsiQuery *p = self;
    WCHAR *szwQuery;
    unsigned r;

    szwQuery = strdupAtoW(p->query);
    r = _libmsi_parse_sql (p->database, szwQuery, &p->view, &p->mem);
    msi_free(szwQuery);

    return r == LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_view_find_column( LibmsiView *table, const WCHAR *name, const WCHAR *table_name, unsigned *n )
{
    const WCHAR *col_name;
    const WCHAR *haystack_table_name;
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
        x = strcmpW( name, col_name );
        if( table_name )
            x |= strcmpW( table_name, haystack_table_name );
        if( !x )
        {
            *n = i;
            return LIBMSI_RESULT_SUCCESS;
        }
    }
    return LIBMSI_RESULT_INVALID_PARAMETER;
}

LibmsiResult libmsi_database_open_query(LibmsiDatabase *db,
              const char *szQuery, LibmsiQuery **pQuery)
{
    unsigned r;
    WCHAR *szwQuery;

    TRACE("%d %s %p\n", db, debugstr_a(szQuery), pQuery);

    if( szQuery )
    {
        szwQuery = strdupAtoW( szQuery );
        if( !szwQuery )
            return LIBMSI_RESULT_FUNCTION_FAILED;
    }
    else
        szwQuery = NULL;

    if( !db )
        return LIBMSI_RESULT_INVALID_HANDLE;

    g_object_ref(db);
    r = _libmsi_database_open_query( db, szwQuery, pQuery );
    g_object_unref(db);

    msi_free( szwQuery );
    return r;
}

unsigned _libmsi_database_open_query(LibmsiDatabase *db,
              const WCHAR *szwQuery, LibmsiQuery **pView)
{
    char *szQuery;
    TRACE("%s %p\n", debugstr_w(szQuery), pView);

    szQuery = strdupWtoA(szwQuery);
    *pView = libmsi_query_new (db, szQuery, NULL);
    msi_free(szQuery);

    return *pView ? LIBMSI_RESULT_SUCCESS : LIBMSI_RESULT_BAD_QUERY_SYNTAX;
}

unsigned _libmsi_query_open( LibmsiDatabase *db, LibmsiQuery **view, const WCHAR *fmt, ... )
{
    unsigned r;
    int size = 100, res;
    WCHAR *query;

    /* construct the string */
    for (;;)
    {
        va_list va;
        query = msi_alloc( size*sizeof(WCHAR) );
        va_start(va, fmt);
        res = vsnprintfW(query, size, fmt, va);
        va_end(va);
        if (res == -1) size *= 2;
        else if (res >= size) size = res + 1;
        else break;
        msi_free( query );
    }
    /* perform the query */
    r = _libmsi_database_open_query(db, query, view);
    msi_free(query);
    return r;
}

unsigned _libmsi_query_iterate_records( LibmsiQuery *view, unsigned *count,
                         record_func func, void *param )
{
    LibmsiRecord *rec = NULL;
    unsigned r, n = 0, max = 0;

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

    libmsi_query_close( view );

    if( count )
        *count = n;

    if( r == LIBMSI_RESULT_NO_MORE_ITEMS )
        r = LIBMSI_RESULT_SUCCESS;

    return r;
}

/* return a single record from a query */
LibmsiRecord *_libmsi_query_get_record( LibmsiDatabase *db, const WCHAR *fmt, ... )
{
    LibmsiRecord *rec = NULL;
    LibmsiQuery *view = NULL;
    unsigned r;
    int size = 100, res;
    WCHAR *query;

    /* construct the string */
    for (;;)
    {
        va_list va;
        query = msi_alloc( size*sizeof(WCHAR) );
        va_start(va, fmt);
        res = vsnprintfW(query, size, fmt, va);
        va_end(va);
        if (res == -1) size *= 2;
        else if (res >= size) size = res + 1;
        else break;
        msi_free( query );
    }
    /* perform the query */
    r = _libmsi_database_open_query(db, query, &view);
    msi_free(query);

    if( r == LIBMSI_RESULT_SUCCESS )
    {
        _libmsi_query_execute( view, NULL );
        _libmsi_query_fetch( view, &rec );
        libmsi_query_close( view );
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
        return LIBMSI_RESULT_NO_MORE_ITEMS;

    *rec = libmsi_record_new (col_count);
    if (!*rec)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    for (i = 1; i <= col_count; i++)
    {
        ret = view->ops->get_column_info(view, i, NULL, &type, NULL, NULL);
        if (ret)
        {
            ERR("Error getting column type for %d\n", i);
            continue;
        }

        if (MSITYPE_IS_BINARY(type))
        {
            IStream *stm = NULL;

            ret = view->ops->fetch_stream(view, row, i, &stm);
            if ((ret == LIBMSI_RESULT_SUCCESS) && stm)
            {
                _libmsi_record_set_IStream(*rec, i, stm);
                IStream_Release(stm);
            }
            else
                WARN("failed to get stream\n");

            continue;
        }

        ret = view->ops->fetch_int(view, row, i, &ival);
        if (ret)
        {
            ERR("Error fetching data for %d\n", i);
            continue;
        }

        if (! (type & MSITYPE_VALID))
            ERR("Invalid type!\n");

        /* check if it's nul (0) - if so, don't set anything */
        if (!ival)
            continue;

        if (type & MSITYPE_STRING)
        {
            const WCHAR *sval;

            sval = msi_string_lookup_id(db->strings, ival);
            _libmsi_record_set_stringW(*rec, i, sval);
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

LibmsiResult libmsi_query_fetch(LibmsiQuery *query, LibmsiRecord **record)
{
    unsigned ret;

    TRACE("%d %p\n", query, record);

    if( !record )
        return LIBMSI_RESULT_INVALID_PARAMETER;
    *record = 0;

    if( !query )
        return LIBMSI_RESULT_INVALID_HANDLE;
    g_object_ref(query);
    ret = _libmsi_query_fetch( query, record );
    g_object_unref(query);
    return ret;
}

LibmsiResult libmsi_query_close(LibmsiQuery *query)
{
    LibmsiView *view;
    unsigned ret;

    TRACE("%p\n", query );

    if( !query )
        return LIBMSI_RESULT_INVALID_HANDLE;

    g_object_ref(query);
    view = query->view;
    if( !view )
        return LIBMSI_RESULT_FUNCTION_FAILED;
    if( !view->ops->close )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    ret = view->ops->close( view );
    g_object_unref(query);
    return ret;
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

LibmsiResult libmsi_query_execute(LibmsiQuery *query, LibmsiRecord *rec)
{
    LibmsiResult ret;
    
    TRACE("%d %d\n", query, rec);

    if( !query )
        return LIBMSI_RESULT_INVALID_HANDLE;
    g_object_ref(query);

    if( rec )
        g_object_ref(rec);

    ret = _libmsi_query_execute( query, rec );

    g_object_unref(query);
    if( rec )
        g_object_unref(rec);

    return ret;
}

static unsigned msi_set_record_type_string( LibmsiRecord *rec, unsigned field,
                                        unsigned type, bool temporary )
{
    static const WCHAR fmt[] = { '%','d',0 };
    WCHAR szType[0x10];

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

    sprintfW( &szType[1], fmt, (type&0xff) );

    TRACE("type %04x -> %s\n", type, debugstr_w(szType) );

    return _libmsi_record_set_stringW( rec, field, szType );
}

unsigned _libmsi_query_get_column_info( LibmsiQuery *query, LibmsiColInfo info, LibmsiRecord **prec )
{
    unsigned r = LIBMSI_RESULT_FUNCTION_FAILED, i, count = 0, type;
    LibmsiRecord *rec;
    LibmsiView *view = query->view;
    const WCHAR *name;
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
            _libmsi_record_set_stringW( rec, i+1, name );
        else
            msi_set_record_type_string( rec, i+1, type, temporary );
    }
    *prec = rec;
    return LIBMSI_RESULT_SUCCESS;
}

LibmsiResult libmsi_query_get_column_info(LibmsiQuery *query, LibmsiColInfo info, LibmsiRecord **prec)
{
    unsigned r;

    TRACE("%d %d %p\n", query, info, prec);

    if( !prec )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( info != LIBMSI_COL_INFO_NAMES && info != LIBMSI_COL_INFO_TYPES )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( !query )
        return LIBMSI_RESULT_INVALID_HANDLE;

    g_object_ref(query);
    r = _libmsi_query_get_column_info( query, info, prec );
    g_object_unref(query);

    return r;
}

LibmsiDBError libmsi_query_get_error( LibmsiQuery *query, char *buffer, unsigned *buflen )
{
    const WCHAR *column;
    LibmsiDBError r;
    unsigned len;

    TRACE("%u %p %p\n", query, buffer, buflen);

    if (!buflen)
        return LIBMSI_DB_ERROR_INVALIDARG;

    if (!query)
        return LIBMSI_DB_ERROR_INVALIDARG;

    g_object_ref(query);
    if ((r = query->view->error)) column = query->view->error_column;
    else column = szEmpty;

    len = WideCharToMultiByte( CP_ACP, 0, column, -1, NULL, 0, NULL, NULL );
    if (buffer)
    {
        if (*buflen >= len)
            WideCharToMultiByte( CP_ACP, 0, column, -1, buffer, *buflen, NULL, NULL );
        else
            r = LIBMSI_DB_ERROR_MOREDATA;
    }

    *buflen = len;
    g_object_unref(query);
    return r;
}

LibmsiQuery*
libmsi_query_new (LibmsiDatabase *database, const char *query, GError **error)
{
    LibmsiQuery *self;

    g_return_val_if_fail (LIBMSI_IS_DATABASE (database), NULL);
    g_return_val_if_fail (query != NULL, NULL);

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
