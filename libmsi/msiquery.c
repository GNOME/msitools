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
#include "msiserver.h"

#include "initguid.h"


static void MSI_CloseQuery( LibmsiObject *arg )
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

unsigned VIEW_find_column( LibmsiView *table, const WCHAR *name, const WCHAR *table_name, unsigned *n )
{
    const WCHAR *col_name;
    const WCHAR *haystack_table_name;
    unsigned i, count, r;

    r = table->ops->get_dimensions( table, NULL, &count );
    if( r != ERROR_SUCCESS )
        return r;

    for( i=1; i<=count; i++ )
    {
        int x;

        r = table->ops->get_column_info( table, i, &col_name, NULL,
                                         NULL, &haystack_table_name );
        if( r != ERROR_SUCCESS )
            return r;
        x = strcmpW( name, col_name );
        if( table_name )
            x |= strcmpW( table_name, haystack_table_name );
        if( !x )
        {
            *n = i;
            return ERROR_SUCCESS;
        }
    }
    return ERROR_INVALID_PARAMETER;
}

unsigned MsiDatabaseOpenQuery(LibmsiDatabase *db,
              const char *szQuery, LibmsiQuery **pQuery)
{
    unsigned r;
    WCHAR *szwQuery;

    TRACE("%d %s %p\n", db, debugstr_a(szQuery), pQuery);

    if( szQuery )
    {
        szwQuery = strdupAtoW( szQuery );
        if( !szwQuery )
            return ERROR_FUNCTION_FAILED;
    }
    else
        szwQuery = NULL;

    if( !db )
        return ERROR_INVALID_HANDLE;
    msiobj_addref( &db->hdr);

    r = MSI_DatabaseOpenQueryW( db, szwQuery, pQuery );
    msiobj_release( &db->hdr );

    msi_free( szwQuery );
    return r;
}

unsigned MSI_DatabaseOpenQueryW(LibmsiDatabase *db,
              const WCHAR *szQuery, LibmsiQuery **pView)
{
    LibmsiQuery *query;
    unsigned r;

    TRACE("%s %p\n", debugstr_w(szQuery), pView);

    if( !szQuery)
        return ERROR_INVALID_PARAMETER;

    /* pre allocate a handle to hold a pointer to the view */
    query = alloc_msiobject( sizeof (LibmsiQuery), MSI_CloseQuery );
    if( !query )
        return ERROR_FUNCTION_FAILED;

    msiobj_addref( &db->hdr );
    query->db = db;
    list_init( &query->mem );

    r = MSI_ParseSQL( db, szQuery, &query->view, &query->mem );
    if( r == ERROR_SUCCESS )
    {
        msiobj_addref( &query->hdr );
        *pView = query;
    }

    msiobj_release( &query->hdr );
    return r;
}

unsigned MSI_OpenQuery( LibmsiDatabase *db, LibmsiQuery **view, const WCHAR *fmt, ... )
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
    r = MSI_DatabaseOpenQueryW(db, query, view);
    msi_free(query);
    return r;
}

unsigned MSI_IterateRecords( LibmsiQuery *view, unsigned *count,
                         record_func func, void *param )
{
    LibmsiRecord *rec = NULL;
    unsigned r, n = 0, max = 0;

    r = MSI_QueryExecute( view, NULL );
    if( r != ERROR_SUCCESS )
        return r;

    if( count )
        max = *count;

    /* iterate a query */
    for( n = 0; (max == 0) || (n < max); n++ )
    {
        r = MSI_QueryFetch( view, &rec );
        if( r != ERROR_SUCCESS )
            break;
        if (func)
            r = func( rec, param );
        msiobj_release( &rec->hdr );
        if( r != ERROR_SUCCESS )
            break;
    }

    MsiQueryClose( view );

    if( count )
        *count = n;

    if( r == ERROR_NO_MORE_ITEMS )
        r = ERROR_SUCCESS;

    return r;
}

/* return a single record from a query */
LibmsiRecord *MSI_QueryGetRecord( LibmsiDatabase *db, const WCHAR *fmt, ... )
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
    r = MSI_DatabaseOpenQueryW(db, query, &view);
    msi_free(query);

    if( r == ERROR_SUCCESS )
    {
        MSI_QueryExecute( view, NULL );
        MSI_QueryFetch( view, &rec );
        MsiQueryClose( view );
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
        return ERROR_INVALID_PARAMETER;

    if (row >= row_count)
        return ERROR_NO_MORE_ITEMS;

    *rec = MsiCreateRecord(col_count);
    if (!*rec)
        return ERROR_FUNCTION_FAILED;

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
            if ((ret == ERROR_SUCCESS) && stm)
            {
                MSI_RecordSetIStream(*rec, i, stm);
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
            MSI_RecordSetStringW(*rec, i, sval);
        }
        else
        {
            if ((type & MSI_DATASIZEMASK) == 2)
                MsiRecordSetInteger(*rec, i, ival - (1<<15));
            else
                MsiRecordSetInteger(*rec, i, ival - (1<<31));
        }
    }

    return ERROR_SUCCESS;
}

unsigned MSI_QueryFetch(LibmsiQuery *query, LibmsiRecord **prec)
{
    LibmsiView *view;
    unsigned r;

    TRACE("%p %p\n", query, prec );

    view = query->view;
    if( !view )
        return ERROR_FUNCTION_FAILED;

    r = msi_view_get_row(query->db, view, query->row, prec);
    if (r == ERROR_SUCCESS)
    {
        query->row ++;
        MSI_RecordSetIntPtr(*prec, 0, (intptr_t)query);
    }

    return r;
}

unsigned MsiQueryFetch(LibmsiQuery *query, LibmsiRecord **record)
{
    unsigned ret;

    TRACE("%d %p\n", query, record);

    if( !record )
        return ERROR_INVALID_PARAMETER;
    *record = 0;

    if( !query )
        return ERROR_INVALID_HANDLE;
    msiobj_addref( &query->hdr);
    ret = MSI_QueryFetch( query, record );
    msiobj_release( &query->hdr );
    return ret;
}

unsigned MsiQueryClose(LibmsiQuery *query)
{
    LibmsiView *view;
    unsigned ret;

    TRACE("%p\n", query );

    if( !query )
        return ERROR_INVALID_HANDLE;

    msiobj_addref( &query->hdr );
    view = query->view;
    if( !view )
        return ERROR_FUNCTION_FAILED;
    if( !view->ops->close )
        return ERROR_FUNCTION_FAILED;

    ret = view->ops->close( view );
    msiobj_release( &query->hdr );
    return ret;
}

unsigned MSI_QueryExecute(LibmsiQuery *query, LibmsiRecord *rec )
{
    LibmsiView *view;

    TRACE("%p %p\n", query, rec);

    view = query->view;
    if( !view )
        return ERROR_FUNCTION_FAILED;
    if( !view->ops->execute )
        return ERROR_FUNCTION_FAILED;
    query->row = 0;

    return view->ops->execute( view, rec );
}

unsigned MsiQueryExecute(LibmsiQuery *query, LibmsiRecord *rec)
{
    unsigned ret;
    
    TRACE("%d %d\n", query, rec);

    if( !query )
        return ERROR_INVALID_HANDLE;
    msiobj_addref( &query->hdr );

    if( rec )
        msiobj_addref( &rec->hdr );

    ret = MSI_QueryExecute( query, rec );

    msiobj_release( &query->hdr );
    if( rec )
        msiobj_release( &rec->hdr );

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

    return MSI_RecordSetStringW( rec, field, szType );
}

unsigned MSI_QueryGetColumnInfo( LibmsiQuery *query, LibmsiColInfo info, LibmsiRecord **prec )
{
    unsigned r = ERROR_FUNCTION_FAILED, i, count = 0, type;
    LibmsiRecord *rec;
    LibmsiView *view = query->view;
    const WCHAR *name;
    bool temporary;

    if( !view )
        return ERROR_FUNCTION_FAILED;

    if( !view->ops->get_dimensions )
        return ERROR_FUNCTION_FAILED;

    r = view->ops->get_dimensions( view, NULL, &count );
    if( r != ERROR_SUCCESS )
        return r;
    if( !count )
        return ERROR_INVALID_PARAMETER;

    rec = MsiCreateRecord( count );
    if( !rec )
        return ERROR_FUNCTION_FAILED;

    for( i=0; i<count; i++ )
    {
        name = NULL;
        r = view->ops->get_column_info( view, i+1, &name, &type, &temporary, NULL );
        if( r != ERROR_SUCCESS )
            continue;
        if (info == LIBMSI_COL_INFO_NAMES)
            MSI_RecordSetStringW( rec, i+1, name );
        else
            msi_set_record_type_string( rec, i+1, type, temporary );
    }
    *prec = rec;
    return ERROR_SUCCESS;
}

unsigned MsiQueryGetColumnInfo(LibmsiQuery *query, LibmsiColInfo info, LibmsiRecord **prec)
{
    unsigned r;

    TRACE("%d %d %p\n", query, info, prec);

    if( !prec )
        return ERROR_INVALID_PARAMETER;

    if( info != LIBMSI_COL_INFO_NAMES && info != LIBMSI_COL_INFO_TYPES )
        return ERROR_INVALID_PARAMETER;

    if( !query )
        return ERROR_INVALID_HANDLE;

    msiobj_addref ( &query->hdr );
    r = MSI_QueryGetColumnInfo( query, info, prec );
    msiobj_release( &query->hdr );

    return r;
}

unsigned MSI_QueryModify( LibmsiQuery *query, LibmsiModify mode, LibmsiRecord *rec )
{
    LibmsiView *view = NULL;
    unsigned r;

    if ( !query || !rec )
        return ERROR_INVALID_HANDLE;

    view = query->view;
    if ( !view  || !view->ops->modify)
        return ERROR_FUNCTION_FAILED;

    if ( mode == LIBMSI_MODIFY_UPDATE && MSI_RecordGetIntPtr( rec, 0 ) != (intptr_t)query )
        return ERROR_FUNCTION_FAILED;

    r = view->ops->modify( view, mode, rec, query->row );
    if (mode == LIBMSI_MODIFY_DELETE && r == ERROR_SUCCESS)
        query->row--;

    return r;
}

unsigned MsiQueryModify( LibmsiQuery *query, LibmsiModify eModifyMode,
                LibmsiRecord *rec)
{
    unsigned r = ERROR_FUNCTION_FAILED;

    TRACE("%d %x %d\n", query, eModifyMode, rec);

    if( !query )
        return ERROR_INVALID_HANDLE;

    msiobj_addref( &query->hdr);

    if (rec)
        msiobj_addref( &rec->hdr);

    r = MSI_QueryModify( query, eModifyMode, rec );

    msiobj_release( &query->hdr );
    if( rec )
        msiobj_release( &rec->hdr );

    return r;
}

LibmsiDBError MsiQueryGetError( LibmsiQuery *query, char *buffer, unsigned *buflen )
{
    const WCHAR *column;
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

    len = WideCharToMultiByte( CP_ACP, 0, column, -1, NULL, 0, NULL, NULL );
    if (buffer)
    {
        if (*buflen >= len)
            WideCharToMultiByte( CP_ACP, 0, column, -1, buffer, *buflen, NULL, NULL );
        else
            r = LIBMSI_DB_ERROR_MOREDATA;
    }
    *buflen = len - 1;
    msiobj_release( &query->hdr );
    return r;
}

unsigned MSI_DatabaseApplyTransform( LibmsiDatabase *db,
                 const char *szTransformFile, int iErrorCond )
{
    HRESULT r;
    unsigned ret = ERROR_FUNCTION_FAILED;
    IStorage *stg = NULL;
    STATSTG stat;
    WCHAR *szwTransformFile = NULL;

    TRACE("%p %s %d\n", db, debugstr_a(szTransformFile), iErrorCond);
    szwTransformFile = strdupAtoW(szTransformFile);
    if (!szwTransformFile) goto end;

    r = StgOpenStorage( szwTransformFile, NULL,
           STGM_DIRECT|STGM_READ|STGM_SHARE_DENY_WRITE, NULL, 0, &stg);
    if ( FAILED(r) )
    {
        WARN("failed to open transform 0x%08x\n", r);
        return ret;
    }

    r = IStorage_Stat( stg, &stat, STATFLAG_NONAME );
    if ( FAILED( r ) )
        goto end;

    if ( !IsEqualGUID( &stat.clsid, &CLSID_MsiTransform ) )
        goto end;

    if( TRACE_ON( msi ) )
        enum_stream_names( stg );

    ret = msi_table_apply_transform( db, stg );

end:
    msi_free(szwTransformFile);
    IStorage_Release( stg );

    return ret;
}

unsigned MsiDatabaseApplyTransform( LibmsiDatabase *db,
                 const char *szTransformFile, int iErrorCond)
{
    unsigned r;

    msiobj_addref( &db->hdr );
    if( !db )
        return ERROR_INVALID_HANDLE;
    r = MSI_DatabaseApplyTransform( db, szTransformFile, iErrorCond );
    msiobj_release( &db->hdr );
    return r;
}

unsigned MsiDatabaseCommit( LibmsiDatabase *db )
{
    unsigned r;

    TRACE("%d\n", db);

    msiobj_addref( &db->hdr );
    if( !db )
        return ERROR_INVALID_HANDLE;

    if (db->mode == LIBMSI_DB_OPEN_READONLY)
    {
        msiobj_release( &db->hdr );
        return ERROR_SUCCESS;
    }

    /* FIXME: lock the database */

    r = MSI_CommitTables( db );
    if (r != ERROR_SUCCESS) ERR("Failed to commit tables!\n");

    /* FIXME: unlock the database */

    msiobj_release( &db->hdr );

    if (r == ERROR_SUCCESS)
    {
        msi_free( db->deletefile );
        db->deletefile = NULL;
    }

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
    const WCHAR *name;
    const WCHAR *table;
    unsigned type;

    type = MsiRecordGetInteger( rec, 4 );
    if( type & MSITYPE_KEY )
    {
        info->n++;
        if( info->rec )
        {
            if ( info->n == 1 )
            {
                table = MSI_RecordGetStringRaw( rec, 1 );
                MSI_RecordSetStringW( info->rec, 0, table);
            }

            name = MSI_RecordGetStringRaw( rec, 3 );
            MSI_RecordSetStringW( info->rec, info->n, name );
        }
    }

    return ERROR_SUCCESS;
}

unsigned MSI_DatabaseGetPrimaryKeys( LibmsiDatabase *db,
                const WCHAR *table, LibmsiRecord **prec )
{
    static const WCHAR sql[] = {
        's','e','l','e','c','t',' ','*',' ',
        'f','r','o','m',' ','`','_','C','o','l','u','m','n','s','`',' ',
        'w','h','e','r','e',' ',
        '`','T','a','b','l','e','`',' ','=',' ','\'','%','s','\'',0 };
    struct msi_primary_key_record_info info;
    LibmsiQuery *query = NULL;
    unsigned r;

    if (!TABLE_Exists( db, table ))
        return ERROR_INVALID_TABLE;

    r = MSI_OpenQuery( db, &query, sql, table );
    if( r != ERROR_SUCCESS )
        return r;

    /* count the number of primary key records */
    info.n = 0;
    info.rec = 0;
    r = MSI_IterateRecords( query, 0, msi_primary_key_iterator, &info );
    if( r == ERROR_SUCCESS )
    {
        TRACE("Found %d primary keys\n", info.n );

        /* allocate a record and fill in the names of the tables */
        info.rec = MsiCreateRecord( info.n );
        info.n = 0;
        r = MSI_IterateRecords( query, 0, msi_primary_key_iterator, &info );
        if( r == ERROR_SUCCESS )
            *prec = info.rec;
        else
            msiobj_release( &info.rec->hdr );
    }
    msiobj_release( &query->hdr );

    return r;
}

unsigned MsiDatabaseGetPrimaryKeys(LibmsiDatabase *db, 
                    const char *table, LibmsiRecord **prec)
{
    WCHAR *szwTable = NULL;
    unsigned r;

    TRACE("%d %s %p\n", db, debugstr_a(table), prec);

    if( table )
    {
        szwTable = strdupAtoW( table );
        if( !szwTable )
            return ERROR_OUTOFMEMORY;
    }

    if( !db )
        return ERROR_INVALID_HANDLE;

    msiobj_addref( &db->hdr );
    r = MSI_DatabaseGetPrimaryKeys( db, szwTable, prec );
    msiobj_release( &db->hdr );
    msi_free( szwTable );

    return r;
}

LibmsiCondition MsiDatabaseIsTablePersistent(
              LibmsiDatabase *db, const char *szTableName)
{
    WCHAR *szwTableName = NULL;
    LibmsiCondition r;

    TRACE("%x %s\n", db, debugstr_a(szTableName));

    if( szTableName )
    {
        szwTableName = strdupAtoW( szTableName );
        if( !szwTableName )
            return LIBMSI_CONDITION_ERROR;
    }

    msiobj_addref( &db->hdr );
    if( !db )
        return LIBMSI_CONDITION_ERROR;

    r = MSI_DatabaseIsTablePersistent( db, szwTableName );

    msiobj_release( &db->hdr );
    msi_free( szwTableName );

    return r;
}
