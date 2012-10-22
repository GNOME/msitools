/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2006 Mike McCormack for CodeWeavers
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
#include "shlwapi.h"
#include "oleauto.h"
#include "rpcproxy.h"
#include "msipriv.h"
#include "msiserver.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msi);

static LONG dll_count;

/* the UI level */
INSTALLUILEVEL           gUILevel         = INSTALLUILEVEL_BASIC;
HWND                     gUIhwnd          = 0;
INSTALLUI_HANDLERA       gUIHandlerA      = NULL;
INSTALLUI_HANDLERW       gUIHandlerW      = NULL;
INSTALLUI_HANDLER_RECORD gUIHandlerRecord = NULL;
DWORD                    gUIFilter        = 0;
LPVOID                   gUIContext       = NULL;
WCHAR                   *gszLogFile       = NULL;
HINSTANCE msi_hInstance;


/*
 * Dll lifetime tracking declaration
 */
static void LockModule(void)
{
    InterlockedIncrement(&dll_count);
}

static void UnlockModule(void)
{
    InterlockedDecrement(&dll_count);
}

/******************************************************************
 *      DllMain
 */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        msi_hInstance = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
        IsWow64Process( GetCurrentProcess(), &is_wow64 );
        break;
    case DLL_PROCESS_DETACH:
        msi_free_handle_table();
        msi_free( gszLogFile );
        break;
    }
    return TRUE;
}

/******************************************************************
 * DllGetVersion              [MSI.@]
 */
HRESULT WINAPI DllGetVersion(DLLVERSIONINFO *pdvi)
{
    TRACE("%p\n",pdvi);

    if (pdvi->cbSize < sizeof(DLLVERSIONINFO))
        return E_INVALIDARG;

    pdvi->dwMajorVersion = MSI_MAJORVERSION;
    pdvi->dwMinorVersion = MSI_MINORVERSION;
    pdvi->dwBuildNumber = MSI_BUILDNUMBER;
    pdvi->dwPlatformID = DLLVER_PLATFORM_WINDOWS;

    return S_OK;
}

/******************************************************************
 * DllCanUnloadNow            [MSI.@]
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    return dll_count == 0 ? S_OK : S_FALSE;
}

/***********************************************************************
 *  DllRegisterServer (MSI.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources( msi_hInstance );
}

/***********************************************************************
 *  DllUnregisterServer (MSI.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources( msi_hInstance );
}
