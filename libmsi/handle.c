/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002-2004 Mike McCormack for CodeWeavers
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

#define COBJMACROS

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "shlwapi.h"
#include "debug.h"
#include "libmsi.h"
#include "msipriv.h"


static CRITICAL_SECTION MSI_object_cs;
static CRITICAL_SECTION_DEBUG MSI_object_cs_debug =
{
    0, 0, &MSI_object_cs,
    { &MSI_object_cs_debug.ProcessLocksList, 
      &MSI_object_cs_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": MSI_object_cs") }
};
static CRITICAL_SECTION MSI_object_cs = { &MSI_object_cs_debug, -1, 0, 0, 0, 0 };

void *msihandle2msiinfo(MSIOBJECT *obj, UINT type)
{
    if( !obj )
        return NULL;
    if( type && obj->type != type )
        return NULL;

    msiobj_addref( obj );
    return obj;
}

void *alloc_msiobject(UINT type, UINT size, msihandledestructor destroy )
{
    MSIOBJECT *info;

    info = msi_alloc_zero( size );
    if( info )
    {
        info->type = type;
        info->refcount = 1;
        info->destructor = destroy;
    }

    return info;
}

void msiobj_addref( MSIOBJECT *info )
{
    if( !info )
        return;

    InterlockedIncrement(&info->refcount);
}

void msiobj_lock( MSIOBJECT *obj )
{
    EnterCriticalSection( &MSI_object_cs );
}

void msiobj_unlock( MSIOBJECT *obj )
{
    LeaveCriticalSection( &MSI_object_cs );
}

int msiobj_release( MSIOBJECT *obj )
{
    int ret;

    if( !obj )
        return -1;

    ret = InterlockedDecrement( &obj->refcount );
    if( ret==0 )
    {
        if( obj->destructor )
            obj->destructor( obj );
        msi_free( obj );
        TRACE("object %p destroyed\n", obj);
    }

    return ret;
}

/***********************************************************
 *   MsiCloseHandle   [MSI.@]
 */
UINT MsiCloseHandle(MSIOBJECT *obj)
{
    TRACE("%x\n",obj);

    if( obj )
        msiobj_release( obj );

    return ERROR_SUCCESS;
}
