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


static void MSI_CloseView( MSIOBJECT *arg )
{
    MSIQUERY *query = (MSIQUERY*) arg;
    struct list *ptr, *t;

    if( query->view && query->view->ops->delete )
        query->view->ops->delete( query->view );
    msiobj_release( &query->db->hdr );

    LIST_FOR_EACH_SAFE( ptr, t, &query->mem )
    {
        msi_free( ptr );
    }
}

unsigned VIEW_find_column( MSIVIEW *table, const WCHAR *name, const WCHAR *table_name, unsigned *n )
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

unsigned MsiDatabaseOpenViewA(MSIOBJECT *hdb,
              const char *szQuery, MSIOBJECT **phView)
{
    unsigned r;
    WCHAR *szwQuery;

    TRACE("%d %s %p\n", hdb, debugstr_a(szQuery), phView);

    if( szQuery )
    {
        szwQuery = strdupAtoW( szQuery );
        if( !szwQuery )
            return ERROR_FUNCTION_FAILED;
    }
    else
        szwQuery = NULL;

    r = MsiDatabaseOpenViewW( hdb, szwQuery, phView);

    msi_free( szwQuery );
    return r;
}

unsigned MSI_DatabaseOpenViewW(MSIDATABASE *db,
              const WCHAR *szQuery, MSIQUERY **pView)
{
    MSIQUERY *query;
    unsigned r;

    TRACE("%s %p\n", debugstr_w(szQuery), pView);

    if( !szQuery)
        return ERROR_INVALID_PARAMETER;

    /* pre allocate a handle to hold a pointer to the view */
    query = alloc_msiobject( MSIOBJECTTYPE_VIEW, sizeof (MSIQUERY),
                              MSI_CloseView );
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

unsigned MSI_OpenQuery( MSIDATABASE *db, MSIQUERY **view, const WCHAR *fmt, ... )
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
    r = MSI_DatabaseOpenViewW(db, query, view);
    msi_free(query);
    return r;
}

unsigned MSI_IterateRecords( MSIQUERY *view, unsigned *count,
                         record_func func, void *param )
{
    MSIRECORD *rec = NULL;
    unsigned r, n = 0, max = 0;

    r = MSI_ViewExecute( view, NULL );
    if( r != ERROR_SUCCESS )
        return r;

    if( count )
        max = *count;

    /* iterate a query */
    for( n = 0; (max == 0) || (n < max); n++ )
    {
        r = MSI_ViewFetch( view, &rec );
        if( r != ERROR_SUCCESS )
            break;
        if (func)
            r = func( rec, param );
        msiobj_release( &rec->hdr );
        if( r != ERROR_SUCCESS )
            break;
    }

    MSI_ViewClose( view );

    if( count )
        *count = n;

    if( r == ERROR_NO_MORE_ITEMS )
        r = ERROR_SUCCESS;

    return r;
}

/* return a single record from a query */
MSIRECORD *MSI_QueryGetRecord( MSIDATABASE *db, const WCHAR *fmt, ... )
{
    MSIRECORD *rec = NULL;
    MSIQUERY *view = NULL;
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
    r = MSI_DatabaseOpenViewW(db, query, &view);
    msi_free(query);

    if( r == ERROR_SUCCESS )
    {
        MSI_ViewExecute( view, NULL );
        MSI_ViewFetch( view, &rec );
        MSI_ViewClose( view );
        msiobj_release( &view->hdr );
    }
    return rec;
}

unsigned MsiDatabaseOpenViewW(MSIOBJECT *hdb,
              const WCHAR *szQuery, MSIOBJECT **phView)
{
    MSIDATABASE *db;
    MSIQUERY *query = NULL;
    unsigned ret;

    TRACE("%s %p\n", debugstr_w(szQuery), phView);

    db = msihandle2msiinfo( hdb, MSIOBJECTTYPE_DATABASE );
    if( !db )
        return ERROR_INVALID_HANDLE;

    ret = MSI_DatabaseOpenViewW( db, szQuery, &query );
    if( ret == ERROR_SUCCESS )
        *phView = &query->hdr;
    msiobj_release( &db->hdr );

    return ret;
}

unsigned msi_view_get_row(MSIDATABASE *db, MSIVIEW *view, unsigned row, MSIRECORD **rec)
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

    *rec = MSI_CreateRecord(col_count);
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
                MSI_RecordSetInteger(*rec, i, ival - (1<<15));
            else
                MSI_RecordSetInteger(*rec, i, ival - (1<<31));
        }
    }

    return ERROR_SUCCESS;
}

unsigned MSI_ViewFetch(MSIQUERY *query, MSIRECORD **prec)
{
    MSIVIEW *view;
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

unsigned MsiViewFetch(MSIOBJECT *hView, MSIOBJECT **record)
{
    MSIQUERY *query;
    MSIRECORD *rec = NULL;
    unsigned ret;

    TRACE("%d %p\n", hView, record);

    if( !record )
        return ERROR_INVALID_PARAMETER;
    *record = 0;

    query = msihandle2msiinfo( hView, MSIOBJECTTYPE_VIEW );
    if( !query )
        return ERROR_INVALID_HANDLE;
    ret = MSI_ViewFetch( query, &rec );
    if( ret == ERROR_SUCCESS )
        *record = &rec->hdr;
    msiobj_release( &query->hdr );
    return ret;
}

unsigned MSI_ViewClose(MSIQUERY *query)
{
    MSIVIEW *view;

    TRACE("%p\n", query );

    view = query->view;
    if( !view )
        return ERROR_FUNCTION_FAILED;
    if( !view->ops->close )
        return ERROR_FUNCTION_FAILED;

    return view->ops->close( view );
}

unsigned MsiViewClose(MSIOBJECT *hView)
{
    MSIQUERY *query;
    unsigned ret;

    TRACE("%d\n", hView );

    query = msihandle2msiinfo( hView, MSIOBJECTTYPE_VIEW );
    if( !query )
        return ERROR_INVALID_HANDLE;

    ret = MSI_ViewClose( query );
    msiobj_release( &query->hdr );
    return ret;
}

unsigned MSI_ViewExecute(MSIQUERY *query, MSIRECORD *rec )
{
    MSIVIEW *view;

    TRACE("%p %p\n", query, rec);

    view = query->view;
    if( !view )
        return ERROR_FUNCTION_FAILED;
    if( !view->ops->execute )
        return ERROR_FUNCTION_FAILED;
    query->row = 0;

    return view->ops->execute( view, rec );
}

unsigned MsiViewExecute(MSIOBJECT *hView, MSIOBJECT *hRec)
{
    MSIQUERY *query;
    MSIRECORD *rec = NULL;
    unsigned ret;
    
    TRACE("%d %d\n", hView, hRec);

    query = msihandle2msiinfo( hView, MSIOBJECTTYPE_VIEW );
    if( !query )
        return ERROR_INVALID_HANDLE;

    if( hRec )
    {
        rec = msihandle2msiinfo( hRec, MSIOBJECTTYPE_RECORD );
        if( !rec )
        {
            ret = ERROR_INVALID_HANDLE;
            goto out;
        }
    }

    msiobj_lock( &rec->hdr );
    ret = MSI_ViewExecute( query, rec );
    msiobj_unlock( &rec->hdr );

out:
    msiobj_release( &query->hdr );
    if( rec )
        msiobj_release( &rec->hdr );

    return ret;
}

static unsigned msi_set_record_type_string( MSIRECORD *rec, unsigned field,
                                        unsigned type, BOOL temporary )
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

unsigned MSI_ViewGetColumnInfo( MSIQUERY *query, MSICOLINFO info, MSIRECORD **prec )
{
    unsigned r = ERROR_FUNCTION_FAILED, i, count = 0, type;
    MSIRECORD *rec;
    MSIVIEW *view = query->view;
    const WCHAR *name;
    BOOL temporary;

    if( !view )
        return ERROR_FUNCTION_FAILED;

    if( !view->ops->get_dimensions )
        return ERROR_FUNCTION_FAILED;

    r = view->ops->get_dimensions( view, NULL, &count );
    if( r != ERROR_SUCCESS )
        return r;
    if( !count )
        return ERROR_INVALID_PARAMETER;

    rec = MSI_CreateRecord( count );
    if( !rec )
        return ERROR_FUNCTION_FAILED;

    for( i=0; i<count; i++ )
    {
        name = NULL;
        r = view->ops->get_column_info( view, i+1, &name, &type, &temporary, NULL );
        if( r != ERROR_SUCCESS )
            continue;
        if (info == MSICOLINFO_NAMES)
            MSI_RecordSetStringW( rec, i+1, name );
        else
            msi_set_record_type_string( rec, i+1, type, temporary );
    }
    *prec = rec;
    return ERROR_SUCCESS;
}

unsigned MsiViewGetColumnInfo(MSIOBJECT *hView, MSICOLINFO info, MSIOBJECT **hRec)
{
    MSIQUERY *query = NULL;
    MSIRECORD *rec = NULL;
    unsigned r;

    TRACE("%d %d %p\n", hView, info, hRec);

    if( !hRec )
        return ERROR_INVALID_PARAMETER;

    if( info != MSICOLINFO_NAMES && info != MSICOLINFO_TYPES )
        return ERROR_INVALID_PARAMETER;

    query = msihandle2msiinfo( hView, MSIOBJECTTYPE_VIEW );
    if( !query )
        return ERROR_INVALID_HANDLE;

    r = MSI_ViewGetColumnInfo( query, info, &rec );
    if ( r == ERROR_SUCCESS )
        *hRec = &rec->hdr;

    msiobj_release( &query->hdr );

    return r;
}

unsigned MSI_ViewModify( MSIQUERY *query, MSIMODIFY mode, MSIRECORD *rec )
{
    MSIVIEW *view = NULL;
    unsigned r;

    if ( !query || !rec )
        return ERROR_INVALID_HANDLE;

    view = query->view;
    if ( !view  || !view->ops->modify)
        return ERROR_FUNCTION_FAILED;

    if ( mode == MSIMODIFY_UPDATE && MSI_RecordGetIntPtr( rec, 0 ) != (intptr_t)query )
        return ERROR_FUNCTION_FAILED;

    r = view->ops->modify( view, mode, rec, query->row );
    if (mode == MSIMODIFY_DELETE && r == ERROR_SUCCESS)
        query->row--;

    return r;
}

unsigned MsiViewModify( MSIOBJECT *hView, MSIMODIFY eModifyMode,
                MSIOBJECT *hRecord)
{
    MSIQUERY *query = NULL;
    MSIRECORD *rec = NULL;
    unsigned r = ERROR_FUNCTION_FAILED;

    TRACE("%d %x %d\n", hView, eModifyMode, hRecord);

    query = msihandle2msiinfo( hView, MSIOBJECTTYPE_VIEW );
    if( !query )
        return ERROR_INVALID_HANDLE;

    rec = msihandle2msiinfo( hRecord, MSIOBJECTTYPE_RECORD );
    r = MSI_ViewModify( query, eModifyMode, rec );

    msiobj_release( &query->hdr );
    if( rec )
        msiobj_release( &rec->hdr );

    return r;
}

MSIDBERROR MsiViewGetErrorW( MSIOBJECT *handle, WCHAR *buffer, unsigned *buflen )
{
    MSIQUERY *query;
    const WCHAR *column;
    MSIDBERROR r;
    unsigned len;

    TRACE("%u %p %p\n", handle, buffer, buflen);

    if (!buflen)
        return MSIDBERROR_INVALIDARG;

    query = msihandle2msiinfo( handle, MSIOBJECTTYPE_VIEW );
    if( !query )
        return MSIDBERROR_INVALIDARG;

    if ((r = query->view->error)) column = query->view->error_column;
    else column = szEmpty;

    len = strlenW( column );
    if (buffer)
    {
        if (*buflen > len)
            strcpyW( buffer, column );
        else
            r = MSIDBERROR_MOREDATA;
    }
    *buflen = len;
    msiobj_release( &query->hdr );
    return r;
}

MSIDBERROR MsiViewGetErrorA( MSIOBJECT *handle, char *buffer, unsigned *buflen )
{
    MSIQUERY *query;
    const WCHAR *column;
    MSIDBERROR r;
    unsigned len;

    TRACE("%u %p %p\n", handle, buffer, buflen);

    if (!buflen)
        return MSIDBERROR_INVALIDARG;

    query = msihandle2msiinfo( handle, MSIOBJECTTYPE_VIEW );
    if (!query)
        return MSIDBERROR_INVALIDARG;

    if ((r = query->view->error)) column = query->view->error_column;
    else column = szEmpty;

    len = WideCharToMultiByte( CP_ACP, 0, column, -1, NULL, 0, NULL, NULL );
    if (buffer)
    {
        if (*buflen >= len)
            WideCharToMultiByte( CP_ACP, 0, column, -1, buffer, *buflen, NULL, NULL );
        else
            r = MSIDBERROR_MOREDATA;
    }
    *buflen = len - 1;
    msiobj_release( &query->hdr );
    return r;
}

MSIOBJECT * MsiGetLastErrorRecord( void )
{
    FIXME("\n");
    return 0;
}

unsigned MSI_DatabaseApplyTransformW( MSIDATABASE *db,
                 const WCHAR *szTransformFile, int iErrorCond )
{
    HRESULT r;
    unsigned ret = ERROR_FUNCTION_FAILED;
    IStorage *stg = NULL;
    STATSTG stat;

    TRACE("%p %s %d\n", db, debugstr_w(szTransformFile), iErrorCond);

    r = StgOpenStorage( szTransformFile, NULL,
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
    IStorage_Release( stg );

    return ret;
}

unsigned MsiDatabaseApplyTransformW( MSIOBJECT *hdb,
                 const WCHAR *szTransformFile, int iErrorCond)
{
    MSIDATABASE *db;
    unsigned r;

    db = msihandle2msiinfo( hdb, MSIOBJECTTYPE_DATABASE );
    if( !db )
        return ERROR_INVALID_HANDLE;
    r = MSI_DatabaseApplyTransformW( db, szTransformFile, iErrorCond );
    msiobj_release( &db->hdr );
    return r;
}

unsigned MsiDatabaseApplyTransformA( MSIOBJECT *hdb, 
                 const char *szTransformFile, int iErrorCond)
{
    WCHAR *wstr;
    unsigned ret;

    TRACE("%d %s %d\n", hdb, debugstr_a(szTransformFile), iErrorCond);

    wstr = strdupAtoW( szTransformFile );
    if( szTransformFile && !wstr )
        return ERROR_NOT_ENOUGH_MEMORY;

    ret = MsiDatabaseApplyTransformW( hdb, wstr, iErrorCond);

    msi_free( wstr );

    return ret;
}

unsigned MsiDatabaseCommit( MSIOBJECT *hdb )
{
    MSIDATABASE *db;
    unsigned r;

    TRACE("%d\n", hdb);

    db = msihandle2msiinfo( hdb, MSIOBJECTTYPE_DATABASE );
    if( !db )
        return ERROR_INVALID_HANDLE;

    if (db->mode == MSIDBOPEN_READONLY)
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
    MSIRECORD *rec;
};

static unsigned msi_primary_key_iterator( MSIRECORD *rec, void *param )
{
    struct msi_primary_key_record_info *info = param;
    const WCHAR *name;
    const WCHAR *table;
    unsigned type;

    type = MSI_RecordGetInteger( rec, 4 );
    if( type & MSITYPE_KEY )
    {
        info->n++;
        if( info->rec )
        {
            if ( info->n == 1 )
            {
                table = MSI_RecordGetString( rec, 1 );
                MSI_RecordSetStringW( info->rec, 0, table);
            }

            name = MSI_RecordGetString( rec, 3 );
            MSI_RecordSetStringW( info->rec, info->n, name );
        }
    }

    return ERROR_SUCCESS;
}

unsigned MSI_DatabaseGetPrimaryKeys( MSIDATABASE *db,
                const WCHAR *table, MSIRECORD **prec )
{
    static const WCHAR sql[] = {
        's','e','l','e','c','t',' ','*',' ',
        'f','r','o','m',' ','`','_','C','o','l','u','m','n','s','`',' ',
        'w','h','e','r','e',' ',
        '`','T','a','b','l','e','`',' ','=',' ','\'','%','s','\'',0 };
    struct msi_primary_key_record_info info;
    MSIQUERY *query = NULL;
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
        info.rec = MSI_CreateRecord( info.n );
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

unsigned MsiDatabaseGetPrimaryKeysW( MSIOBJECT *hdb,
                    const WCHAR *table, MSIOBJECT **phRec )
{
    MSIRECORD *rec = NULL;
    MSIDATABASE *db;
    unsigned r;

    TRACE("%d %s %p\n", hdb, debugstr_w(table), phRec);

    db = msihandle2msiinfo( hdb, MSIOBJECTTYPE_DATABASE );
    if( !db )
        return ERROR_INVALID_HANDLE;

    r = MSI_DatabaseGetPrimaryKeys( db, table, &rec );
    if( r == ERROR_SUCCESS )
        *phRec = &rec->hdr;
    msiobj_release( &db->hdr );

    return r;
}

unsigned MsiDatabaseGetPrimaryKeysA(MSIOBJECT *hdb, 
                    const char *table, MSIOBJECT **phRec)
{
    WCHAR *szwTable = NULL;
    unsigned r;

    TRACE("%d %s %p\n", hdb, debugstr_a(table), phRec);

    if( table )
    {
        szwTable = strdupAtoW( table );
        if( !szwTable )
            return ERROR_OUTOFMEMORY;
    }
    r = MsiDatabaseGetPrimaryKeysW( hdb, szwTable, phRec );
    msi_free( szwTable );

    return r;
}

MSICONDITION MsiDatabaseIsTablePersistentA(
              MSIOBJECT *hDatabase, const char *szTableName)
{
    WCHAR *szwTableName = NULL;
    MSICONDITION r;

    TRACE("%x %s\n", hDatabase, debugstr_a(szTableName));

    if( szTableName )
    {
        szwTableName = strdupAtoW( szTableName );
        if( !szwTableName )
            return MSICONDITION_ERROR;
    }
    r = MsiDatabaseIsTablePersistentW( hDatabase, szwTableName );
    msi_free( szwTableName );

    return r;
}

MSICONDITION MsiDatabaseIsTablePersistentW(
              MSIOBJECT *hDatabase, const WCHAR *szTableName)
{
    MSIDATABASE *db;
    MSICONDITION r;

    TRACE("%x %s\n", hDatabase, debugstr_w(szTableName));

    db = msihandle2msiinfo( hDatabase, MSIOBJECTTYPE_DATABASE );
    if( !db )
        return MSICONDITION_ERROR;

    r = MSI_DatabaseIsTablePersistent( db, szTableName );

    msiobj_release( &db->hdr );

    return r;
}
