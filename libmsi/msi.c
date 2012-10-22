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

#define COBJMACROS
#define NONAMELESSUNION

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winnls.h"
#include "shlwapi.h"
#include "msi.h"
#include "msidefs.h"
#include "msiquery.h"
#include "msipriv.h"
#include "msiserver.h"
#include "wincrypt.h"
#include "winver.h"
#include "wingdi.h"
#include "winuser.h"
#include "shlobj.h"
#include "shobjidl.h"
#include "objidl.h"
#include "wintrust.h"
#include "softpub.h"

#include "initguid.h"
#include "msxml2.h"

#include "wine/debug.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(msi);

static const WCHAR installerW[] = {'\\','I','n','s','t','a','l','l','e','r',0};

UINT WINAPI MsiAdvertiseProductA(LPCSTR szPackagePath, LPCSTR szScriptfilePath,
                LPCSTR szTransforms, LANGID lgidLanguage)
{
    FIXME("%s %s %s %08x\n",debugstr_a(szPackagePath),
          debugstr_a(szScriptfilePath), debugstr_a(szTransforms), lgidLanguage);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiAdvertiseProductW(LPCWSTR szPackagePath, LPCWSTR szScriptfilePath,
                LPCWSTR szTransforms, LANGID lgidLanguage)
{
    FIXME("%s %s %s %08x\n",debugstr_w(szPackagePath),
          debugstr_w(szScriptfilePath), debugstr_w(szTransforms), lgidLanguage);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiAdvertiseProductExA(LPCSTR szPackagePath, LPCSTR szScriptfilePath,
      LPCSTR szTransforms, LANGID lgidLanguage, DWORD dwPlatform, DWORD dwOptions)
{
    FIXME("%s %s %s %08x %08x %08x\n", debugstr_a(szPackagePath),
          debugstr_a(szScriptfilePath), debugstr_a(szTransforms),
          lgidLanguage, dwPlatform, dwOptions);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiAdvertiseProductExW( LPCWSTR szPackagePath, LPCWSTR szScriptfilePath,
      LPCWSTR szTransforms, LANGID lgidLanguage, DWORD dwPlatform, DWORD dwOptions)
{
    FIXME("%s %s %s %08x %08x %08x\n", debugstr_w(szPackagePath),
          debugstr_w(szScriptfilePath), debugstr_w(szTransforms),
          lgidLanguage, dwPlatform, dwOptions);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT msi_strcpy_to_awstring( LPCWSTR str, awstring *awbuf, DWORD *sz )
{
    UINT len, r = ERROR_SUCCESS;

    if (awbuf->str.w && !sz )
        return ERROR_INVALID_PARAMETER;

    if (!sz)
        return r;
 
    if (awbuf->unicode)
    {
        len = lstrlenW( str );
        if (awbuf->str.w) 
            lstrcpynW( awbuf->str.w, str, *sz );
    }
    else
    {
        len = WideCharToMultiByte( CP_ACP, 0, str, -1, NULL, 0, NULL, NULL );
        if (len)
            len--;
        WideCharToMultiByte( CP_ACP, 0, str, -1, awbuf->str.a, *sz, NULL, NULL );
        if ( awbuf->str.a && *sz && (len >= *sz) )
            awbuf->str.a[*sz - 1] = 0;
    }

    if (awbuf->str.w && len >= *sz)
        r = ERROR_MORE_DATA;
    *sz = len;
    return r;
}

UINT WINAPI MsiEnableLogA(DWORD dwLogMode, LPCSTR szLogFile, DWORD attributes)
{
    LPWSTR szwLogFile = NULL;
    UINT r;

    TRACE("%08x %s %08x\n", dwLogMode, debugstr_a(szLogFile), attributes);

    if( szLogFile )
    {
        szwLogFile = strdupAtoW( szLogFile );
        if( !szwLogFile )
            return ERROR_OUTOFMEMORY;
    }
    r = MsiEnableLogW( dwLogMode, szwLogFile, attributes );
    msi_free( szwLogFile );
    return r;
}

UINT WINAPI MsiEnableLogW(DWORD dwLogMode, LPCWSTR szLogFile, DWORD attributes)
{
    TRACE("%08x %s %08x\n", dwLogMode, debugstr_w(szLogFile), attributes);

    msi_free(gszLogFile);
    gszLogFile = NULL;
    if (szLogFile)
    {
        HANDLE file;

        if (!(attributes & INSTALLLOGATTRIBUTES_APPEND))
            DeleteFileW(szLogFile);
        file = CreateFileW(szLogFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE)
        {
            gszLogFile = strdupW(szLogFile);
            CloseHandle(file);
        }
        else
            ERR("Unable to enable log %s (%u)\n", debugstr_w(szLogFile), GetLastError());
    }

    return ERROR_SUCCESS;
}

UINT WINAPI MsiEnumComponentCostsA( MSIHANDLE handle, LPCSTR component, DWORD index,
                                    INSTALLSTATE state, LPSTR drive, DWORD *buflen,
                                    int *cost, int *temp )
{
    UINT r;
    DWORD len;
    WCHAR *driveW, *componentW = NULL;

    TRACE("%d, %s, %u, %d, %p, %p, %p %p\n", handle, debugstr_a(component), index,
          state, drive, buflen, cost, temp);

    if (!drive || !buflen) return ERROR_INVALID_PARAMETER;
    if (component && !(componentW = strdupAtoW( component ))) return ERROR_OUTOFMEMORY;

    len = *buflen;
    if (!(driveW = msi_alloc( len * sizeof(WCHAR) )))
    {
        msi_free( componentW );
        return ERROR_OUTOFMEMORY;
    }
    r = MsiEnumComponentCostsW( handle, componentW, index, state, driveW, buflen, cost, temp );
    if (!r)
    {
        WideCharToMultiByte( CP_ACP, 0, driveW, -1, drive, len, NULL, NULL );
    }
    msi_free( componentW );
    msi_free( driveW );
    return r;
}

static UINT set_drive( WCHAR *buffer, WCHAR letter )
{
    buffer[0] = letter;
    buffer[1] = ':';
    buffer[2] = 0;
    return 2;
}

UINT WINAPI MsiEnumComponentCostsW( MSIHANDLE handle, LPCWSTR component, DWORD index,
                                    INSTALLSTATE state, LPWSTR drive, DWORD *buflen,
                                    int *cost, int *temp )
{
    UINT r = ERROR_NO_MORE_ITEMS;
    MSICOMPONENT *comp = NULL;
    MSIPACKAGE *package;
    MSIFILE *file;
    STATSTG stat = {0};
    WCHAR path[MAX_PATH];

    TRACE("%d, %s, %u, %d, %p, %p, %p %p\n", handle, debugstr_w(component), index,
          state, drive, buflen, cost, temp);

    if (!drive || !buflen || !cost || !temp) return ERROR_INVALID_PARAMETER;
    package = msihandle2msiinfo( handle, MSIHANDLETYPE_PACKAGE );
    if ( !package )
        return ERROR_INVALID_HANDLE;

    if (!msi_get_property_int( package->db, szCostingComplete, 0 ))
    {
        msiobj_release( &package->hdr );
        return ERROR_FUNCTION_NOT_CALLED;
    }
    if (component && component[0] && !(comp = msi_get_loaded_component( package, component )))
    {
        msiobj_release( &package->hdr );
        return ERROR_UNKNOWN_COMPONENT;
    }
    if (*buflen < 3)
    {
        *buflen = 2;
        msiobj_release( &package->hdr );
        return ERROR_MORE_DATA;
    }
    if (index)
    {
        msiobj_release( &package->hdr );
        return ERROR_NO_MORE_ITEMS;
    }

    drive[0] = 0;
    *cost = *temp = 0;
    GetWindowsDirectoryW( path, MAX_PATH );
    if (component && component[0])
    {
        if (comp->assembly && !comp->assembly->application) *temp = comp->Cost;
        if (!comp->Enabled || !comp->KeyPath)
        {
            *cost = 0;
            *buflen = set_drive( drive, path[0] );
            r = ERROR_SUCCESS;
        }
        else if ((file = msi_get_loaded_file( package, comp->KeyPath )))
        {
            *cost = max( 8, comp->Cost / 512 );
            *buflen = set_drive( drive, file->TargetPath[0] );
            r = ERROR_SUCCESS;
        }
    }
    else if (IStorage_Stat( package->db->storage, &stat, STATFLAG_NONAME ) == S_OK)
    {
        *temp = max( 8, stat.cbSize.QuadPart / 512 );
        *buflen = set_drive( drive, path[0] );
        r = ERROR_SUCCESS;
    }
    msiobj_release( &package->hdr );
    return r;
}

INSTALLUILEVEL WINAPI MsiSetInternalUI(INSTALLUILEVEL dwUILevel, HWND *phWnd)
{
    INSTALLUILEVEL old = gUILevel;
    HWND oldwnd = gUIhwnd;

    TRACE("%08x %p\n", dwUILevel, phWnd);

    gUILevel = dwUILevel;
    if (phWnd)
    {
        gUIhwnd = *phWnd;
        *phWnd = oldwnd;
    }
    return old;
}

INSTALLUI_HANDLERA WINAPI MsiSetExternalUIA(INSTALLUI_HANDLERA puiHandler,
                                  DWORD dwMessageFilter, LPVOID pvContext)
{
    INSTALLUI_HANDLERA prev = gUIHandlerA;

    TRACE("%p %08x %p\n", puiHandler, dwMessageFilter, pvContext);

    gUIHandlerA = puiHandler;
    gUIHandlerW = NULL;
    gUIFilter   = dwMessageFilter;
    gUIContext  = pvContext;

    return prev;
}

INSTALLUI_HANDLERW WINAPI MsiSetExternalUIW(INSTALLUI_HANDLERW puiHandler,
                                  DWORD dwMessageFilter, LPVOID pvContext)
{
    INSTALLUI_HANDLERW prev = gUIHandlerW;

    TRACE("%p %08x %p\n", puiHandler, dwMessageFilter, pvContext);

    gUIHandlerA = NULL;
    gUIHandlerW = puiHandler;
    gUIFilter   = dwMessageFilter;
    gUIContext  = pvContext;

    return prev;
}

UINT WINAPI MsiMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType,
                WORD wLanguageId, DWORD f)
{
    FIXME("%p %s %s %u %08x %08x\n", hWnd, debugstr_a(lpText), debugstr_a(lpCaption),
          uType, wLanguageId, f);
    return MessageBoxExA(hWnd,lpText,lpCaption,uType,wLanguageId); 
}

UINT WINAPI MsiMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType,
                WORD wLanguageId, DWORD f)
{
    FIXME("%p %s %s %u %08x %08x\n", hWnd, debugstr_w(lpText), debugstr_w(lpCaption),
          uType, wLanguageId, f);
    return MessageBoxExW(hWnd,lpText,lpCaption,uType,wLanguageId); 
}

UINT WINAPI MsiMessageBoxExA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType,
                DWORD unknown, WORD wLanguageId, DWORD f)
{
    FIXME("(%p, %s, %s, %u, 0x%08x, 0x%08x, 0x%08x): semi-stub\n", hWnd, debugstr_a(lpText),
            debugstr_a(lpCaption), uType, unknown, wLanguageId, f);
    return MessageBoxExA(hWnd, lpText, lpCaption, uType, wLanguageId);
}

UINT WINAPI MsiMessageBoxExW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType,
                DWORD unknown, WORD wLanguageId, DWORD f)
{
    FIXME("(%p, %s, %s, %u, 0x%08x, 0x%08x, 0x%08x): semi-stub\n", hWnd, debugstr_w(lpText),
            debugstr_w(lpCaption), uType, unknown, wLanguageId, f);
    return MessageBoxExW(hWnd, lpText, lpCaption, uType, wLanguageId);
}

UINT WINAPI MsiProvideAssemblyA( LPCSTR szAssemblyName, LPCSTR szAppContext,
                DWORD dwInstallMode, DWORD dwAssemblyInfo, LPSTR lpPathBuf,
                LPDWORD pcchPathBuf )
{
    FIXME("%s %s %08x %08x %p %p\n", debugstr_a(szAssemblyName),
          debugstr_a(szAppContext), dwInstallMode, dwAssemblyInfo, lpPathBuf,
          pcchPathBuf);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiProvideAssemblyW( LPCWSTR szAssemblyName, LPCWSTR szAppContext,
                DWORD dwInstallMode, DWORD dwAssemblyInfo, LPWSTR lpPathBuf,
                LPDWORD pcchPathBuf )
{
    FIXME("%s %s %08x %08x %p %p\n", debugstr_w(szAssemblyName),
          debugstr_w(szAppContext), dwInstallMode, dwAssemblyInfo, lpPathBuf,
          pcchPathBuf);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiProvideComponentFromDescriptorA( LPCSTR szDescriptor,
                LPSTR szPath, LPDWORD pcchPath, LPDWORD pcchArgs )
{
    FIXME("%s %p %p %p\n", debugstr_a(szDescriptor), szPath, pcchPath, pcchArgs );
    return ERROR_CALL_NOT_IMPLEMENTED;
}

UINT WINAPI MsiProvideComponentFromDescriptorW( LPCWSTR szDescriptor,
                LPWSTR szPath, LPDWORD pcchPath, LPDWORD pcchArgs )
{
    FIXME("%s %p %p %p\n", debugstr_w(szDescriptor), szPath, pcchPath, pcchArgs );
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/******************************************************************
 * MsiGetProductPropertyA      [MSI.@]
 */
UINT WINAPI MsiGetProductPropertyA(MSIHANDLE hProduct, LPCSTR szProperty,
                                   LPSTR szValue, LPDWORD pccbValue)
{
    LPWSTR prop = NULL, val = NULL;
    DWORD len;
    UINT r;

    TRACE("(%d, %s, %p, %p)\n", hProduct, debugstr_a(szProperty),
          szValue, pccbValue);

    if (szValue && !pccbValue)
        return ERROR_INVALID_PARAMETER;

    if (szProperty) prop = strdupAtoW(szProperty);

    len = 0;
    r = MsiGetProductPropertyW(hProduct, prop, NULL, &len);
    if (r != ERROR_SUCCESS && r != ERROR_MORE_DATA)
        goto done;

    if (r == ERROR_SUCCESS)
    {
        if (szValue) *szValue = '\0';
        if (pccbValue) *pccbValue = 0;
        goto done;
    }

    val = msi_alloc(++len * sizeof(WCHAR));
    if (!val)
    {
        r = ERROR_OUTOFMEMORY;
        goto done;
    }

    r = MsiGetProductPropertyW(hProduct, prop, val, &len);
    if (r != ERROR_SUCCESS)
        goto done;

    len = WideCharToMultiByte(CP_ACP, 0, val, -1, NULL, 0, NULL, NULL);

    if (szValue)
        WideCharToMultiByte(CP_ACP, 0, val, -1, szValue,
                            *pccbValue, NULL, NULL);

    if (pccbValue)
    {
        if (len > *pccbValue)
            r = ERROR_MORE_DATA;

        *pccbValue = len - 1;
    }

done:
    msi_free(prop);
    msi_free(val);

    return r;
}

/******************************************************************
 * MsiGetProductPropertyW      [MSI.@]
 */
UINT WINAPI MsiGetProductPropertyW(MSIHANDLE hProduct, LPCWSTR szProperty,
                                   LPWSTR szValue, LPDWORD pccbValue)
{
    MSIPACKAGE *package;
    MSIQUERY *view = NULL;
    MSIRECORD *rec = NULL;
    LPCWSTR val;
    UINT r;

    static const WCHAR query[] = {
       'S','E','L','E','C','T',' ','*',' ','F','R','O','M',' ',
       '`','P','r','o','p','e','r','t','y','`',' ','W','H','E','R','E',' ',
       '`','P','r','o','p','e','r','t','y','`','=','\'','%','s','\'',0};

    TRACE("(%d, %s, %p, %p)\n", hProduct, debugstr_w(szProperty),
          szValue, pccbValue);

    if (!szProperty)
        return ERROR_INVALID_PARAMETER;

    if (szValue && !pccbValue)
        return ERROR_INVALID_PARAMETER;

    package = msihandle2msiinfo(hProduct, MSIHANDLETYPE_PACKAGE);
    if (!package)
        return ERROR_INVALID_HANDLE;

    r = MSI_OpenQuery(package->db, &view, query, szProperty);
    if (r != ERROR_SUCCESS)
        goto done;

    r = MSI_ViewExecute(view, 0);
    if (r != ERROR_SUCCESS)
        goto done;

    r = MSI_ViewFetch(view, &rec);
    if (r != ERROR_SUCCESS)
        goto done;

    val = MSI_RecordGetString(rec, 2);
    if (!val)
        goto done;

    if (lstrlenW(val) >= *pccbValue)
    {
        lstrcpynW(szValue, val, *pccbValue);
        *pccbValue = lstrlenW(val);
        r = ERROR_MORE_DATA;
    }
    else
    {
        lstrcpyW(szValue, val);
        *pccbValue = lstrlenW(val);
        r = ERROR_SUCCESS;
    }

done:
    if (view)
    {
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
        if (rec) msiobj_release(&rec->hdr);
    }

    if (!rec)
    {
        if (szValue) *szValue = '\0';
        if (pccbValue) *pccbValue = 0;
        r = ERROR_SUCCESS;
    }

    msiobj_release(&package->hdr);
    return r;
}

UINT WINAPI MsiVerifyPackageA( LPCSTR szPackage )
{
    UINT r;
    LPWSTR szPack = NULL;

    TRACE("%s\n", debugstr_a(szPackage) );

    if( szPackage )
    {
        szPack = strdupAtoW( szPackage );
        if( !szPack )
            return ERROR_OUTOFMEMORY;
    }

    r = MsiVerifyPackageW( szPack );

    msi_free( szPack );

    return r;
}

UINT WINAPI MsiVerifyPackageW( LPCWSTR szPackage )
{
    MSIHANDLE handle;
    UINT r;

    TRACE("%s\n", debugstr_w(szPackage) );

    r = MsiOpenDatabaseW( szPackage, MSIDBOPEN_READONLY, &handle );
    MsiCloseHandle( handle );

    return r;
}

/***********************************************************************
 * MsiGetFeatureUsageW           [MSI.@]
 */
UINT WINAPI MsiGetFeatureUsageW( LPCWSTR szProduct, LPCWSTR szFeature,
                                 LPDWORD pdwUseCount, LPWORD pwDateUsed )
{
    FIXME("%s %s %p %p\n",debugstr_w(szProduct), debugstr_w(szFeature),
          pdwUseCount, pwDateUsed);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/***********************************************************************
 * MsiGetFeatureUsageA           [MSI.@]
 */
UINT WINAPI MsiGetFeatureUsageA( LPCSTR szProduct, LPCSTR szFeature,
                                 LPDWORD pdwUseCount, LPWORD pwDateUsed )
{
    LPWSTR prod = NULL, feat = NULL;
    UINT ret = ERROR_OUTOFMEMORY;

    TRACE("%s %s %p %p\n", debugstr_a(szProduct), debugstr_a(szFeature),
          pdwUseCount, pwDateUsed);

    prod = strdupAtoW( szProduct );
    if (szProduct && !prod)
        goto end;

    feat = strdupAtoW( szFeature );
    if (szFeature && !feat)
        goto end;

    ret = MsiGetFeatureUsageW( prod, feat, pdwUseCount, pwDateUsed );

end:
    msi_free( prod );
    msi_free( feat );

    return ret;
}

/***********************************************************************
 * MsiCreateAndVerifyInstallerDirectory [MSI.@]
 *
 * Notes: undocumented
 */
UINT WINAPI MsiCreateAndVerifyInstallerDirectory(DWORD dwReserved)
{
    WCHAR path[MAX_PATH];

    TRACE("%d\n", dwReserved);

    if (dwReserved)
    {
        FIXME("dwReserved=%d\n", dwReserved);
        return ERROR_INVALID_PARAMETER;
    }

    if (!GetWindowsDirectoryW(path, MAX_PATH))
        return ERROR_FUNCTION_FAILED;

    lstrcatW(path, installerW);

    if (!CreateDirectoryW(path, NULL))
        return ERROR_FUNCTION_FAILED;

    return ERROR_SUCCESS;
}

typedef struct
{
    unsigned int i[2];
    unsigned int buf[4];
    unsigned char in[64];
    unsigned char digest[16];
} MD5_CTX;

extern VOID WINAPI MD5Init( MD5_CTX *);
extern VOID WINAPI MD5Update( MD5_CTX *, const unsigned char *, unsigned int );
extern VOID WINAPI MD5Final( MD5_CTX *);

/***********************************************************************
 * MsiGetFileHashW            [MSI.@]
 */
UINT WINAPI MsiGetFileHashW( LPCWSTR szFilePath, DWORD dwOptions,
                             PMSIFILEHASHINFO pHash )
{
    HANDLE handle, mapping;
    void *p;
    DWORD length;
    UINT r = ERROR_FUNCTION_FAILED;

    TRACE("%s %08x %p\n", debugstr_w(szFilePath), dwOptions, pHash );

    if (!szFilePath)
        return ERROR_INVALID_PARAMETER;

    if (!*szFilePath)
        return ERROR_PATH_NOT_FOUND;

    if (dwOptions)
        return ERROR_INVALID_PARAMETER;
    if (!pHash)
        return ERROR_INVALID_PARAMETER;
    if (pHash->dwFileHashInfoSize < sizeof *pHash)
        return ERROR_INVALID_PARAMETER;

    handle = CreateFileW( szFilePath, GENERIC_READ,
                          FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL );
    if (handle == INVALID_HANDLE_VALUE)
    {
        WARN("can't open file %u\n", GetLastError());
        return ERROR_FILE_NOT_FOUND;
    }
    length = GetFileSize( handle, NULL );

    if (length)
    {
        mapping = CreateFileMappingW( handle, NULL, PAGE_READONLY, 0, 0, NULL );
        if (mapping)
        {
            p = MapViewOfFile( mapping, FILE_MAP_READ, 0, 0, length );
            if (p)
            {
                MD5_CTX ctx;

                MD5Init( &ctx );
                MD5Update( &ctx, p, length );
                MD5Final( &ctx );
                UnmapViewOfFile( p );

                memcpy( pHash->dwData, ctx.digest, sizeof pHash->dwData );
                r = ERROR_SUCCESS;
            }
            CloseHandle( mapping );
        }
    }
    else
    {
        /* Empty file -> set hash to 0 */
        memset( pHash->dwData, 0, sizeof pHash->dwData );
        r = ERROR_SUCCESS;
    }

    CloseHandle( handle );

    return r;
}

/***********************************************************************
 * MsiGetFileHashA            [MSI.@]
 */
UINT WINAPI MsiGetFileHashA( LPCSTR szFilePath, DWORD dwOptions,
                             PMSIFILEHASHINFO pHash )
{
    LPWSTR file;
    UINT r;

    TRACE("%s %08x %p\n", debugstr_a(szFilePath), dwOptions, pHash );

    file = strdupAtoW( szFilePath );
    if (szFilePath && !file)
        return ERROR_OUTOFMEMORY;

    r = MsiGetFileHashW( file, dwOptions, pHash );
    msi_free( file );
    return r;
}

/***********************************************************************
 * MsiAdvertiseScriptW        [MSI.@]
 */
UINT WINAPI MsiAdvertiseScriptW( LPCWSTR szScriptFile, DWORD dwFlags,
                                 PHKEY phRegData, BOOL fRemoveItems )
{
    FIXME("%s %08x %p %d\n",
          debugstr_w( szScriptFile ), dwFlags, phRegData, fRemoveItems );
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/***********************************************************************
 * MsiAdvertiseScriptA        [MSI.@]
 */
UINT WINAPI MsiAdvertiseScriptA( LPCSTR szScriptFile, DWORD dwFlags,
                                 PHKEY phRegData, BOOL fRemoveItems )
{
    FIXME("%s %08x %p %d\n",
          debugstr_a( szScriptFile ), dwFlags, phRegData, fRemoveItems );
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/***********************************************************************
 * MsiIsProductElevatedW        [MSI.@]
 */
UINT WINAPI MsiIsProductElevatedW( LPCWSTR szProduct, BOOL *pfElevated )
{
    FIXME("%s %p - stub\n",
          debugstr_w( szProduct ), pfElevated );
    *pfElevated = TRUE;
    return ERROR_SUCCESS;
}

/***********************************************************************
 * MsiIsProductElevatedA        [MSI.@]
 */
UINT WINAPI MsiIsProductElevatedA( LPCSTR szProduct, BOOL *pfElevated )
{
    FIXME("%s %p - stub\n",
          debugstr_a( szProduct ), pfElevated );
    *pfElevated = TRUE;
    return ERROR_SUCCESS;
}

/***********************************************************************
 * MsiSetExternalUIRecord     [MSI.@]
 */
UINT WINAPI MsiSetExternalUIRecord( INSTALLUI_HANDLER_RECORD handler,
                                    DWORD filter, LPVOID context,
                                    PINSTALLUI_HANDLER_RECORD prev )
{
    TRACE("%p %08x %p %p\n", handler, filter, context, prev);

    if (prev)
        *prev = gUIHandlerRecord;

    gUIHandlerRecord = handler;
    gUIFilter        = filter;
    gUIContext       = context;

    return ERROR_SUCCESS;
}

/***********************************************************************
 * MsiInstallMissingComponentA     [MSI.@]
 */
UINT WINAPI MsiInstallMissingComponentA( LPCSTR product, LPCSTR component, INSTALLSTATE state )
{
    UINT r;
    WCHAR *productW = NULL, *componentW = NULL;

    TRACE("%s, %s, %d\n", debugstr_a(product), debugstr_a(component), state);

    if (product && !(productW = strdupAtoW( product )))
        return ERROR_OUTOFMEMORY;

    if (component && !(componentW = strdupAtoW( component )))
    {
        msi_free( productW );
        return ERROR_OUTOFMEMORY;
    }

    r = MsiInstallMissingComponentW( productW, componentW, state );
    msi_free( productW );
    msi_free( componentW );
    return r;
}

/***********************************************************************
 * MsiInstallMissingComponentW     [MSI.@]
 */
UINT WINAPI MsiInstallMissingComponentW(LPCWSTR szProduct, LPCWSTR szComponent, INSTALLSTATE eInstallState)
{
    FIXME("(%s %s %d\n", debugstr_w(szProduct), debugstr_w(szComponent), eInstallState);
    return ERROR_SUCCESS;
}

/***********************************************************************
 * MsiBeginTransactionA     [MSI.@]
 */
UINT WINAPI MsiBeginTransactionA( LPCSTR name, DWORD attrs, MSIHANDLE *id, HANDLE *event )
{
    WCHAR *nameW;
    UINT r;

    FIXME("%s %u %p %p\n", debugstr_a(name), attrs, id, event);

    nameW = strdupAtoW( name );
    if (name && !nameW)
        return ERROR_OUTOFMEMORY;

    r = MsiBeginTransactionW( nameW, attrs, id, event );
    msi_free( nameW );
    return r;
}

/***********************************************************************
 * MsiBeginTransactionW     [MSI.@]
 */
UINT WINAPI MsiBeginTransactionW( LPCWSTR name, DWORD attrs, MSIHANDLE *id, HANDLE *event )
{
    FIXME("%s %u %p %p\n", debugstr_w(name), attrs, id, event);

    *id = (MSIHANDLE)0xdeadbeef;
    *event = (HANDLE)0xdeadbeef;

    return ERROR_SUCCESS;
}

/***********************************************************************
 * MsiEndTransaction     [MSI.@]
 */
UINT WINAPI MsiEndTransaction( DWORD state )
{
    FIXME("%u\n", state);
    return ERROR_SUCCESS;
}

UINT WINAPI Migrate10CachedPackagesW(void* a, void* b, void* c, DWORD d)
{
    FIXME("%p,%p,%p,%08x\n", a, b, c, d);
    return ERROR_SUCCESS;
}
