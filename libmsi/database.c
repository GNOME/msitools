/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002,2003,2004,2005 Mike McCormack for CodeWeavers
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
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define COBJMACROS
#define NONAMELESSUNION

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "debug.h"
#include "unicode.h"
#include "libmsi.h"
#include "msipriv.h"
#include "objidl.h"
#include "objbase.h"
#include "msiserver.h"
#include "query.h"

#include "initguid.h"


/*
 *  .MSI  file format
 *
 *  An .msi file is a structured storage file.
 *  It contains a number of streams.
 *  A stream for each table in the database.
 *  Two streams for the string table in the database.
 *  Any binary data in a table is a reference to a stream.
 */

#define IS_INTMSIDBOPEN(x)      \
      ((x) >= LIBMSI_DB_OPEN_READONLY && (x) <= LIBMSI_DB_OPEN_CREATE)

typedef struct LibmsiTransform {
    struct list entry;
    IStorage *stg;
} LibmsiTransform;

typedef struct LibmsiStorage {
    struct list entry;
    WCHAR *name;
    IStorage *stg;
} LibmsiStorage;

typedef struct LibmsiStream {
    struct list entry;
    WCHAR *name;
    IStorage *stg;
    IStream *stm;
} LibmsiStream;

static HRESULT stream_to_storage(IStream *stm, IStorage **stg)
{
    ILockBytes *lockbytes = NULL;
    STATSTG stat;
    void *data;
    HRESULT hr;
    unsigned size, read;
    ULARGE_INTEGER offset;

    hr = IStream_Stat(stm, &stat, STATFLAG_NONAME);
    if (FAILED(hr))
        return hr;

    if (stat.cbSize.QuadPart >> 32)
    {
        ERR("Storage is too large\n");
        return E_FAIL;
    }

    size = stat.cbSize.QuadPart;
    data = msi_alloc(size);
    if (!data)
        return E_OUTOFMEMORY;

    hr = IStream_Read(stm, data, size, &read);
    if (FAILED(hr) || read != size)
        goto done;

    hr = CreateILockBytesOnHGlobal(NULL, true, &lockbytes);
    if (FAILED(hr))
        goto done;

    ZeroMemory(&offset, sizeof(ULARGE_INTEGER));
    hr = ILockBytes_WriteAt(lockbytes, offset, data, size, &read);
    if (FAILED(hr) || read != size)
        goto done;

    hr = StgOpenStorageOnILockBytes(lockbytes, NULL,
                                    STGM_READWRITE | STGM_SHARE_DENY_NONE,
                                    NULL, 0, stg);
    if (FAILED(hr))
        goto done;

done:
    msi_free(data);
    if (lockbytes) ILockBytes_Release(lockbytes);
    return hr;
}

unsigned msi_open_storage( LibmsiDatabase *db, const WCHAR *stname )
{
    unsigned r;
    HRESULT hr;
    LibmsiStorage *storage;

    LIST_FOR_EACH_ENTRY( storage, &db->storages, LibmsiStorage, entry )
    {
        if( !strcmpW( stname, storage->name ) )
        {
            TRACE("found %s\n", debugstr_w(stname));
            return;
        }
    }

    if (!(storage = msi_alloc_zero( sizeof(LibmsiStorage) ))) return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
    storage->name = strdupW( stname );
    if (!storage->name)
    {
        r = LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
        goto done;
    }

    hr = IStorage_OpenStorage(db->infile, stname, NULL,
                              STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0,
                              &storage->stg);
    if (FAILED(hr))
    {
        r = LIBMSI_RESULT_FUNCTION_FAILED;
        goto done;
    }

    list_add_tail( &db->storages, &storage->entry );
    r = LIBMSI_RESULT_SUCCESS;

done:
    if (r != LIBMSI_RESULT_SUCCESS) {
        msi_free(storage->name);
        msi_free(storage);
    }

    return r;
}

unsigned msi_create_storage( LibmsiDatabase *db, const WCHAR *stname, IStream *stm )
{
    LibmsiStorage *storage;
    IStorage *origstg = NULL;
    IStorage *substg = NULL;
    bool found = false;
    HRESULT hr;
    unsigned r;

    LIST_FOR_EACH_ENTRY( storage, &db->storages, LibmsiStorage, entry )
    {
        if( !strcmpW( stname, storage->name ) )
        {
            TRACE("found %s\n", debugstr_w(stname));
            found = true;
            break;
        }
    }

    if (!found) {
        if (!(storage = msi_alloc_zero( sizeof(LibmsiStorage) ))) return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
        storage->name = strdupW( stname );
        if (!storage->name)
        {
            msi_free(storage);
            return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
        }
    }

    r = stream_to_storage(stm, &origstg);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    hr = IStorage_CreateStorage(db->outfile, stname,
                                STGM_WRITE | STGM_SHARE_EXCLUSIVE,
                                0, 0, &substg);
    if (FAILED(hr))
    {
        r = LIBMSI_RESULT_FUNCTION_FAILED;
        goto done;
    }

    hr = IStorage_CopyTo(origstg, 0, NULL, NULL, substg);
    if (FAILED(hr))
    {
        r = LIBMSI_RESULT_FUNCTION_FAILED;
        goto done;
    }

    if (found) {
        if (storage->stg)
            IStorage_Release(storage->stg);
    } else {
        list_add_tail( &db->storages, &storage->entry );
    }

    storage->stg = origstg;
    IStorage_AddRef(storage->stg);

    r = LIBMSI_RESULT_SUCCESS;

done:
    if (r != LIBMSI_RESULT_SUCCESS) {
        if (!found) {
            msi_free(storage->name);
            msi_free(storage);
        }
    }

    if (substg)
        IStorage_Release(substg);
    if (origstg)
        IStorage_Release(origstg);

    return r;
}

void msi_destroy_storage( LibmsiDatabase *db, const WCHAR *stname )
{
    LibmsiStorage *storage, *storage2;

    LIST_FOR_EACH_ENTRY_SAFE( storage, storage2, &db->storages, LibmsiStorage, entry )
    {
        if (!strcmpW( stname, storage->name ))
        {
            TRACE("destroying %s\n", debugstr_w(stname));

            list_remove( &storage->entry );
            IStorage_Release( storage->stg );
            IStorage_DestroyElement( storage->stg, stname );
            msi_free( storage );
            break;
        }
    }
}

static unsigned find_open_stream( LibmsiDatabase *db, IStorage *stg, const WCHAR *name, IStream **stm )
{
    LibmsiStream *stream;

    LIST_FOR_EACH_ENTRY( stream, &db->streams, LibmsiStream, entry )
    {
        if (stream->stg != stg) continue;

        if( !strcmpW( name, stream->name ) )
        {
            TRACE("found %s\n", debugstr_w(name));
            *stm = stream->stm;
            return LIBMSI_RESULT_SUCCESS;
        }
    }

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

unsigned write_raw_stream_data( LibmsiDatabase *db, const WCHAR *stname,
                        const void *data, unsigned sz )
{
    HRESULT r;
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    unsigned count;
    IStream *stm = NULL;
    ULARGE_INTEGER size;
    LARGE_INTEGER pos;

    r = IStorage_OpenStream( db->outfile, stname, NULL, 
            STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, &stm);
    if( FAILED(r) )
    {
        r = IStorage_CreateStream( db->outfile, stname,
                STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm);
    }
    if( FAILED( r ) )
    {
        WARN("open stream failed r = %08x\n", r);
        return ret;
    }

    size.QuadPart = sz;
    r = IStream_SetSize( stm, size );
    if( FAILED( r ) )
    {
        WARN("Failed to SetSize\n");
        goto end;
    }

    pos.QuadPart = 0;
    r = IStream_Seek( stm, pos, STREAM_SEEK_SET, NULL );
    if( FAILED( r ) )
    {
        WARN("Failed to Seek\n");
        goto end;
    }

    if (sz)
    {
        r = IStream_Write(stm, data, sz, &count );
        if( FAILED( r ) || ( count != sz ) )
        {
            WARN("Failed to Write\n");
            goto end;
        }
    }

    ret = LIBMSI_RESULT_SUCCESS;

end:
    IStream_Release( stm );

    return ret;
}

static void cache_infile_structure( LibmsiDatabase *db )
{
    IEnumSTATSTG *stgenum = NULL;
    STATSTG stat;
    IStream *stream;
    HRESULT hr;
    unsigned r, size;

    hr = IStorage_EnumElements(db->infile, 0, NULL, 0, &stgenum);
    if (FAILED(hr))
        return;

    while (true)
    {
        size = 0;
        hr = IEnumSTATSTG_Next(stgenum, 1, &stat, &size);
        if (FAILED(hr) || !size)
            break;

        /* table streams are not in the _Streams table */
        if (stat.type == STGTY_STREAM && *stat.pwcsName == 0x4840)
        {
            CoTaskMemFree(stat.pwcsName);
            continue;
        }

        if (stat.type == STGTY_STREAM) {
            r = msi_get_raw_stream(db, stat.pwcsName, &stream);
            if ( r == 0 ) {
                IStream_Release(stream);
            }
        } else {
            msi_open_storage(db, stat.pwcsName);
        }

        CoTaskMemFree(stat.pwcsName);
    }

    IEnumSTATSTG_Release(stgenum);
}

unsigned msi_clone_open_stream( LibmsiDatabase *db, IStorage *stg, const WCHAR *name, IStream **stm )
{
    IStream *stream;

    if (find_open_stream( db, stg, name, &stream ) == LIBMSI_RESULT_SUCCESS)
    {
        HRESULT r;
        LARGE_INTEGER pos;

        r = IStream_Clone( stream, stm );
        if( FAILED( r ) )
        {
            WARN("failed to clone stream r = %08x!\n", r);
            return LIBMSI_RESULT_FUNCTION_FAILED;
        }

        pos.QuadPart = 0;
        r = IStream_Seek( *stm, pos, STREAM_SEEK_SET, NULL );
        if( FAILED( r ) )
        {
            IStream_Release( *stm );
            return LIBMSI_RESULT_FUNCTION_FAILED;
        }

        return LIBMSI_RESULT_SUCCESS;
    }

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

unsigned msi_get_raw_stream( LibmsiDatabase *db, const WCHAR *stname, IStream **stm )
{
    HRESULT r;
    IStorage *stg;
    WCHAR decoded[MAX_STREAM_NAME_LEN];

    decode_streamname( stname, decoded );
    TRACE("%s -> %s\n", debugstr_w(stname), debugstr_w(decoded));

    if (msi_clone_open_stream( db, db->infile, stname, stm ) == LIBMSI_RESULT_SUCCESS)
        return LIBMSI_RESULT_SUCCESS;

    r = IStorage_OpenStream( db->infile, stname, NULL,
                             STGM_READ | STGM_SHARE_EXCLUSIVE, 0, stm );
    if( FAILED( r ) )
    {
        LibmsiTransform *transform;

        LIST_FOR_EACH_ENTRY( transform, &db->transforms, LibmsiTransform, entry )
        {
            r = IStorage_OpenStream( transform->stg, stname, NULL,
                                     STGM_READ | STGM_SHARE_EXCLUSIVE, 0, stm );
            if (SUCCEEDED(r))
            {
                stg = transform->stg;
                break;
            }
        }
    }
    else stg = db->infile;

    if( SUCCEEDED(r) )
    {
        LibmsiStream *stream;

        if (!(stream = msi_alloc( sizeof(LibmsiStream) ))) return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
        stream->name = strdupW( stname );
        stream->stg = stg;
        IStorage_AddRef( stg );
        stream->stm = *stm;
        IStream_AddRef( *stm );
        list_add_tail( &db->streams, &stream->entry );
    }

    return SUCCEEDED(r) ? LIBMSI_RESULT_SUCCESS : LIBMSI_RESULT_FUNCTION_FAILED;
}

static void free_transforms( LibmsiDatabase *db )
{
    while( !list_empty( &db->transforms ) )
    {
        LibmsiTransform *t = LIST_ENTRY( list_head( &db->transforms ),
                                      LibmsiTransform, entry );
        list_remove( &t->entry );
        IStorage_Release( t->stg );
        msi_free( t );
    }
}

void msi_destroy_stream( LibmsiDatabase *db, const WCHAR *stname )
{
    LibmsiStream *stream, *stream2;

    LIST_FOR_EACH_ENTRY_SAFE( stream, stream2, &db->streams, LibmsiStream, entry )
    {
        if (!strcmpW( stname, stream->name ))
        {
            TRACE("destroying %s\n", debugstr_w(stname));

            list_remove( &stream->entry );
            IStream_Release( stream->stm );
            IStorage_Release( stream->stg );
            IStorage_DestroyElement( stream->stg, stname );
            msi_free( stream );
            break;
        }
    }
}

static void free_storages( LibmsiDatabase *db )
{
    while( !list_empty( &db->storages ) )
    {
        LibmsiStorage *s = LIST_ENTRY(list_head( &db->storages ), LibmsiStorage, entry);
        list_remove( &s->entry );
        IStorage_Release( s->stg );
        msi_free( s->name );
        msi_free( s );
    }
}

static void free_streams( LibmsiDatabase *db )
{
    while( !list_empty( &db->streams ) )
    {
        LibmsiStream *s = LIST_ENTRY(list_head( &db->streams ), LibmsiStream, entry);
        list_remove( &s->entry );
        IStream_Release( s->stm );
        IStorage_Release( s->stg );
        msi_free( s->name );
        msi_free( s );
    }
}

void append_storage_to_db( LibmsiDatabase *db, IStorage *stg )
{
    LibmsiTransform *t;

    t = msi_alloc( sizeof *t );
    t->stg = stg;
    IStorage_AddRef( stg );
    list_add_head( &db->transforms, &t->entry );

    /* the transform may add or replace streams */
    free_streams( db );
}

static VOID _libmsi_database_destroy( LibmsiObject *arg )
{
    LibmsiDatabase *db = (LibmsiDatabase *) arg;

    msi_free(db->path);
    free_cached_tables( db );
    free_streams( db );
    free_storages( db );
    free_transforms( db );
    if (db->strings) msi_destroy_stringtable( db->strings );
    IStorage_Release( db->infile );
    IStorage_Release( db->outfile );
    if (db->deletefile)
    {
        unlink( db->deletefile );
        msi_free( db->deletefile );
    }
}

static HRESULT db_initialize( LibmsiDatabase *db )
{
    static const WCHAR szTables[]  = { '_','T','a','b','l','e','s',0 };
    HRESULT hr;

    /* create the _Tables stream */
    hr = write_stream_data( db, szTables, NULL, 0 );
    if (FAILED( hr ))
    {
        WARN("failed to create _Tables stream 0x%08x\n", hr);
        return hr;
    }

    hr = msi_init_string_table( db );
    if (FAILED( hr ))
    {
        WARN("failed to initialize string table 0x%08x\n", hr);
        return hr;
    }

    hr = IStorage_Commit( db->outfile, 0 );
    if (FAILED( hr ))
    {
        WARN("failed to commit changes 0x%08x\n", hr);
        return hr;
    }
    return S_OK;
}

LibmsiResult libmsi_database_open(const char *szDBPath, const char *szPersist, LibmsiDatabase **pdb)
{
    IStorage *stg = NULL;
    HRESULT r;
    LibmsiDatabase *db = NULL;
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    WCHAR *szwDBPath;
    const char *szMode;
    STATSTG stat;
    bool created = false, patch = false, need_init = false;
    char path[MAX_PATH];

    TRACE("%s %p\n",debugstr_a(szDBPath),szPersist );

    if( !pdb )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if (IS_INTMSIDBOPEN(szPersist - LIBMSI_DB_OPEN_PATCHFILE))
    {
        TRACE("Database is a patch\n");
        szPersist -= LIBMSI_DB_OPEN_PATCHFILE;
        patch = true;
    }

    szMode = szPersist;
    if( !IS_INTMSIDBOPEN(szPersist) )
    {
        if (!CopyFileA( szDBPath, szPersist, false ))
            return LIBMSI_RESULT_OPEN_FAILED;

        szDBPath = szPersist;
        szPersist = LIBMSI_DB_OPEN_TRANSACT;
        created = true;
    }

    szwDBPath = strdupAtoW(szDBPath);
    if( szPersist == LIBMSI_DB_OPEN_READONLY )
    {
        r = StgOpenStorage( szwDBPath, NULL,
              STGM_DIRECT|STGM_READ|STGM_SHARE_DENY_WRITE, NULL, 0, &stg);
    }
    else if( szPersist == LIBMSI_DB_OPEN_CREATE )
    {
        r = StgCreateDocfile( szwDBPath,
              STGM_CREATE|STGM_TRANSACTED|STGM_READWRITE|STGM_SHARE_EXCLUSIVE, 0, &stg );

        if ( SUCCEEDED(r) )
            r = IStorage_SetClass( stg, patch ? &CLSID_MsiPatch : &CLSID_MsiDatabase );

        need_init = true;
        created = true;
    }
    else if( szPersist == LIBMSI_DB_OPEN_TRANSACT )
    {
        r = StgOpenStorage( szwDBPath, NULL,
              STGM_TRANSACTED|STGM_READWRITE|STGM_SHARE_DENY_WRITE, NULL, 0, &stg);
    }
    else
    {
        ERR("unknown flag %p\n",szPersist);
        return LIBMSI_RESULT_INVALID_PARAMETER;
    }
    msi_free(szwDBPath);

    if( FAILED( r ) || !stg )
    {
        WARN("open failed r = %08x for %s\n", r, debugstr_a(szDBPath));
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    r = IStorage_Stat( stg, &stat, STATFLAG_NONAME );
    if( FAILED( r ) )
    {
        FIXME("Failed to stat storage\n");
        goto end;
    }

    if ( !IsEqualGUID( &stat.clsid, &CLSID_MsiDatabase ) &&
         !IsEqualGUID( &stat.clsid, &CLSID_MsiPatch ) &&
         !IsEqualGUID( &stat.clsid, &CLSID_MsiTransform ) )
    {
        ERR("storage GUID is not a MSI database GUID %s\n",
             debugstr_guid(&stat.clsid) );
        goto end;
    }

    if ( patch && !IsEqualGUID( &stat.clsid, &CLSID_MsiPatch ) )
    {
        ERR("storage GUID is not the MSI patch GUID %s\n",
             debugstr_guid(&stat.clsid) );
        ret = LIBMSI_RESULT_OPEN_FAILED;
        goto end;
    }

    db = alloc_msiobject( sizeof (LibmsiDatabase), _libmsi_database_destroy );
    if( !db )
    {
        FIXME("Failed to allocate a handle\n");
        goto end;
    }

    if (!strchr( szDBPath, '\\' ))
    {
        getcwd( path, MAX_PATH );
        strcat( path, "\\" );
        strcat( path, szDBPath );
    }
    else
        strcpy( path, szDBPath );

    db->path = strdup( path );
    db->media_transform_offset = MSI_INITIAL_MEDIA_TRANSFORM_OFFSET;
    db->media_transform_disk_id = MSI_INITIAL_MEDIA_TRANSFORM_DISKID;

    if( TRACE_ON( msi ) )
        enum_stream_names( stg );

    db->infile = stg;
    IStorage_AddRef( db->infile );
    db->outfile = stg;
    IStorage_AddRef( db->outfile );

    db->mode = szMode;
    if (created)
        db->deletefile = strdup( szDBPath );
    list_init( &db->tables );
    list_init( &db->transforms );
    list_init( &db->streams );
    list_init( &db->storages );

    if (need_init) {
        r = db_initialize( db );

        if (FAILED( r ))
        {
            ret = LIBMSI_RESULT_FUNCTION_FAILED;
            goto end;
        }
    }

    cache_infile_structure( db );
    db->strings = msi_load_string_table( db->infile, &db->bytes_per_strref );
    if( !db->strings )
        goto end;

    ret = LIBMSI_RESULT_SUCCESS;

    msiobj_addref( &db->hdr );
    *pdb = db;

end:
    if( db )
        msiobj_release( &db->hdr );
    if( stg )
        IStorage_Release( stg );

    return ret;
}

static WCHAR *msi_read_text_archive(const char *path, unsigned *len)
{
    int fd;
    struct stat st;
    char *data = NULL;
    WCHAR *wdata = NULL;
    ssize_t nread;

    /* TODO g_file_get_contents */
    fd = open( path, O_RDONLY | O_BINARY);
    if (fd == -1)
        return NULL;

    fstat (fd, &st);
    if (!(data = msi_alloc( st.st_size ))) goto done;

    nread = read(fd, data, st.st_size);
    if (nread != st.st_size) goto done;

    while (!data[st.st_size - 1]) st.st_size--;
    *len = MultiByteToWideChar( CP_ACP, 0, data, st.st_size, NULL, 0 );
    if ((wdata = msi_alloc( (*len + 1) * sizeof(WCHAR) )))
    {
        MultiByteToWideChar( CP_ACP, 0, data, st.st_size, wdata, *len );
        wdata[*len] = 0;
    }

done:
    close( fd );
    msi_free( data );
    return wdata;
}

static void msi_parse_line(WCHAR **line, WCHAR ***entries, unsigned *num_entries, unsigned *len)
{
    WCHAR *ptr = *line;
    WCHAR *save;
    unsigned i, count = 1, chars_left = *len;

    *entries = NULL;

    /* stay on this line */
    while (chars_left && *ptr != '\n')
    {
        /* entries are separated by tabs */
        if (*ptr == '\t')
            count++;

        ptr++;
        chars_left--;
    }

    *entries = msi_alloc(count * sizeof(WCHAR *));
    if (!*entries)
        return;

    /* store pointers into the data */
    chars_left = *len;
    for (i = 0, ptr = *line; i < count; i++)
    {
        while (chars_left && *ptr == '\r')
        {
            ptr++;
            chars_left--;
        }
        save = ptr;

        while (chars_left && *ptr != '\t' && *ptr != '\n' && *ptr != '\r')
        {
            if (!*ptr) *ptr = '\n'; /* convert embedded nulls to \n */
            if (ptr > *line && *ptr == '\x19' && *(ptr - 1) == '\x11')
            {
                *ptr = '\n';
                *(ptr - 1) = '\r';
            }
            ptr++;
            chars_left--;
        }

        /* NULL-separate the data */
        if (*ptr == '\n' || *ptr == '\r')
        {
            while (chars_left && (*ptr == '\n' || *ptr == '\r'))
            {
                *(ptr++) = 0;
                chars_left--;
            }
        }
        else if (*ptr)
        {
            *(ptr++) = 0;
            chars_left--;
        }
        (*entries)[i] = save;
    }

    /* move to the next line if there's more, else EOF */
    *line = ptr;
    *len = chars_left;
    if (num_entries)
        *num_entries = count;
}

static WCHAR *msi_build_createsql_prelude(WCHAR *table)
{
    WCHAR *prelude;
    unsigned size;

    static const WCHAR create_fmt[] = {'C','R','E','A','T','E',' ','T','A','B','L','E',' ','`','%','s','`',' ','(',' ',0};

    size = sizeof(create_fmt)/sizeof(create_fmt[0]) + strlenW(table) - 2;
    prelude = msi_alloc(size * sizeof(WCHAR));
    if (!prelude)
        return NULL;

    sprintfW(prelude, create_fmt, table);
    return prelude;
}

static WCHAR *msi_build_createsql_columns(WCHAR **columns_data, WCHAR **types, unsigned num_columns)
{
    WCHAR *columns;
    WCHAR *p;
    const WCHAR *type;
    unsigned sql_size = 1, i, len;
    WCHAR expanded[128], *ptr;
    WCHAR size[10], comma[2], extra[30];

    static const WCHAR column_fmt[] = {'`','%','s','`',' ','%','s','%','s','%','s','%','s',' ',0};
    static const WCHAR size_fmt[] = {'(','%','s',')',0};
    static const WCHAR type_char[] = {'C','H','A','R',0};
    static const WCHAR type_int[] = {'I','N','T',0};
    static const WCHAR type_long[] = {'L','O','N','G',0};
    static const WCHAR type_object[] = {'O','B','J','E','C','T',0};
    static const WCHAR type_notnull[] = {' ','N','O','T',' ','N','U','L','L',0};
    static const WCHAR localizable[] = {' ','L','O','C','A','L','I','Z','A','B','L','E',0};

    columns = msi_alloc_zero(sql_size * sizeof(WCHAR));
    if (!columns)
        return NULL;

    for (i = 0; i < num_columns; i++)
    {
        type = NULL;
        comma[1] = size[0] = extra[0] = '\0';

        if (i == num_columns - 1)
            comma[0] = '\0';
        else
            comma[0] = ',';

        ptr = &types[i][1];
        len = atolW(ptr);
        extra[0] = '\0';

        switch (types[i][0])
        {
            case 'l':
                strcpyW(extra, type_notnull);
                /* fall through */
            case 'L':
                strcatW(extra, localizable);
                type = type_char;
                sprintfW(size, size_fmt, ptr);
                break;
            case 's':
                strcpyW(extra, type_notnull);
                /* fall through */
            case 'S':
                type = type_char;
                sprintfW(size, size_fmt, ptr);
                break;
            case 'i':
                strcpyW(extra, type_notnull);
                /* fall through */
            case 'I':
                if (len <= 2)
                    type = type_int;
                else if (len == 4)
                    type = type_long;
                else
                {
                    WARN("invalid int width %u\n", len);
                    msi_free(columns);
                    return NULL;
                }
                break;
            case 'v':
                strcpyW(extra, type_notnull);
                /* fall through */
            case 'V':
                type = type_object;
                break;
            default:
                ERR("Unknown type: %c\n", types[i][0]);
                msi_free(columns);
                return NULL;
        }

        sprintfW(expanded, column_fmt, columns_data[i], type, size, extra, comma);
        sql_size += strlenW(expanded);

        p = msi_realloc(columns, sql_size * sizeof(WCHAR));
        if (!p)
        {
            msi_free(columns);
            return NULL;
        }
        columns = p;

        strcatW(columns, expanded);
    }

    return columns;
}

static WCHAR *msi_build_createsql_postlude(WCHAR **primary_keys, unsigned num_keys)
{
    WCHAR *postlude;
    WCHAR *keys;
    WCHAR *ptr;
    unsigned size, key_size, i;

    static const WCHAR key_fmt[] = {'`','%','s','`',',',' ',0};
    static const WCHAR postlude_fmt[] = {'P','R','I','M','A','R','Y',' ','K','E','Y',' ','%','s',')',0};

    for (i = 0, size = 1; i < num_keys; i++)
        size += strlenW(key_fmt) + strlenW(primary_keys[i]) - 2;

    keys = msi_alloc(size * sizeof(WCHAR));
    if (!keys)
        return NULL;

    for (i = 0, ptr = keys; i < num_keys; i++)
    {
        key_size = strlenW(key_fmt) + strlenW(primary_keys[i]) -2;
        sprintfW(ptr, key_fmt, primary_keys[i]);
        ptr += key_size;
    }

    /* remove final ', ' */
    *(ptr - 2) = '\0';

    size = strlenW(postlude_fmt) + size - 1;
    postlude = msi_alloc(size * sizeof(WCHAR));
    if (!postlude)
        goto done;

    sprintfW(postlude, postlude_fmt, keys);

done:
    msi_free(keys);
    return postlude;
}

static unsigned msi_add_table_to_db(LibmsiDatabase *db, WCHAR **columns, WCHAR **types, WCHAR **labels, unsigned num_labels, unsigned num_columns)
{
    unsigned r = LIBMSI_RESULT_OUTOFMEMORY;
    unsigned size;
    LibmsiQuery *view;
    WCHAR *create_sql = NULL;
    WCHAR *prelude;
    WCHAR *columns_sql;
    WCHAR *postlude;

    prelude = msi_build_createsql_prelude(labels[0]);
    columns_sql = msi_build_createsql_columns(columns, types, num_columns);
    postlude = msi_build_createsql_postlude(labels + 1, num_labels - 1); /* skip over table name */

    if (!prelude || !columns_sql || !postlude)
        goto done;

    size = strlenW(prelude) + strlenW(columns_sql) + strlenW(postlude) + 1;
    create_sql = msi_alloc(size * sizeof(WCHAR));
    if (!create_sql)
        goto done;

    strcpyW(create_sql, prelude);
    strcatW(create_sql, columns_sql);
    strcatW(create_sql, postlude);

    r = _libmsi_database_open_query( db, create_sql, &view );
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    r = _libmsi_query_execute(view, NULL);
    libmsi_query_close(view);
    msiobj_release(&view->hdr);

done:
    msi_free(prelude);
    msi_free(columns_sql);
    msi_free(postlude);
    msi_free(create_sql);
    return r;
}

static char *msi_import_stream_filename(const char *path, const WCHAR *name)
{
    unsigned len;
    char *ascii_name = strdupWtoA(name);
    char *fullname;
    char *ptr;

    len = strlen(path) + strlen(ascii_name) + 1;
    fullname = msi_alloc(len);
    if (!fullname)
       return NULL;

    strcpy( fullname, path );

    /* chop off extension from path */
    ptr = strrchr(fullname, '.');
    if (!ptr)
    {
        msi_free (fullname);
        return NULL;
    }
    *ptr++ = '\\';
    strcpy( ptr, ascii_name );
    msi_free( ascii_name );
    return fullname;
}

static unsigned construct_record(unsigned num_columns, WCHAR **types,
                             WCHAR **data, const char *path, LibmsiRecord **rec)
{
    unsigned i;

    *rec = libmsi_record_create(num_columns);
    if (!*rec)
        return LIBMSI_RESULT_OUTOFMEMORY;

    for (i = 0; i < num_columns; i++)
    {
        switch (types[i][0])
        {
            case 'L': case 'l': case 'S': case 's':
                _libmsi_record_set_stringW(*rec, i + 1, data[i]);
                break;
            case 'I': case 'i':
                if (*data[i])
                    libmsi_record_set_int(*rec, i + 1, atoiW(data[i]));
                break;
            case 'V': case 'v':
                if (*data[i])
                {
                    unsigned r;
                    char *file = msi_import_stream_filename(path, data[i]);
                    if (!file)
                        return LIBMSI_RESULT_FUNCTION_FAILED;

                    r = _libmsi_record_load_stream_from_file(*rec, i + 1, file);
                    msi_free (file);
                    if (r != LIBMSI_RESULT_SUCCESS)
                        return LIBMSI_RESULT_FUNCTION_FAILED;
                }
                break;
            default:
                ERR("Unhandled column type: %c\n", types[i][0]);
                msiobj_release(&(*rec)->hdr);
                return LIBMSI_RESULT_FUNCTION_FAILED;
        }
    }

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned msi_add_records_to_table(LibmsiDatabase *db, WCHAR **columns, WCHAR **types,
                                     WCHAR **labels, WCHAR ***records,
                                     int num_columns, int num_records,
                                     const char *path)
{
    unsigned r, num_rows, num_cols;
    int i;
    LibmsiView *view;
    LibmsiRecord *rec;

    r = table_view_create(db, labels[0], &view);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = view->ops->get_dimensions( view, &num_rows, &num_cols );
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    while (num_rows > 0)
    {
        r = view->ops->delete_row(view, --num_rows);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;
    }

    for (i = 0; i < num_records; i++)
    {
        r = construct_record(num_columns, types, records[i], path, &rec);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;

        r = view->ops->insert_row(view, rec, -1, false);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            msiobj_release(&rec->hdr);
            goto done;
        }

        msiobj_release(&rec->hdr);
    }

done:
    msiobj_release(&view->hdr);
    return r;
}

static unsigned _libmsi_database_import(LibmsiDatabase *db, const char *folder, const char *file)
{
    unsigned r = LIBMSI_RESULT_OUTOFMEMORY;
    unsigned len, i;
    unsigned num_labels, num_types;
    unsigned num_columns, num_records = 0;
    char *path;
    WCHAR **columns;
    WCHAR **types;
    WCHAR **labels;
    WCHAR *ptr;
    WCHAR *data;
    WCHAR ***records = NULL;
    WCHAR ***temp_records;

    static const WCHAR suminfo[] =
        {'_','S','u','m','m','a','r','y','I','n','f','o','r','m','a','t','i','o','n',0};
    static const WCHAR forcecodepage[] =
        {'_','F','o','r','c','e','C','o','d','e','p','a','g','e',0};

    TRACE("%p %s %s\n", db, debugstr_a(folder), debugstr_a(file) );

    if( folder == NULL || file == NULL )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    len = strlen(folder) + 1 + strlen(file) + 1;
    path = msi_alloc( len );
    if (!path)
        return LIBMSI_RESULT_OUTOFMEMORY;

    strcpy( path, folder );
    strcat( path, "\\" );
    strcat( path, file );

    data = msi_read_text_archive( path, &len );
    if (!data)
        goto done;

    ptr = data;
    msi_parse_line( &ptr, &columns, &num_columns, &len );
    msi_parse_line( &ptr, &types, &num_types, &len );
    msi_parse_line( &ptr, &labels, &num_labels, &len );

    if (num_columns == 1 && !columns[0][0] && num_labels == 1 && !labels[0][0] &&
        num_types == 2 && !strcmpW( types[1], forcecodepage ))
    {
        r = msi_set_string_table_codepage( db->strings, atoiW( types[0] ) );
        goto done;
    }

    if (num_columns != num_types)
    {
        r = LIBMSI_RESULT_FUNCTION_FAILED;
        goto done;
    }

    records = msi_alloc(sizeof(WCHAR **));
    if (!records)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        goto done;
    }

    /* read in the table records */
    while (len)
    {
        msi_parse_line( &ptr, &records[num_records], NULL, &len );

        num_records++;
        temp_records = msi_realloc(records, (num_records + 1) * sizeof(WCHAR **));
        if (!temp_records)
        {
            r = LIBMSI_RESULT_OUTOFMEMORY;
            goto done;
        }
        records = temp_records;
    }

    if (!strcmpW(labels[0], suminfo))
    {
        r = msi_add_suminfo( db, records, num_records, num_columns );
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            r = LIBMSI_RESULT_FUNCTION_FAILED;
            goto done;
        }
    }
    else
    {
        if (!table_view_exists(db, labels[0]))
        {
            r = msi_add_table_to_db( db, columns, types, labels, num_labels, num_columns );
            if (r != LIBMSI_RESULT_SUCCESS)
            {
                r = LIBMSI_RESULT_FUNCTION_FAILED;
                goto done;
            }
        }

        r = msi_add_records_to_table( db, columns, types, labels, records, num_columns, num_records, path );
    }

done:
    msi_free(path);
    msi_free(data);
    msi_free(columns);
    msi_free(types);
    msi_free(labels);

    for (i = 0; i < num_records; i++)
        msi_free(records[i]);

    msi_free(records);

    return r;
}

LibmsiResult libmsi_database_import(LibmsiDatabase *db, const char *szFolder, const char *szFilename)
{
    unsigned r;

    TRACE("%x %s %s\n",db,debugstr_a(szFolder), debugstr_a(szFilename));

    if( !db )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &db->hdr );
    r = _libmsi_database_import( db, szFolder, szFilename );
    msiobj_release( &db->hdr );
    return r;
}

static unsigned msi_export_record( int fd, LibmsiRecord *row, unsigned start )
{
    unsigned i, count, len, r = LIBMSI_RESULT_SUCCESS;
    const char *sep;
    char *buffer;
    unsigned sz;

    len = 0x100;
    buffer = msi_alloc( len );
    if ( !buffer )
        return LIBMSI_RESULT_OUTOFMEMORY;

    count = libmsi_record_get_field_count( row );
    for ( i=start; i<=count; i++ )
    {
        sz = len;
        r = libmsi_record_get_string( row, i, buffer, &sz );
        if (r == LIBMSI_RESULT_MORE_DATA)
        {
            char *p = msi_realloc( buffer, sz + 1 );
            if (!p)
                break;
            len = sz + 1;
            buffer = p;
        }
        sz = len;
        r = libmsi_record_get_string( row, i, buffer, &sz );
        if (r != LIBMSI_RESULT_SUCCESS)
            break;

        /* TODO full_write */
        if (write( fd, buffer, sz ) != sz)
        {
            r = LIBMSI_RESULT_FUNCTION_FAILED;
            break;
        }

        sep = (i < count) ? "\t" : "\r\n";
        if (write( fd, sep, strlen(sep) ) != strlen(sep))
        {
            r = LIBMSI_RESULT_FUNCTION_FAILED;
            break;
        }
    }
    msi_free( buffer );
    return r;
}

static unsigned msi_export_row( LibmsiRecord *row, void *arg )
{
    return msi_export_record( (intptr_t) arg, row, 1 );
}

static unsigned msi_export_forcecodepage( int fd, unsigned codepage )
{
    static const char fmt[] = "\r\n\r\n%u\t_ForceCodepage\r\n";
    char data[sizeof(fmt) + 10];
    unsigned sz;

    sprintf( data, fmt, codepage );

    sz = strlen(data) + 1;
    if (write( fd, data, sz ) != sz)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned _libmsi_database_export( LibmsiDatabase *db, const WCHAR *table,
               int fd)
{
    static const WCHAR query[] = {
        's','e','l','e','c','t',' ','*',' ','f','r','o','m',' ','%','s',0 };
    static const WCHAR forcecodepage[] = {
        '_','F','o','r','c','e','C','o','d','e','p','a','g','e',0 };
    LibmsiRecord *rec = NULL;
    LibmsiQuery *view = NULL;
    unsigned r;

    TRACE("%p %s %d\n", db, debugstr_w(table), fd );

    if (!strcmpW( table, forcecodepage ))
    {
        unsigned codepage = msi_get_string_table_codepage( db->strings );
        r = msi_export_forcecodepage( fd, codepage );
        goto done;
    }

    r = _libmsi_query_open( db, &view, query, table );
    if (r == LIBMSI_RESULT_SUCCESS)
    {
        /* write out row 1, the column names */
        r = _libmsi_query_get_column_info(view, LIBMSI_COL_INFO_NAMES, &rec);
        if (r == LIBMSI_RESULT_SUCCESS)
        {
            msi_export_record( fd, rec, 1 );
            msiobj_release( &rec->hdr );
        }

        /* write out row 2, the column types */
        r = _libmsi_query_get_column_info(view, LIBMSI_COL_INFO_TYPES, &rec);
        if (r == LIBMSI_RESULT_SUCCESS)
        {
            msi_export_record( fd, rec, 1 );
            msiobj_release( &rec->hdr );
        }

        /* write out row 3, the table name + keys */
        r = _libmsi_database_get_primary_keys( db, table, &rec );
        if (r == LIBMSI_RESULT_SUCCESS)
        {
            _libmsi_record_set_stringW( rec, 0, table );
            msi_export_record( fd, rec, 0 );
            msiobj_release( &rec->hdr );
        }

        /* write out row 4 onwards, the data */
        r = _libmsi_query_iterate_records( view, 0, msi_export_row, (void *)(intptr_t) fd );
        msiobj_release( &view->hdr );
    }

done:
    return r;
}

/***********************************************************************
 * MsiExportDatabaseW        [MSI.@]
 *
 * Writes a file containing the table data as tab separated ASCII.
 *
 * The format is as follows:
 *
 * row1 : colname1 <tab> colname2 <tab> .... colnameN <cr> <lf>
 * row2 : coltype1 <tab> coltype2 <tab> .... coltypeN <cr> <lf>
 * row3 : tablename <tab> key1 <tab> key2 <tab> ... keyM <cr> <lf>
 *
 * Followed by the data, starting at row 1 with one row per line
 *
 * row4 : data <tab> data <tab> data <tab> ... data <cr> <lf>
 */
LibmsiResult libmsi_database_export( LibmsiDatabase *db, const char *szTable,
               int fd )
{
    WCHAR *path = NULL;
    WCHAR *file = NULL;
    WCHAR *table = NULL;
    unsigned r = LIBMSI_RESULT_OUTOFMEMORY;

    TRACE("%x %s %d\n", db, debugstr_a(szTable), fd);

    if( szTable )
    {
        table = strdupAtoW( szTable );
        if( !table )
            goto end;
    }

    if( !db )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref ( &db->hdr );
    r = _libmsi_database_export( db, table, fd );
    msiobj_release( &db->hdr );

end:
    msi_free( table );
    msi_free( path );
    msi_free( file );

    return r;
}

typedef struct _tagMERGETABLE
{
    struct list entry;
    struct list rows;
    WCHAR *name;
    unsigned numconflicts;
    WCHAR **columns;
    unsigned numcolumns;
    WCHAR **types;
    unsigned numtypes;
    WCHAR **labels;
    unsigned numlabels;
} MERGETABLE;

typedef struct _tagMERGEROW
{
    struct list entry;
    LibmsiRecord *data;
} MERGEROW;

typedef struct _tagMERGEDATA
{
    LibmsiDatabase *db;
    LibmsiDatabase *merge;
    MERGETABLE *curtable;
    LibmsiQuery *curview;
    struct list *tabledata;
} MERGEDATA;

static bool merge_type_match(const WCHAR *type1, const WCHAR *type2)
{
    if (((type1[0] == 'l') || (type1[0] == 's')) &&
        ((type2[0] == 'l') || (type2[0] == 's')))
        return true;

    if (((type1[0] == 'L') || (type1[0] == 'S')) &&
        ((type2[0] == 'L') || (type2[0] == 'S')))
        return true;

    return !strcmpW( type1, type2 );
}

static unsigned merge_verify_colnames(LibmsiQuery *dbview, LibmsiQuery *mergeview)
{
    LibmsiRecord *dbrec, *mergerec;
    unsigned r, i, count;

    r = _libmsi_query_get_column_info(dbview, LIBMSI_COL_INFO_NAMES, &dbrec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = _libmsi_query_get_column_info(mergeview, LIBMSI_COL_INFO_NAMES, &mergerec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    count = libmsi_record_get_field_count(dbrec);
    for (i = 1; i <= count; i++)
    {
        if (!_libmsi_record_get_string_raw(mergerec, i))
            break;

        if (strcmpW( _libmsi_record_get_string_raw( dbrec, i ), _libmsi_record_get_string_raw( mergerec, i ) ))
        {
            r = LIBMSI_RESULT_DATATYPE_MISMATCH;
            goto done;
        }
    }

    msiobj_release(&dbrec->hdr);
    msiobj_release(&mergerec->hdr);
    dbrec = mergerec = NULL;

    r = _libmsi_query_get_column_info(dbview, LIBMSI_COL_INFO_TYPES, &dbrec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = _libmsi_query_get_column_info(mergeview, LIBMSI_COL_INFO_TYPES, &mergerec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    count = libmsi_record_get_field_count(dbrec);
    for (i = 1; i <= count; i++)
    {
        if (!_libmsi_record_get_string_raw(mergerec, i))
            break;

        if (!merge_type_match(_libmsi_record_get_string_raw(dbrec, i),
                     _libmsi_record_get_string_raw(mergerec, i)))
        {
            r = LIBMSI_RESULT_DATATYPE_MISMATCH;
            break;
        }
    }

done:
    msiobj_release(&dbrec->hdr);
    msiobj_release(&mergerec->hdr);

    return r;
}

static unsigned merge_verify_primary_keys(LibmsiDatabase *db, LibmsiDatabase *mergedb,
                                      const WCHAR *table)
{
    LibmsiRecord *dbrec, *mergerec = NULL;
    unsigned r, i, count;

    r = _libmsi_database_get_primary_keys(db, table, &dbrec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = _libmsi_database_get_primary_keys(mergedb, table, &mergerec);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    count = libmsi_record_get_field_count(dbrec);
    if (count != libmsi_record_get_field_count(mergerec))
    {
        r = LIBMSI_RESULT_DATATYPE_MISMATCH;
        goto done;
    }

    for (i = 1; i <= count; i++)
    {
        if (strcmpW( _libmsi_record_get_string_raw( dbrec, i ), _libmsi_record_get_string_raw( mergerec, i ) ))
        {
            r = LIBMSI_RESULT_DATATYPE_MISMATCH;
            goto done;
        }
    }

done:
    msiobj_release(&dbrec->hdr);
    msiobj_release(&mergerec->hdr);

    return r;
}

static WCHAR *get_key_value(LibmsiQuery *view, const WCHAR *key, LibmsiRecord *rec)
{
    LibmsiRecord *colnames;
    WCHAR *str;
    WCHAR *val;
    unsigned r, i = 0, sz = 0;
    int cmp;

    r = _libmsi_query_get_column_info(view, LIBMSI_COL_INFO_NAMES, &colnames);
    if (r != LIBMSI_RESULT_SUCCESS)
        return NULL;

    do
    {
        str = msi_dup_record_field(colnames, ++i);
        cmp = strcmpW( key, str );
        msi_free(str);
    } while (cmp);

    msiobj_release(&colnames->hdr);

    r = _libmsi_record_get_stringW(rec, i, NULL, &sz);
    if (r != LIBMSI_RESULT_SUCCESS)
        return NULL;
    sz++;

    if (_libmsi_record_get_string_raw(rec, i))  /* check record field is a string */
    {
        /* quote string record fields */
        const WCHAR szQuote[] = {'\'', 0};
        sz += 2;
        val = msi_alloc(sz*sizeof(WCHAR));
        if (!val)
            return NULL;

        strcpyW(val, szQuote);
        r = _libmsi_record_get_stringW(rec, i, val+1, &sz);
        strcpyW(val+1+sz, szQuote);
    }
    else
    {
        /* do not quote integer record fields */
        val = msi_alloc(sz*sizeof(WCHAR));
        if (!val)
            return NULL;

        r = _libmsi_record_get_stringW(rec, i, val, &sz);
    }

    if (r != LIBMSI_RESULT_SUCCESS)
    {
        ERR("failed to get string!\n");
        msi_free(val);
        return NULL;
    }

    return val;
}

static WCHAR *create_diff_row_query(LibmsiDatabase *merge, LibmsiQuery *view,
                                    WCHAR *table, LibmsiRecord *rec)
{
    WCHAR *query = NULL;
    WCHAR *clause = NULL;
    WCHAR *val;
    const WCHAR *setptr;
    const WCHAR *key;
    unsigned size, oldsize;
    LibmsiRecord *keys;
    unsigned r, i, count;

    static const WCHAR keyset[] = {
        '`','%','s','`',' ','=',' ','%','s',' ','A','N','D',' ',0};
    static const WCHAR lastkeyset[] = {
        '`','%','s','`',' ','=',' ','%','s',' ',0};
    static const WCHAR fmt[] = {'S','E','L','E','C','T',' ','*',' ',
        'F','R','O','M',' ','`','%','s','`',' ',
        'W','H','E','R','E',' ','%','s',0};

    r = _libmsi_database_get_primary_keys(merge, table, &keys);
    if (r != LIBMSI_RESULT_SUCCESS)
        return NULL;

    clause = msi_alloc_zero(sizeof(WCHAR));
    if (!clause)
        goto done;

    size = 1;
    count = libmsi_record_get_field_count(keys);
    for (i = 1; i <= count; i++)
    {
        key = _libmsi_record_get_string_raw(keys, i);
        val = get_key_value(view, key, rec);

        if (i == count)
            setptr = lastkeyset;
        else
            setptr = keyset;

        oldsize = size;
        size += strlenW(setptr) + strlenW(key) + strlenW(val) - 4;
        clause = msi_realloc(clause, size * sizeof (WCHAR));
        if (!clause)
        {
            msi_free(val);
            goto done;
        }

        sprintfW(clause + oldsize - 1, setptr, key, val);
        msi_free(val);
    }

    size = strlenW(fmt) + strlenW(table) + strlenW(clause) + 1;
    query = msi_alloc(size * sizeof(WCHAR));
    if (!query)
        goto done;

    sprintfW(query, fmt, table, clause);

done:
    msi_free(clause);
    msiobj_release(&keys->hdr);
    return query;
}

static unsigned merge_diff_row(LibmsiRecord *rec, void *param)
{
    MERGEDATA *data = param;
    MERGETABLE *table = data->curtable;
    MERGEROW *mergerow;
    LibmsiQuery *dbview = NULL;
    LibmsiRecord *row = NULL;
    WCHAR *query = NULL;
    unsigned r = LIBMSI_RESULT_SUCCESS;

    if (table_view_exists(data->db, table->name))
    {
        query = create_diff_row_query(data->merge, data->curview, table->name, rec);
        if (!query)
            return LIBMSI_RESULT_OUTOFMEMORY;

        r = _libmsi_database_open_query(data->db, query, &dbview);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;

        r = _libmsi_query_execute(dbview, NULL);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;

        r = _libmsi_query_fetch(dbview, &row);
        if (r == LIBMSI_RESULT_SUCCESS && !_libmsi_record_compare(rec, row))
        {
            table->numconflicts++;
            goto done;
        }
        else if (r != LIBMSI_RESULT_NO_MORE_ITEMS)
            goto done;

        r = LIBMSI_RESULT_SUCCESS;
    }

    mergerow = msi_alloc(sizeof(MERGEROW));
    if (!mergerow)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        goto done;
    }

    mergerow->data = _libmsi_record_clone(rec);
    if (!mergerow->data)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        msi_free(mergerow);
        goto done;
    }

    list_add_tail(&table->rows, &mergerow->entry);

done:
    msi_free(query);
    msiobj_release(&row->hdr);
    msiobj_release(&dbview->hdr);
    return r;
}

static unsigned msi_get_table_labels(LibmsiDatabase *db, const WCHAR *table, WCHAR ***labels, unsigned *numlabels)
{
    unsigned r, i, count;
    LibmsiRecord *prec = NULL;

    r = _libmsi_database_get_primary_keys(db, table, &prec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    count = libmsi_record_get_field_count(prec);
    *numlabels = count + 1;
    *labels = msi_alloc((*numlabels)*sizeof(WCHAR *));
    if (!*labels)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        goto end;
    }

    (*labels)[0] = strdupW(table);
    for (i=1; i<=count; i++ )
    {
        (*labels)[i] = strdupW(_libmsi_record_get_string_raw(prec, i));
    }

end:
    msiobj_release( &prec->hdr );
    return r;
}

static unsigned msi_get_query_columns(LibmsiQuery *query, WCHAR ***columns, unsigned *numcolumns)
{
    unsigned r, i, count;
    LibmsiRecord *prec = NULL;

    r = _libmsi_query_get_column_info(query, LIBMSI_COL_INFO_NAMES, &prec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    count = libmsi_record_get_field_count(prec);
    *columns = msi_alloc(count*sizeof(WCHAR *));
    if (!*columns)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        goto end;
    }

    for (i=1; i<=count; i++ )
    {
        (*columns)[i-1] = strdupW(_libmsi_record_get_string_raw(prec, i));
    }

    *numcolumns = count;

end:
    msiobj_release( &prec->hdr );
    return r;
}

static unsigned msi_get_query_types(LibmsiQuery *query, WCHAR ***types, unsigned *numtypes)
{
    unsigned r, i, count;
    LibmsiRecord *prec = NULL;

    r = _libmsi_query_get_column_info(query, LIBMSI_COL_INFO_TYPES, &prec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    count = libmsi_record_get_field_count(prec);
    *types = msi_alloc(count*sizeof(WCHAR *));
    if (!*types)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        goto end;
    }

    *numtypes = count;
    for (i=1; i<=count; i++ )
    {
        (*types)[i-1] = strdupW(_libmsi_record_get_string_raw(prec, i));
    }

end:
    msiobj_release( &prec->hdr );
    return r;
}

static void merge_free_rows(MERGETABLE *table)
{
    struct list *item, *cursor;

    LIST_FOR_EACH_SAFE(item, cursor, &table->rows)
    {
        MERGEROW *row = LIST_ENTRY(item, MERGEROW, entry);

        list_remove(&row->entry);
        msiobj_release(&row->data->hdr);
        msi_free(row);
    }
}

static void free_merge_table(MERGETABLE *table)
{
    unsigned i;

    if (table->labels != NULL)
    {
        for (i = 0; i < table->numlabels; i++)
            msi_free(table->labels[i]);

        msi_free(table->labels);
    }

    if (table->columns != NULL)
    {
        for (i = 0; i < table->numcolumns; i++)
            msi_free(table->columns[i]);

        msi_free(table->columns);
    }

    if (table->types != NULL)
    {
        for (i = 0; i < table->numtypes; i++)
            msi_free(table->types[i]);

        msi_free(table->types);
    }

    msi_free(table->name);
    merge_free_rows(table);

    msi_free(table);
}

static unsigned msi_get_merge_table (LibmsiDatabase *db, const WCHAR *name, MERGETABLE **ptable)
{
    unsigned r;
    MERGETABLE *table;
    LibmsiQuery *mergeview = NULL;

    static const WCHAR query[] = {'S','E','L','E','C','T',' ','*',' ',
        'F','R','O','M',' ','`','%','s','`',0};

    table = msi_alloc_zero(sizeof(MERGETABLE));
    if (!table)
    {
       *ptable = NULL;
       return LIBMSI_RESULT_OUTOFMEMORY;
    }

    r = msi_get_table_labels(db, name, &table->labels, &table->numlabels);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto err;

    r = _libmsi_query_open(db, &mergeview, query, name);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto err;

    r = msi_get_query_columns(mergeview, &table->columns, &table->numcolumns);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto err;

    r = msi_get_query_types(mergeview, &table->types, &table->numtypes);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto err;

    list_init(&table->rows);

    table->name = strdupW(name);
    table->numconflicts = 0;

    msiobj_release(&mergeview->hdr);
    *ptable = table;
    return LIBMSI_RESULT_SUCCESS;

err:
    msiobj_release(&mergeview->hdr);
    free_merge_table(table);
    *ptable = NULL;
    return r;
}

static unsigned merge_diff_tables(LibmsiRecord *rec, void *param)
{
    MERGEDATA *data = param;
    MERGETABLE *table;
    LibmsiQuery *dbview = NULL;
    LibmsiQuery *mergeview = NULL;
    const WCHAR *name;
    unsigned r;

    static const WCHAR query[] = {'S','E','L','E','C','T',' ','*',' ',
        'F','R','O','M',' ','`','%','s','`',0};

    name = _libmsi_record_get_string_raw(rec, 1);

    r = _libmsi_query_open(data->merge, &mergeview, query, name);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    if (table_view_exists(data->db, name))
    {
        r = _libmsi_query_open(data->db, &dbview, query, name);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;

        r = merge_verify_colnames(dbview, mergeview);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;

        r = merge_verify_primary_keys(data->db, data->merge, name);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;
    }

    r = msi_get_merge_table(data->merge, name, &table);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    data->curtable = table;
    data->curview = mergeview;
    r = _libmsi_query_iterate_records(mergeview, NULL, merge_diff_row, data);
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        free_merge_table(table);
        goto done;
    }

    list_add_tail(data->tabledata, &table->entry);

done:
    msiobj_release(&dbview->hdr);
    msiobj_release(&mergeview->hdr);
    return r;
}

static unsigned gather_merge_data(LibmsiDatabase *db, LibmsiDatabase *merge,
                              struct list *tabledata)
{
    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
        '`','_','T','a','b','l','e','s','`',0};
    LibmsiQuery *view;
    MERGEDATA data;
    unsigned r;

    r = _libmsi_database_open_query(merge, query, &view);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    data.db = db;
    data.merge = merge;
    data.tabledata = tabledata;
    r = _libmsi_query_iterate_records(view, NULL, merge_diff_tables, &data);
    msiobj_release(&view->hdr);
    return r;
}

static unsigned merge_table(LibmsiDatabase *db, MERGETABLE *table)
{
    unsigned r;
    MERGEROW *row;
    LibmsiView *tv;

    if (!table_view_exists(db, table->name))
    {
        r = msi_add_table_to_db(db, table->columns, table->types,
               table->labels, table->numlabels, table->numcolumns);
        if (r != LIBMSI_RESULT_SUCCESS)
           return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    LIST_FOR_EACH_ENTRY(row, &table->rows, MERGEROW, entry)
    {
        r = table_view_create(db, table->name, &tv);
        if (r != LIBMSI_RESULT_SUCCESS)
            return r;

        r = tv->ops->insert_row(tv, row->data, -1, false);
        tv->ops->delete(tv);

        if (r != LIBMSI_RESULT_SUCCESS)
            return r;
    }

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned update_merge_errors(LibmsiDatabase *db, const WCHAR *error,
                                WCHAR *table, unsigned numconflicts)
{
    unsigned r;
    LibmsiQuery *view;

    static const WCHAR create[] = {
        'C','R','E','A','T','E',' ','T','A','B','L','E',' ',
        '`','%','s','`',' ','(','`','T','a','b','l','e','`',' ',
        'C','H','A','R','(','2','5','5',')',' ','N','O','T',' ',
        'N','U','L','L',',',' ','`','N','u','m','R','o','w','M','e','r','g','e',
        'C','o','n','f','l','i','c','t','s','`',' ','S','H','O','R','T',' ',
        'N','O','T',' ','N','U','L','L',' ','P','R','I','M','A','R','Y',' ',
        'K','E','Y',' ','`','T','a','b','l','e','`',')',0};
    static const WCHAR insert[] = {
        'I','N','S','E','R','T',' ','I','N','T','O',' ',
        '`','%','s','`',' ','(','`','T','a','b','l','e','`',',',' ',
        '`','N','u','m','R','o','w','M','e','r','g','e',
        'C','o','n','f','l','i','c','t','s','`',')',' ','V','A','L','U','E','S',
        ' ','(','\'','%','s','\'',',',' ','%','d',')',0};

    if (!table_view_exists(db, error))
    {
        r = _libmsi_query_open(db, &view, create, error);
        if (r != LIBMSI_RESULT_SUCCESS)
            return r;

        r = _libmsi_query_execute(view, NULL);
        msiobj_release(&view->hdr);
        if (r != LIBMSI_RESULT_SUCCESS)
            return r;
    }

    r = _libmsi_query_open(db, &view, insert, error, table, numconflicts);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = _libmsi_query_execute(view, NULL);
    msiobj_release(&view->hdr);
    return r;
}

LibmsiResult libmsi_database_merge(LibmsiDatabase *db, LibmsiDatabase *merge,
                              const char *szTableName)
{
    struct list tabledata = LIST_INIT(tabledata);
    struct list *item, *cursor;
    WCHAR *szwTableName;
    MERGETABLE *table;
    bool conflicts;
    unsigned r;

    TRACE("(%d, %d, %s)\n", db, merge,
          debugstr_a(szTableName));

    if (szTableName && !*szTableName)
        return LIBMSI_RESULT_INVALID_TABLE;

    if (!db || !merge)
        return LIBMSI_RESULT_INVALID_HANDLE;

    szwTableName = strdupAtoW(szTableName);
    msiobj_addref( &db->hdr );
    msiobj_addref( &merge->hdr );
    r = gather_merge_data(db, merge, &tabledata);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    conflicts = false;
    LIST_FOR_EACH_ENTRY(table, &tabledata, MERGETABLE, entry)
    {
        if (table->numconflicts)
        {
            conflicts = true;

            r = update_merge_errors(db, szwTableName, table->name,
                                    table->numconflicts);
            if (r != LIBMSI_RESULT_SUCCESS)
                break;
        }
        else
        {
            r = merge_table(db, table);
            if (r != LIBMSI_RESULT_SUCCESS)
                break;
        }
    }

    LIST_FOR_EACH_SAFE(item, cursor, &tabledata)
    {
        MERGETABLE *table = LIST_ENTRY(item, MERGETABLE, entry);
        list_remove(&table->entry);
        free_merge_table(table);
    }

    if (conflicts)
        r = LIBMSI_RESULT_FUNCTION_FAILED;

done:
    msiobj_release(&db->hdr);
    msiobj_release(&merge->hdr);
    return r;
}

LibmsiDBState libmsi_database_get_state( LibmsiDatabase *db )
{
    LibmsiDBState ret = LIBMSI_DB_STATE_READ;

    TRACE("%d\n", db);

    if( !db )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &db->hdr );
    if (db->mode != LIBMSI_DB_OPEN_READONLY )
        ret = LIBMSI_DB_STATE_WRITE;
    msiobj_release( &db->hdr );

    return ret;
}
