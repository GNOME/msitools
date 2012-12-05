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
#include "debug.h"
#include "unicode.h"
#include "libmsi.h"
#include "objbase.h"
#include "objidl.h"
#include "msipriv.h"
#include "winnls.h"

#include "query.h"


static void _libmsi_query_destroy( LibmsiObject *arg )
{
    LibmsiQuery *query = (LibmsiQuery*) arg;
    struct list *ptr, *t;

    if( query->view && query->view->ops->delete )
        query->view->ops->delete( query->view );
    msiobj_release( &query->db->hdr );

    LIST_FOR_EACH_SAFE( ptr, t, &query->mem )
    {
        msi_free( ptr );
    }
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

LibmsiResult libmsi_database_open_query(LibmsiDatabase *db,
              const char *szQuery, LibmsiQuery **pQuery)
{
    unsigned r;

    TRACE("%d %s %p\n", db, debugstr_a(szQuery), pQuery);

    if( !db )
        return LIBMSI_RESULT_INVALID_HANDLE;
    msiobj_addref( &db->hdr);

    r = _libmsi_database_open_query( db, szQuery, pQuery );
    msiobj_release( &db->hdr );

    return r;
}

unsigned _libmsi_database_open_query(LibmsiDatabase *db,
              const char *szQuery, LibmsiQuery **pView)
{
    LibmsiQuery *query;
    unsigned r;

    TRACE("%s %p\n", debugstr_a(szQuery), pView);

    if( !szQuery)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    /* pre allocate a handle to hold a pointer to the view */
    query = alloc_msiobject( sizeof (LibmsiQuery), _libmsi_query_destroy );
    if( !query )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    msiobj_addref( &db->hdr );
    query->db = db;
    list_init( &query->mem );

    r = _libmsi_parse_sql( db, szQuery, &query->view, &query->mem );
    if( r == LIBMSI_RESULT_SUCCESS )
    {
        msiobj_addref( &query->hdr );
        *pView = query;
    }

    msiobj_release( &query->hdr );
    return r;
}

unsigned _libmsi_query_open( LibmsiDatabase *db, LibmsiQuery **view, const char *fmt, ... )
{
    unsigned r;
    int size = 100, res;
    char *query;

    /* construct the string */
    for (;;)
    {
        va_list va;
        query = msi_alloc( size*sizeof(char) );
        va_start(va, fmt);
        res = vsnprintf(query, size, fmt, va);
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
        msiobj_release( &rec->hdr );
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
LibmsiRecord *_libmsi_query_get_record( LibmsiDatabase *db, const char *fmt, ... )
{
    LibmsiRecord *rec = NULL;
    LibmsiQuery *view = NULL;
    unsigned r;
    int size = 100, res;
    char *query;

    /* construct the string */
    for (;;)
    {
        va_list va;
        query = msi_alloc( size*sizeof(char) );
        va_start(va, fmt);
        res = vsnprintf(query, size, fmt, va);
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
        msiobj_release( &view->hdr );
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

    *rec = libmsi_record_create(col_count);
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
            GsfInput *stm = NULL;

            ret = view->ops->fetch_stream(view, row, i, &stm);
            if ((ret == LIBMSI_RESULT_SUCCESS) && stm)
            {
                _libmsi_record_set_gsf_input(*rec, i, stm);
                g_object_unref(G_OBJECT(stm));
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

    r = msi_view_get_row(query->db, view, query->row, prec);
    if (r == LIBMSI_RESULT_SUCCESS)
    {
        query->row ++;
        _libmsi_record_set_int_ptr(*prec, 0, (intptr_t)query);
    }

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
    msiobj_addref( &query->hdr);
    ret = _libmsi_query_fetch( query, record );
    msiobj_release( &query->hdr );
    return ret;
}

LibmsiResult libmsi_query_close(LibmsiQuery *query)
{
    LibmsiView *view;
    unsigned ret;

    TRACE("%p\n", query );

    if( !query )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &query->hdr );
    view = query->view;
    if( !view )
        return LIBMSI_RESULT_FUNCTION_FAILED;
    if( !view->ops->close )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    ret = view->ops->close( view );
    msiobj_release( &query->hdr );
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
    msiobj_addref( &query->hdr );

    if( rec )
        msiobj_addref( &rec->hdr );

    ret = _libmsi_query_execute( query, rec );

    msiobj_release( &query->hdr );
    if( rec )
        msiobj_release( &rec->hdr );

    return ret;
}

static unsigned msi_set_record_type_string( LibmsiRecord *rec, unsigned field,
                                        unsigned type, bool temporary )
{
    static const char fmt[] = { '%','d',0 };
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

    return libmsi_record_set_string( rec, field, szType );
}

unsigned _libmsi_query_get_column_info( LibmsiQuery *query, LibmsiColInfo info, LibmsiRecord **prec )
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

    rec = libmsi_record_create( count );
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

    msiobj_addref ( &query->hdr );
    r = _libmsi_query_get_column_info( query, info, prec );
    msiobj_release( &query->hdr );

    return r;
}

LibmsiDBError libmsi_query_get_error( LibmsiQuery *query, char *buffer, unsigned *buflen )
{
    const char *column;
    LibmsiDBError r;
    unsigned len;

    TRACE("%u %p %p\n", query, buffer, buflen);

    if (!buflen)
        return LIBMSI_DB_ERROR_INVALIDARG;

    if (!query)
        return LIBMSI_DB_ERROR_INVALIDARG;

    msiobj_addref( &query->hdr);
    if ((r = query->view->error)) column = query->view->error_column;
    else column = szEmpty;

    len = strlen(column);
    if (buffer)
    {
        if (*buflen >= len)
            strcpy(buffer, column);
        else
            r = LIBMSI_DB_ERROR_MOREDATA;
    }

    *buflen = len;
    msiobj_release( &query->hdr );
    return r;
}

unsigned _libmsi_database_apply_transform( LibmsiDatabase *db,
                 const char *szTransformFile, int iErrorCond )
{
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    GsfInput *in;
    GsfInfile *stg;
    uint8_t uuid[16];

    TRACE("%p %s %d\n", db, debugstr_a(szTransformFile), iErrorCond);
    in = gsf_input_stdio_new(szTransformFile, NULL);
    if (!in)
    {
        WARN("open file failed for transform %s\n", debugstr_a(szTransformFile));
        return LIBMSI_RESULT_OPEN_FAILED;
    }
    stg = gsf_infile_msole_new( in, NULL );
    g_object_unref(G_OBJECT(in));

    if( !gsf_infile_msole_get_class_id (GSF_INFILE_MSOLE(stg), uuid))
    {
        FIXME("Failed to stat storage\n");
        goto end;
    }

    if ( memcmp( uuid, clsid_msi_transform, 16 ) != 0 )
        goto end;

    if( TRACE_ON( msi ) )
        enum_stream_names( stg );

    ret = msi_table_apply_transform( db, stg );

end:
    g_object_unref(G_OBJECT(stg));

    return ret;
}

LibmsiResult libmsi_database_apply_transform( LibmsiDatabase *db,
                 const char *szTransformFile, int iErrorCond)
{
    unsigned r;

    msiobj_addref( &db->hdr );
    if( !db )
        return LIBMSI_RESULT_INVALID_HANDLE;
    r = _libmsi_database_apply_transform( db, szTransformFile, iErrorCond );
    msiobj_release( &db->hdr );
    return r;
}

static int gsf_infile_copy(GsfInfile *inf, GsfOutfile *outf)
{
    int n = gsf_infile_num_children(inf);
    int i;

    for (i = 0; i < n; i++) {
        const char *name = gsf_infile_name_by_index(inf, i);
        GsfInput *child = gsf_infile_child_by_index(inf, i);
        GsfInfile *childf = GSF_IS_INFILE (child) ? GSF_INFILE (child) : NULL;
        gboolean is_dir = childf && gsf_infile_num_children (childf) > 0;
        GsfOutput *dest = gsf_outfile_new_child(outf, name, is_dir);
        gboolean ok;

        if (is_dir)
            ok = gsf_infile_copy(childf, GSF_OUTFILE(dest));
        else
            ok = gsf_input_copy(child, dest);

        g_object_unref(G_OBJECT(child));
        g_object_unref(G_OBJECT(dest));
        if (!ok)
            return false;
    }
    return true;
}

static unsigned commit_storage( const char *name, GsfInfile *stg, void *opaque)
{
    LibmsiDatabase *db = opaque;
    GsfOutfile *outstg;
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;

    TRACE("%s %p %p\n", debugstr_a(name), stg, opaque);

    outstg = GSF_OUTFILE(gsf_outfile_new_child( db->outfile, name, true ));
    if ( !outstg )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    if ( !gsf_infile_copy( stg, outstg ) )
        goto end;

    ret = LIBMSI_RESULT_SUCCESS;

end:
    gsf_output_close(GSF_OUTPUT(outstg));
    g_object_unref(G_OBJECT(outstg));
    return ret;
}

static unsigned commit_stream( const char *name, GsfInput *stm, void *opaque)
{
    LibmsiDatabase *db = opaque;
    GsfOutput *outstm;
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    char decname[0x40];

    decode_streamname(name, decname);
    TRACE("%s(%s) %p %p\n", debugstr_a(name), debugstr_a(decname), stm, opaque);

    outstm = gsf_outfile_new_child( db->outfile, name, false );
    if ( !outstm )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    gsf_input_seek (stm, 0, G_SEEK_SET);
    gsf_output_seek (outstm, 0, G_SEEK_SET);
    if ( !gsf_input_copy( stm, outstm ))
        goto end;

    ret = LIBMSI_RESULT_SUCCESS;

end:
    gsf_output_close(GSF_OUTPUT(outstm));
    g_object_unref(G_OBJECT(outstm));
    return ret;
}

LibmsiResult libmsi_database_commit( LibmsiDatabase *db )
{
    unsigned r = LIBMSI_RESULT_SUCCESS;
    unsigned bytes_per_strref;

    TRACE("%d\n", db);

    if( !db )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &db->hdr );
    if (db->mode == LIBMSI_DB_OPEN_READONLY)
        goto end;

    /* FIXME: lock the database */

    r = msi_save_string_table( db->strings, db, &bytes_per_strref );
    if( r != LIBMSI_RESULT_SUCCESS )
    {
        WARN("failed to save string table r=%08x\n",r);
        goto end;
    }

    r = msi_enum_db_storages( db, commit_storage, db );
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        WARN("failed to save storages r=%08x\n",r);
        goto end;
    }

    r = msi_enum_db_streams( db, commit_stream, db );
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        WARN("failed to save streams r=%08x\n",r);
        goto end;
    }

    r = _libmsi_database_commit_tables( db, bytes_per_strref );
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        WARN("failed to save tables r=%08x\n",r);
        goto end;
    }

    db->bytes_per_strref = bytes_per_strref;

    /* FIXME: unlock the database */

    _libmsi_database_close(db, true);
    _libmsi_database_open(db);
    _libmsi_database_start_transaction(db, LIBMSI_DB_OPEN_TRANSACT);

end:
    msiobj_release( &db->hdr );

    return r;
}

struct msi_primary_key_record_info
{
    unsigned n;
    LibmsiRecord *rec;
};

static unsigned msi_primary_key_iterator( LibmsiRecord *rec, void *param )
{
    struct msi_primary_key_record_info *info = param;
    const char *name;
    const char *table;
    unsigned type;

    type = libmsi_record_get_integer( rec, 4 );
    if( type & MSITYPE_KEY )
    {
        info->n++;
        if( info->rec )
        {
            if ( info->n == 1 )
            {
                table = _libmsi_record_get_string_raw( rec, 1 );
                libmsi_record_set_string( info->rec, 0, table);
            }

            name = _libmsi_record_get_string_raw( rec, 3 );
            libmsi_record_set_string( info->rec, info->n, name );
        }
    }

    return LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_database_get_primary_keys( LibmsiDatabase *db,
                const char *table, LibmsiRecord **prec )
{
    static const char sql[] = {
        's','e','l','e','c','t',' ','*',' ',
        'f','r','o','m',' ','`','_','C','o','l','u','m','n','s','`',' ',
        'w','h','e','r','e',' ',
        '`','T','a','b','l','e','`',' ','=',' ','\'','%','s','\'',0 };
    struct msi_primary_key_record_info info;
    LibmsiQuery *query = NULL;
    unsigned r;

    if (!table_view_exists( db, table ))
        return LIBMSI_RESULT_INVALID_TABLE;

    r = _libmsi_query_open( db, &query, sql, table );
    if( r != LIBMSI_RESULT_SUCCESS )
        return r;

    /* count the number of primary key records */
    info.n = 0;
    info.rec = 0;
    r = _libmsi_query_iterate_records( query, 0, msi_primary_key_iterator, &info );
    if( r == LIBMSI_RESULT_SUCCESS )
    {
        TRACE("Found %d primary keys\n", info.n );

        /* allocate a record and fill in the names of the tables */
        info.rec = libmsi_record_create( info.n );
        info.n = 0;
        r = _libmsi_query_iterate_records( query, 0, msi_primary_key_iterator, &info );
        if( r == LIBMSI_RESULT_SUCCESS )
            *prec = info.rec;
        else
            msiobj_release( &info.rec->hdr );
    }
    msiobj_release( &query->hdr );

    return r;
}

LibmsiResult libmsi_database_get_primary_keys(LibmsiDatabase *db, 
                    const char *table, LibmsiRecord **prec)
{
    unsigned r;

    TRACE("%d %s %p\n", db, debugstr_a(table), prec);

    if( !db )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &db->hdr );
    r = _libmsi_database_get_primary_keys( db, table, prec );
    msiobj_release( &db->hdr );

    return r;
}

LibmsiCondition libmsi_database_is_table_persistent(
              LibmsiDatabase *db, const char *szTableName)
{
    LibmsiCondition r;

    TRACE("%x %s\n", db, debugstr_a(szTableName));

    msiobj_addref( &db->hdr );
    if( !db )
        return LIBMSI_CONDITION_ERROR;

    r = _libmsi_database_is_table_persistent( db, szTableName );

    msiobj_release( &db->hdr );

    return r;
}
