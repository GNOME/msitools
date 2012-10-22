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

#define COBJMACROS

#include "windef.h"
#include "winerror.h"
#include "wine/debug.h"
#include "fdi.h"
#include "msipriv.h"
#include "wingdi.h"
#include "winuser.h"
#include "winreg.h"
#include "shlwapi.h"
#include "objidl.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(msi);

/* from msvcrt/fcntl.h */
#define _O_RDONLY      0
#define _O_WRONLY      1
#define _O_RDWR        2
#define _O_ACCMODE     (_O_RDONLY|_O_WRONLY|_O_RDWR)
#define _O_APPEND      0x0008
#define _O_RANDOM      0x0010
#define _O_SEQUENTIAL  0x0020
#define _O_TEMPORARY   0x0040
#define _O_NOINHERIT   0x0080
#define _O_CREAT       0x0100
#define _O_TRUNC       0x0200
#define _O_EXCL        0x0400
#define _O_SHORT_LIVED 0x1000
#define _O_TEXT        0x4000
#define _O_BINARY      0x8000

static MSICABINETSTREAM *msi_get_cabinet_stream( MSIPACKAGE *package, UINT disk_id )
{
    MSICABINETSTREAM *cab;

    LIST_FOR_EACH_ENTRY( cab, &package->cabinet_streams, MSICABINETSTREAM, entry )
    {
        if (cab->disk_id == disk_id) return cab;
    }
    return NULL;
}

static void * CDECL cabinet_alloc(ULONG cb)
{
    return msi_alloc(cb);
}

static void CDECL cabinet_free(void *pv)
{
    msi_free(pv);
}

static INT_PTR CDECL cabinet_open(char *pszFile, int oflag, int pmode)
{
    HANDLE handle;
    DWORD dwAccess = 0;
    DWORD dwShareMode = 0;
    DWORD dwCreateDisposition = OPEN_EXISTING;

    switch (oflag & _O_ACCMODE)
    {
    case _O_RDONLY:
        dwAccess = GENERIC_READ;
        dwShareMode = FILE_SHARE_READ | FILE_SHARE_DELETE;
        break;
    case _O_WRONLY:
        dwAccess = GENERIC_WRITE;
        dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
        break;
    case _O_RDWR:
        dwAccess = GENERIC_READ | GENERIC_WRITE;
        dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
        break;
    }

    if ((oflag & (_O_CREAT | _O_EXCL)) == (_O_CREAT | _O_EXCL))
        dwCreateDisposition = CREATE_NEW;
    else if (oflag & _O_CREAT)
        dwCreateDisposition = CREATE_ALWAYS;

    handle = CreateFileA(pszFile, dwAccess, dwShareMode, NULL,
                         dwCreateDisposition, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return 0;

    return (INT_PTR)handle;
}

static UINT CDECL cabinet_read(INT_PTR hf, void *pv, UINT cb)
{
    HANDLE handle = (HANDLE)hf;
    DWORD read;

    if (ReadFile(handle, pv, cb, &read, NULL))
        return read;

    return 0;
}

static UINT CDECL cabinet_write(INT_PTR hf, void *pv, UINT cb)
{
    HANDLE handle = (HANDLE)hf;
    DWORD written;

    if (WriteFile(handle, pv, cb, &written, NULL))
        return written;

    return 0;
}

static int CDECL cabinet_close(INT_PTR hf)
{
    HANDLE handle = (HANDLE)hf;
    return CloseHandle(handle) ? 0 : -1;
}

static LONG CDECL cabinet_seek(INT_PTR hf, LONG dist, int seektype)
{
    HANDLE handle = (HANDLE)hf;
    /* flags are compatible and so are passed straight through */
    return SetFilePointer(handle, dist, NULL, seektype);
}

struct package_disk
{
    MSIPACKAGE *package;
    UINT        id;
};

static struct package_disk package_disk;

static INT_PTR CDECL cabinet_open_stream( char *pszFile, int oflag, int pmode )
{
    MSICABINETSTREAM *cab;
    IStream *stream;
    WCHAR *encoded;
    HRESULT hr;

    cab = msi_get_cabinet_stream( package_disk.package, package_disk.id );
    if (!cab)
    {
        WARN("failed to get cabinet stream\n");
        return 0;
    }
    if (!cab->stream[0] || !(encoded = encode_streamname( FALSE, cab->stream + 1 )))
    {
        WARN("failed to encode stream name\n");
        return 0;
    }
    if (msi_clone_open_stream( package_disk.package->db, cab->storage, encoded, &stream ) != ERROR_SUCCESS)
    {
        hr = IStorage_OpenStream( cab->storage, encoded, NULL, STGM_READ|STGM_SHARE_EXCLUSIVE, 0, &stream );
        if (FAILED(hr))
        {
            WARN("failed to open stream 0x%08x\n", hr);
            msi_free( encoded );
            return 0;
        }
    }
    msi_free( encoded );
    return (INT_PTR)stream;
}

static UINT CDECL cabinet_read_stream( INT_PTR hf, void *pv, UINT cb )
{
    IStream *stm = (IStream *)hf;
    DWORD read;
    HRESULT hr;

    hr = IStream_Read( stm, pv, cb, &read );
    if (hr == S_OK || hr == S_FALSE)
        return read;

    return 0;
}

static int CDECL cabinet_close_stream( INT_PTR hf )
{
    IStream *stm = (IStream *)hf;
    IStream_Release( stm );
    return 0;
}

static LONG CDECL cabinet_seek_stream( INT_PTR hf, LONG dist, int seektype )
{
    IStream *stm = (IStream *)hf;
    LARGE_INTEGER move;
    ULARGE_INTEGER newpos;
    HRESULT hr;

    move.QuadPart = dist;
    hr = IStream_Seek( stm, move, seektype, &newpos );
    if (SUCCEEDED(hr))
    {
        if (newpos.QuadPart <= MAXLONG) return newpos.QuadPart;
        ERR("Too big!\n");
    }
    return -1;
}

static UINT CDECL msi_media_get_disk_info(MSIPACKAGE *package, MSIMEDIAINFO *mi)
{
    MSIRECORD *row;

    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','*',' ', 'F','R','O','M',' ',
        '`','M','e','d','i','a','`',' ','W','H','E','R','E',' ',
        '`','D','i','s','k','I','d','`',' ','=',' ','%','i',0};

    row = MSI_QueryGetRecord(package->db, query, mi->disk_id);
    if (!row)
    {
        TRACE("Unable to query row\n");
        return ERROR_FUNCTION_FAILED;
    }

    mi->disk_prompt = strdupW(MSI_RecordGetString(row, 3));
    mi->cabinet = strdupW(MSI_RecordGetString(row, 4));
    mi->volume_label = strdupW(MSI_RecordGetString(row, 5));

    if (!mi->first_volume)
        mi->first_volume = strdupW(mi->volume_label);

    msiobj_release(&row->hdr);
    return ERROR_SUCCESS;
}

static INT_PTR cabinet_partial_file(FDINOTIFICATIONTYPE fdint,
                                    PFDINOTIFICATION pfdin)
{
    MSICABDATA *data = pfdin->pv;
    data->mi->is_continuous = FALSE;
    return 0;
}

static WCHAR *get_cabinet_filename(MSIMEDIAINFO *mi)
{
    int len;
    WCHAR *ret;

    len = strlenW(mi->sourcedir) + strlenW(mi->cabinet) + 1;
    if (!(ret = msi_alloc(len * sizeof(WCHAR)))) return NULL;
    strcpyW(ret, mi->sourcedir);
    strcatW(ret, mi->cabinet);
    return ret;
}

static INT_PTR cabinet_next_cabinet(FDINOTIFICATIONTYPE fdint,
                                    PFDINOTIFICATION pfdin)
{
    MSICABDATA *data = pfdin->pv;
    MSIMEDIAINFO *mi = data->mi;
    LPWSTR cabinet_file = NULL, cab = strdupAtoW(pfdin->psz1);
    INT_PTR res = -1;
    UINT rc;

    msi_free(mi->disk_prompt);
    msi_free(mi->cabinet);
    msi_free(mi->volume_label);
    mi->disk_prompt = NULL;
    mi->cabinet = NULL;
    mi->volume_label = NULL;

    mi->disk_id++;
    mi->is_continuous = TRUE;

    rc = msi_media_get_disk_info(data->package, mi);
    if (rc != ERROR_SUCCESS)
    {
        ERR("Failed to get next cabinet information: %d\n", rc);
        goto done;
    }

    if (strcmpiW( mi->cabinet, cab ))
    {
        ERR("Continuous cabinet does not match the next cabinet in the Media table\n");
        goto done;
    }

    if (!(cabinet_file = get_cabinet_filename(mi)))
        goto done;

    TRACE("Searching for %s\n", debugstr_w(cabinet_file));

    res = 0;
    if (GetFileAttributesW(cabinet_file) == INVALID_FILE_ATTRIBUTES)
        res = -1;

done:
    msi_free(cab);
    msi_free(cabinet_file);
    return res;
}

static INT_PTR cabinet_next_cabinet_stream( FDINOTIFICATIONTYPE fdint,
                                            PFDINOTIFICATION pfdin )
{
    MSICABDATA *data = pfdin->pv;
    MSIMEDIAINFO *mi = data->mi;
    UINT rc;

    msi_free( mi->disk_prompt );
    msi_free( mi->cabinet );
    msi_free( mi->volume_label );
    mi->disk_prompt = NULL;
    mi->cabinet = NULL;
    mi->volume_label = NULL;

    mi->disk_id++;
    mi->is_continuous = TRUE;

    rc = msi_media_get_disk_info( data->package, mi );
    if (rc != ERROR_SUCCESS)
    {
        ERR("Failed to get next cabinet information: %u\n", rc);
        return -1;
    }
    package_disk.id = mi->disk_id;

    TRACE("next cabinet is %s disk id %u\n", debugstr_w(mi->cabinet), mi->disk_id);
    return 0;
}

static INT_PTR cabinet_copy_file(FDINOTIFICATIONTYPE fdint,
                                 PFDINOTIFICATION pfdin)
{
    MSICABDATA *data = pfdin->pv;
    HANDLE handle = 0;
    LPWSTR path = NULL;
    DWORD attrs;

    data->curfile = strdupAtoW(pfdin->psz1);
    if (!data->cb(data->package, data->curfile, MSICABEXTRACT_BEGINEXTRACT, &path,
                  &attrs, data->user))
    {
        /* We're not extracting this file, so free the filename. */
        msi_free(data->curfile);
        data->curfile = NULL;
        goto done;
    }

    TRACE("extracting %s -> %s\n", debugstr_w(data->curfile), debugstr_w(path));

    attrs = attrs & (FILE_ATTRIBUTE_READONLY|FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM);
    if (!attrs) attrs = FILE_ATTRIBUTE_NORMAL;

    handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0,
                         NULL, CREATE_ALWAYS, attrs, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        DWORD attrs2 = GetFileAttributesW(path);

        if (attrs2 == INVALID_FILE_ATTRIBUTES)
        {
            ERR("failed to create %s (error %d)\n", debugstr_w(path), err);
            goto done;
        }
        else if (err == ERROR_ACCESS_DENIED && (attrs2 & FILE_ATTRIBUTE_READONLY))
        {
            TRACE("removing read-only attribute on %s\n", debugstr_w(path));
            SetFileAttributesW( path, attrs2 & ~FILE_ATTRIBUTE_READONLY );
            handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, attrs2, NULL);

            if (handle != INVALID_HANDLE_VALUE) goto done;
            err = GetLastError();
        }
        if (err == ERROR_SHARING_VIOLATION || err == ERROR_USER_MAPPED_FILE)
        {
            WCHAR *tmpfileW, *tmppathW, *p;
            DWORD len;

            TRACE("file in use, scheduling rename operation\n");

            if (!(tmppathW = strdupW( path ))) return ERROR_OUTOFMEMORY;
            if ((p = strrchrW(tmppathW, '\\'))) *p = 0;
            len = strlenW( tmppathW ) + 16;
            if (!(tmpfileW = msi_alloc(len * sizeof(WCHAR))))
            {
                msi_free( tmppathW );
                return ERROR_OUTOFMEMORY;
            }
            if (!GetTempFileNameW(tmppathW, szMsi, 0, tmpfileW)) tmpfileW[0] = 0;
            msi_free( tmppathW );

            handle = CreateFileW(tmpfileW, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, attrs, NULL);

            if (handle != INVALID_HANDLE_VALUE &&
                MoveFileExW(path, NULL, MOVEFILE_DELAY_UNTIL_REBOOT) &&
                MoveFileExW(tmpfileW, path, MOVEFILE_DELAY_UNTIL_REBOOT))
            {
                data->package->need_reboot_at_end = 1;
            }
            else
            {
                WARN("failed to schedule rename operation %s (error %d)\n", debugstr_w(path), GetLastError());
                DeleteFileW( tmpfileW );
            }
            msi_free(tmpfileW);
        }
        else
            WARN("failed to create %s (error %d)\n", debugstr_w(path), err);
    }

done:
    msi_free(path);

    return (INT_PTR)handle;
}

static INT_PTR cabinet_close_file_info(FDINOTIFICATIONTYPE fdint,
                                       PFDINOTIFICATION pfdin)
{
    MSICABDATA *data = pfdin->pv;
    FILETIME ft;
    FILETIME ftLocal;
    HANDLE handle = (HANDLE)pfdin->hf;

    data->mi->is_continuous = FALSE;

    if (!DosDateTimeToFileTime(pfdin->date, pfdin->time, &ft))
        return -1;
    if (!LocalFileTimeToFileTime(&ft, &ftLocal))
        return -1;
    if (!SetFileTime(handle, &ftLocal, 0, &ftLocal))
        return -1;

    CloseHandle(handle);

    data->cb(data->package, data->curfile, MSICABEXTRACT_FILEEXTRACTED, NULL, NULL,
             data->user);

    msi_free(data->curfile);
    data->curfile = NULL;

    return 1;
}

static INT_PTR CDECL cabinet_notify(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin)
{
    switch (fdint)
    {
    case fdintPARTIAL_FILE:
        return cabinet_partial_file(fdint, pfdin);

    case fdintNEXT_CABINET:
        return cabinet_next_cabinet(fdint, pfdin);

    case fdintCOPY_FILE:
        return cabinet_copy_file(fdint, pfdin);

    case fdintCLOSE_FILE_INFO:
        return cabinet_close_file_info(fdint, pfdin);

    default:
        return 0;
    }
}

static INT_PTR CDECL cabinet_notify_stream( FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin )
{
    switch (fdint)
    {
    case fdintPARTIAL_FILE:
        return cabinet_partial_file( fdint, pfdin );

    case fdintNEXT_CABINET:
        return cabinet_next_cabinet_stream( fdint, pfdin );

    case fdintCOPY_FILE:
        return cabinet_copy_file( fdint, pfdin );

    case fdintCLOSE_FILE_INFO:
        return cabinet_close_file_info( fdint, pfdin );

    case fdintCABINET_INFO:
        return 0;

    default:
        ERR("Unexpected notification %d\n", fdint);
        return 0;
    }
}

static BOOL extract_cabinet( MSIPACKAGE* package, MSIMEDIAINFO *mi, LPVOID data )
{
    LPSTR cabinet, cab_path = NULL;
    HFDI hfdi;
    ERF erf;
    BOOL ret = FALSE;

    TRACE("extracting %s disk id %u\n", debugstr_w(mi->cabinet), mi->disk_id);

    hfdi = FDICreate( cabinet_alloc, cabinet_free, cabinet_open, cabinet_read,
                      cabinet_write, cabinet_close, cabinet_seek, 0, &erf );
    if (!hfdi)
    {
        ERR("FDICreate failed\n");
        return FALSE;
    }

    cabinet = strdupWtoA( mi->cabinet );
    if (!cabinet)
        goto done;

    cab_path = strdupWtoA( mi->sourcedir );
    if (!cab_path)
        goto done;

    ret = FDICopy( hfdi, cabinet, cab_path, 0, cabinet_notify, NULL, data );
    if (!ret)
        ERR("FDICopy failed\n");

done:
    FDIDestroy( hfdi );
    msi_free(cabinet );
    msi_free( cab_path );

    if (ret)
        mi->is_extracted = TRUE;

    return ret;
}

static BOOL extract_cabinet_stream( MSIPACKAGE *package, MSIMEDIAINFO *mi, LPVOID data )
{
    static char filename[] = {'<','S','T','R','E','A','M','>',0};
    HFDI hfdi;
    ERF erf;
    BOOL ret = FALSE;

    TRACE("extracting %s disk id %u\n", debugstr_w(mi->cabinet), mi->disk_id);

    hfdi = FDICreate( cabinet_alloc, cabinet_free, cabinet_open_stream, cabinet_read_stream,
                      cabinet_write, cabinet_close_stream, cabinet_seek_stream, 0, &erf );
    if (!hfdi)
    {
        ERR("FDICreate failed\n");
        return FALSE;
    }

    package_disk.package = package;
    package_disk.id      = mi->disk_id;

    ret = FDICopy( hfdi, filename, NULL, 0, cabinet_notify_stream, NULL, data );
    if (!ret) ERR("FDICopy failed\n");

    FDIDestroy( hfdi );
    if (ret) mi->is_extracted = TRUE;
    return ret;
}

/***********************************************************************
 *            msi_cabextract
 *
 * Extract files from a cabinet file or stream.
 */
BOOL msi_cabextract(MSIPACKAGE* package, MSIMEDIAINFO *mi, LPVOID data)
{
    if (mi->cabinet[0] == '#')
    {
        return extract_cabinet_stream( package, mi, data );
    }
    return extract_cabinet( package, mi, data );
}

UINT msi_add_cabinet_stream( MSIPACKAGE *package, UINT disk_id, IStorage *storage, const WCHAR *name )
{
    MSICABINETSTREAM *cab, *item;

    TRACE("%p, %u, %p, %s\n", package, disk_id, storage, debugstr_w(name));

    LIST_FOR_EACH_ENTRY( item, &package->cabinet_streams, MSICABINETSTREAM, entry )
    {
        if (item->disk_id == disk_id)
        {
            TRACE("duplicate disk id %u\n", disk_id);
            return ERROR_FUNCTION_FAILED;
        }
    }
    if (!(cab = msi_alloc( sizeof(*cab) ))) return ERROR_OUTOFMEMORY;
    if (!(cab->stream = msi_alloc( (strlenW( name ) + 1) * sizeof(WCHAR ) )))
    {
        msi_free( cab );
        return ERROR_OUTOFMEMORY;
    }
    strcpyW( cab->stream, name );
    cab->disk_id = disk_id;
    cab->storage = storage;
    IStorage_AddRef( storage );
    list_add_tail( &package->cabinet_streams, &cab->entry );

    return ERROR_SUCCESS;
}
