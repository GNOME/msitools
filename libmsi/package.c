/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2004 Aric Stewart for CodeWeavers
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

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#define COBJMACROS

#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winnls.h"
#include "shlwapi.h"
#include "wingdi.h"
#include "wine/debug.h"
#include "msi.h"
#include "msiquery.h"
#include "objidl.h"
#include "wincrypt.h"
#include "wingdi.h"
#include "winuser.h"
#include "wininet.h"
#include "winver.h"
#include "urlmon.h"
#include "shlobj.h"
#include "wine/unicode.h"
#include "objbase.h"
#include "msidefs.h"
#include "sddl.h"

#include "msipriv.h"
#include "msiserver.h"

WINE_DEFAULT_DEBUG_CHANNEL(msi);

MSICOMPONENT *msi_get_loaded_component( MSIPACKAGE *package, const WCHAR *Component )
{
    MSICOMPONENT *comp;

    LIST_FOR_EACH_ENTRY( comp, &package->components, MSICOMPONENT, entry )
    {
        if (!strcmpW( Component, comp->Component )) return comp;
    }
    return NULL;
}

MSIFEATURE *msi_get_loaded_feature(MSIPACKAGE* package, const WCHAR *Feature )
{
    MSIFEATURE *feature;

    LIST_FOR_EACH_ENTRY( feature, &package->features, MSIFEATURE, entry )
    {
        if (!strcmpW( Feature, feature->Feature )) return feature;
    }
    return NULL;
}

MSIFILE *msi_get_loaded_file( MSIPACKAGE *package, const WCHAR *key )
{
    MSIFILE *file;

    LIST_FOR_EACH_ENTRY( file, &package->files, MSIFILE, entry )
    {
        if (!strcmpW( key, file->File )) return file;
    }
    return NULL;
}

MSIFILEPATCH *msi_get_loaded_filepatch( MSIPACKAGE *package, const WCHAR *key )
{
    MSIFILEPATCH *patch;

    /* FIXME: There might be more than one patch */
    LIST_FOR_EACH_ENTRY( patch, &package->filepatches, MSIFILEPATCH, entry )
    {
        if (!strcmpW( key, patch->File->File )) return patch;
    }
    return NULL;
}

MSIFOLDER *msi_get_loaded_folder( MSIPACKAGE *package, const WCHAR *dir )
{
    MSIFOLDER *folder;

    LIST_FOR_EACH_ENTRY( folder, &package->folders, MSIFOLDER, entry )
    {
        if (!strcmpW( dir, folder->Directory )) return folder;
    }
    return NULL;
}


static void remove_tracked_tempfiles( MSIPACKAGE *package )
{
    struct list *item, *cursor;

    LIST_FOR_EACH_SAFE( item, cursor, &package->tempfiles )
    {
        MSITEMPFILE *temp = LIST_ENTRY( item, MSITEMPFILE, entry );

        list_remove( &temp->entry );
        TRACE("deleting temp file %s\n", debugstr_w( temp->Path ));
        DeleteFileW( temp->Path );
        msi_free( temp->Path );
        msi_free( temp );
    }
}

static void free_feature( MSIFEATURE *feature )
{
    struct list *item, *cursor;

    LIST_FOR_EACH_SAFE( item, cursor, &feature->Children )
    {
        FeatureList *fl = LIST_ENTRY( item, FeatureList, entry );
        list_remove( &fl->entry );
        msi_free( fl );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &feature->Components )
    {
        ComponentList *cl = LIST_ENTRY( item, ComponentList, entry );
        list_remove( &cl->entry );
        msi_free( cl );
    }
    msi_free( feature->Feature );
    msi_free( feature->Feature_Parent );
    msi_free( feature->Directory );
    msi_free( feature->Description );
    msi_free( feature->Title );
    msi_free( feature );
}

static void free_folder( MSIFOLDER *folder )
{
    struct list *item, *cursor;

    LIST_FOR_EACH_SAFE( item, cursor, &folder->children )
    {
        FolderList *fl = LIST_ENTRY( item, FolderList, entry );
        list_remove( &fl->entry );
        msi_free( fl );
    }
    msi_free( folder->Parent );
    msi_free( folder->Directory );
    msi_free( folder->TargetDefault );
    msi_free( folder->SourceLongPath );
    msi_free( folder->SourceShortPath );
    msi_free( folder->ResolvedTarget );
    msi_free( folder->ResolvedSource );
    msi_free( folder );
}

static void free_extension( MSIEXTENSION *ext )
{
    struct list *item, *cursor;

    LIST_FOR_EACH_SAFE( item, cursor, &ext->verbs )
    {
        MSIVERB *verb = LIST_ENTRY( item, MSIVERB, entry );

        list_remove( &verb->entry );
        msi_free( verb->Verb );
        msi_free( verb->Command );
        msi_free( verb->Argument );
        msi_free( verb );
    }

    msi_free( ext->Extension );
    msi_free( ext->ProgIDText );
    msi_free( ext );
}

static void free_assembly( MSIASSEMBLY *assembly )
{
    msi_free( assembly->feature );
    msi_free( assembly->manifest );
    msi_free( assembly->application );
    msi_free( assembly->display_name );
    if (assembly->tempdir) RemoveDirectoryW( assembly->tempdir );
    msi_free( assembly->tempdir );
    msi_free( assembly );
}

void msi_free_action_script( MSIPACKAGE *package, UINT script )
{
    UINT i;
    for (i = 0; i < package->script->ActionCount[script]; i++)
        msi_free( package->script->Actions[script][i] );

    msi_free( package->script->Actions[script] );
    package->script->Actions[script] = NULL;
    package->script->ActionCount[script] = 0;
}

static void free_package_structures( MSIPACKAGE *package )
{
    INT i;
    struct list *item, *cursor;

    LIST_FOR_EACH_SAFE( item, cursor, &package->features )
    {
        MSIFEATURE *feature = LIST_ENTRY( item, MSIFEATURE, entry );
        list_remove( &feature->entry );
        free_feature( feature );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->folders )
    {
        MSIFOLDER *folder = LIST_ENTRY( item, MSIFOLDER, entry );
        list_remove( &folder->entry );
        free_folder( folder );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->components )
    {
        MSICOMPONENT *comp = LIST_ENTRY( item, MSICOMPONENT, entry );

        list_remove( &comp->entry );
        msi_free( comp->Component );
        msi_free( comp->ComponentId );
        msi_free( comp->Directory );
        msi_free( comp->Condition );
        msi_free( comp->KeyPath );
        msi_free( comp->FullKeypath );
        if (comp->assembly) free_assembly( comp->assembly );
        msi_free( comp );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->files )
    {
        MSIFILE *file = LIST_ENTRY( item, MSIFILE, entry );

        list_remove( &file->entry );
        msi_free( file->File );
        msi_free( file->FileName );
        msi_free( file->ShortName );
        msi_free( file->LongName );
        msi_free( file->Version );
        msi_free( file->Language );
        msi_free( file->TargetPath );
        msi_free( file );
    }

    /* clean up extension, progid, class and verb structures */
    LIST_FOR_EACH_SAFE( item, cursor, &package->classes )
    {
        MSICLASS *cls = LIST_ENTRY( item, MSICLASS, entry );

        list_remove( &cls->entry );
        msi_free( cls->clsid );
        msi_free( cls->Context );
        msi_free( cls->Description );
        msi_free( cls->FileTypeMask );
        msi_free( cls->IconPath );
        msi_free( cls->DefInprocHandler );
        msi_free( cls->DefInprocHandler32 );
        msi_free( cls->Argument );
        msi_free( cls->ProgIDText );
        msi_free( cls );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->extensions )
    {
        MSIEXTENSION *ext = LIST_ENTRY( item, MSIEXTENSION, entry );

        list_remove( &ext->entry );
        free_extension( ext );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->progids )
    {
        MSIPROGID *progid = LIST_ENTRY( item, MSIPROGID, entry );

        list_remove( &progid->entry );
        msi_free( progid->ProgID );
        msi_free( progid->Description );
        msi_free( progid->IconPath );
        msi_free( progid );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->mimes )
    {
        MSIMIME *mt = LIST_ENTRY( item, MSIMIME, entry );

        list_remove( &mt->entry );
        msi_free( mt->suffix );
        msi_free( mt->clsid );
        msi_free( mt->ContentType );
        msi_free( mt );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->appids )
    {
        MSIAPPID *appid = LIST_ENTRY( item, MSIAPPID, entry );

        list_remove( &appid->entry );
        msi_free( appid->AppID );
        msi_free( appid->RemoteServerName );
        msi_free( appid->LocalServer );
        msi_free( appid->ServiceParameters );
        msi_free( appid->DllSurrogate );
        msi_free( appid );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->sourcelist_info )
    {
        MSISOURCELISTINFO *info = LIST_ENTRY( item, MSISOURCELISTINFO, entry );

        list_remove( &info->entry );
        msi_free( info->value );
        msi_free( info );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->sourcelist_media )
    {
        MSIMEDIADISK *info = LIST_ENTRY( item, MSIMEDIADISK, entry );

        list_remove( &info->entry );
        msi_free( info->volume_label );
        msi_free( info->disk_prompt );
        msi_free( info );
    }

    if (package->script)
    {
        for (i = 0; i < SCRIPT_MAX; i++)
            msi_free_action_script( package, i );

        for (i = 0; i < package->script->UniqueActionsCount; i++)
            msi_free( package->script->UniqueActions[i] );

        msi_free( package->script->UniqueActions );
        msi_free( package->script );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->binaries )
    {
        MSIBINARY *binary = LIST_ENTRY( item, MSIBINARY, entry );

        list_remove( &binary->entry );
        if (binary->module)
            FreeLibrary( binary->module );
        if (!DeleteFileW( binary->tmpfile ))
            ERR("failed to delete %s (%u)\n", debugstr_w(binary->tmpfile), GetLastError());
        msi_free( binary->source );
        msi_free( binary->tmpfile );
        msi_free( binary );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->cabinet_streams )
    {
        MSICABINETSTREAM *cab = LIST_ENTRY( item, MSICABINETSTREAM, entry );

        list_remove( &cab->entry );
        IStorage_Release( cab->storage );
        msi_free( cab->stream );
        msi_free( cab );
    }

    LIST_FOR_EACH_SAFE( item, cursor, &package->patches )
    {
        MSIPATCHINFO *patch = LIST_ENTRY( item, MSIPATCHINFO, entry );

        list_remove( &patch->entry );
        if (patch->delete_on_close && !DeleteFileW( patch->localfile ))
        {
            ERR("failed to delete %s (%u)\n", debugstr_w(patch->localfile), GetLastError());
        }
        msi_free_patchinfo( patch );
    }

    msi_free( package->BaseURL );
    msi_free( package->PackagePath );
    msi_free( package->ProductCode );
    msi_free( package->ActionFormat );
    msi_free( package->LastAction );
    msi_free( package->langids );

    remove_tracked_tempfiles(package);
}

static void MSI_FreePackage( MSIOBJECTHDR *arg)
{
    MSIPACKAGE *package = (MSIPACKAGE *)arg;

    msiobj_release( &package->db->hdr );
    free_package_structures(package);
    CloseHandle( package->log_file );

    if (package->delete_on_close) DeleteFileW( package->localfile );
    msi_free( package->localfile );
}

static UINT set_user_sid_prop( MSIPACKAGE *package )
{
    SID_NAME_USE use;
    LPWSTR user_name;
    LPWSTR sid_str = NULL, dom = NULL;
    DWORD size, dom_size;
    PSID psid = NULL;
    UINT r = ERROR_FUNCTION_FAILED;

    size = 0;
    GetUserNameW( NULL, &size );

    user_name = msi_alloc( (size + 1) * sizeof(WCHAR) );
    if (!user_name)
        return ERROR_OUTOFMEMORY;

    if (!GetUserNameW( user_name, &size ))
        goto done;

    size = 0;
    dom_size = 0;
    LookupAccountNameW( NULL, user_name, NULL, &size, NULL, &dom_size, &use );

    psid = msi_alloc( size );
    dom = msi_alloc( dom_size*sizeof (WCHAR) );
    if (!psid || !dom)
    {
        r = ERROR_OUTOFMEMORY;
        goto done;
    }

    if (!LookupAccountNameW( NULL, user_name, psid, &size, dom, &dom_size, &use ))
        goto done;

    if (!ConvertSidToStringSidW( psid, &sid_str ))
        goto done;

    r = msi_set_property( package->db, szUserSID, sid_str );

done:
    LocalFree( sid_str );
    msi_free( dom );
    msi_free( psid );
    msi_free( user_name );

    return r;
}

typedef struct tagLANGANDCODEPAGE
{
  WORD wLanguage;
  WORD wCodePage;
} LANGANDCODEPAGE;

static UINT msi_load_summary_properties( MSIPACKAGE *package )
{
    UINT rc;
    MSIHANDLE suminfo;
    MSIHANDLE hdb = alloc_msihandle( &package->db->hdr );
    INT count;
    DWORD len;
    LPWSTR package_code;
    static const WCHAR szPackageCode[] = {
        'P','a','c','k','a','g','e','C','o','d','e',0};

    if (!hdb) {
        ERR("Unable to allocate handle\n");
        return ERROR_OUTOFMEMORY;
    }

    rc = MsiGetSummaryInformationW( hdb, NULL, 0, &suminfo );
    MsiCloseHandle(hdb);
    if (rc != ERROR_SUCCESS)
    {
        ERR("Unable to open Summary Information\n");
        return rc;
    }

    rc = MsiSummaryInfoGetPropertyW( suminfo, PID_PAGECOUNT, NULL,
                                     &count, NULL, NULL, NULL );
    if (rc != ERROR_SUCCESS)
    {
        WARN("Unable to query page count: %d\n", rc);
        goto done;
    }

    /* load package code property */
    len = 0;
    rc = MsiSummaryInfoGetPropertyW( suminfo, PID_REVNUMBER, NULL,
                                     NULL, NULL, NULL, &len );
    if (rc != ERROR_MORE_DATA)
    {
        WARN("Unable to query revision number: %d\n", rc);
        rc = ERROR_FUNCTION_FAILED;
        goto done;
    }

    len++;
    package_code = msi_alloc( len * sizeof(WCHAR) );
    rc = MsiSummaryInfoGetPropertyW( suminfo, PID_REVNUMBER, NULL,
                                     NULL, NULL, package_code, &len );
    if (rc != ERROR_SUCCESS)
    {
        WARN("Unable to query rev number: %d\n", rc);
        goto done;
    }

    msi_set_property( package->db, szPackageCode, package_code );
    msi_free( package_code );

    /* load package attributes */
    count = 0;
    MsiSummaryInfoGetPropertyW( suminfo, PID_WORDCOUNT, NULL,
                                &count, NULL, NULL, NULL );
    package->WordCount = count;

done:
    MsiCloseHandle(suminfo);
    return rc;
}

static MSIPACKAGE *msi_alloc_package( void )
{
    MSIPACKAGE *package;

    package = alloc_msiobject( MSIHANDLETYPE_PACKAGE, sizeof (MSIPACKAGE),
                               MSI_FreePackage );
    if( package )
    {
        list_init( &package->components );
        list_init( &package->features );
        list_init( &package->files );
        list_init( &package->filepatches );
        list_init( &package->tempfiles );
        list_init( &package->folders );
        list_init( &package->subscriptions );
        list_init( &package->appids );
        list_init( &package->classes );
        list_init( &package->mimes );
        list_init( &package->extensions );
        list_init( &package->progids );
        list_init( &package->RunningActions );
        list_init( &package->sourcelist_info );
        list_init( &package->sourcelist_media );
        list_init( &package->patches );
        list_init( &package->binaries );
        list_init( &package->cabinet_streams );
    }

    return package;
}

static UINT msi_load_admin_properties(MSIPACKAGE *package)
{
#if 0
    BYTE *data;
    UINT r, sz;

    static const WCHAR stmname[] = {'A','d','m','i','n','P','r','o','p','e','r','t','i','e','s',0};

    r = read_stream_data(package->db->storage, stmname, FALSE, &data, &sz);
    if (r != ERROR_SUCCESS)
        return r;

    r = msi_parse_command_line(package, (WCHAR *)data, TRUE);

    msi_free(data);
    return r;
#endif
}

MSIPACKAGE *MSI_CreatePackage( MSIDATABASE *db, LPCWSTR base_url )
{
    static const WCHAR fmtW[] = {'%','u',0};
    MSIPACKAGE *package;
    WCHAR uilevel[11];
    UINT r;

    TRACE("%p\n", db);

    package = msi_alloc_package();
    if (package)
    {
        msiobj_addref( &db->hdr );
        package->db = db;

        package->WordCount = 0;
        package->PackagePath = strdupW( db->path );
        package->BaseURL = strdupW( base_url );

        package->ProductCode = msi_dup_property( package->db, szProductCode );
        package->script = msi_alloc_zero( sizeof(MSISCRIPT) );

        package->ui_level = gUILevel;
        sprintfW( uilevel, fmtW, gUILevel & INSTALLUILEVEL_MASK );
        msi_set_property(package->db, szUILevel, uilevel);

        r = msi_load_summary_properties( package );
        if (r != ERROR_SUCCESS)
        {
            msiobj_release( &package->hdr );
            return NULL;
        }

        if (package->WordCount & msidbSumInfoSourceTypeAdminImage)
            msi_load_admin_properties( package );

        package->log_file = INVALID_HANDLE_VALUE;
    }
    return package;
}

UINT msi_create_empty_local_file( LPWSTR path, LPCWSTR suffix )
{
    static const WCHAR szInstaller[] = {
        '\\','I','n','s','t','a','l','l','e','r','\\',0};
    static const WCHAR fmt[] = {'%','x',0};
    DWORD time, len, i, offset;
    HANDLE handle;

    time = GetTickCount();
    GetWindowsDirectoryW( path, MAX_PATH );
    strcatW( path, szInstaller );
    CreateDirectoryW( path, NULL );

    len = strlenW(path);
    for (i = 0; i < 0x10000; i++)
    {
        offset = snprintfW( path + len, MAX_PATH - len, fmt, (time + i) & 0xffff );
        memcpy( path + len + offset, suffix, (strlenW( suffix ) + 1) * sizeof(WCHAR) );
        handle = CreateFileW( path, GENERIC_WRITE, 0, NULL,
                              CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0 );
        if (handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
            break;
        }
        if (GetLastError() != ERROR_FILE_EXISTS &&
            GetLastError() != ERROR_SHARING_VIOLATION)
            return ERROR_FUNCTION_FAILED;
    }

    return ERROR_SUCCESS;
}

static UINT msi_parse_summary( MSISUMMARYINFO *si, MSIPACKAGE *package )
{
    WCHAR *template, *p, *q;
    DWORD i, count;

    package->version = msi_suminfo_get_int32( si, PID_PAGECOUNT );
    TRACE("version: %d\n", package->version);

    template = msi_suminfo_dup_string( si, PID_TEMPLATE );
    if (!template)
        return ERROR_SUCCESS; /* native accepts missing template property */

    TRACE("template: %s\n", debugstr_w(template));

    p = strchrW( template, ';' );
    if (!p)
    {
        WARN("invalid template string %s\n", debugstr_w(template));
        msi_free( template );
        return ERROR_PATCH_PACKAGE_INVALID;
    }
    *p = 0;
    if ((q = strchrW( template, ',' ))) *q = 0;
    if (!template[0] || !strcmpW( template, szIntel ))
        package->platform = PLATFORM_INTEL;
    else if (!strcmpW( template, szIntel64 ))
        package->platform = PLATFORM_INTEL64;
    else if (!strcmpW( template, szX64 ) || !strcmpW( template, szAMD64 ))
        package->platform = PLATFORM_X64;
    else if (!strcmpW( template, szARM ))
        package->platform = PLATFORM_ARM;
    else
    {
        WARN("unknown platform %s\n", debugstr_w(template));
        msi_free( template );
        return ERROR_INSTALL_PLATFORM_UNSUPPORTED;
    }
    p++;
    if (!*p)
    {
        msi_free( template );
        return ERROR_SUCCESS;
    }
    count = 1;
    for (q = p; (q = strchrW( q, ',' )); q++) count++;

    package->langids = msi_alloc( count * sizeof(LANGID) );
    if (!package->langids)
    {
        msi_free( template );
        return ERROR_OUTOFMEMORY;
    }

    i = 0;
    while (*p)
    {
        q = strchrW( p, ',' );
        if (q) *q = 0;
        package->langids[i] = atoiW( p );
        if (!q) break;
        p = q + 1;
        i++;
    }
    package->num_langids = i + 1;

    msi_free( template );
    return ERROR_SUCCESS;
}

static UINT validate_package( MSIPACKAGE *package )
{
    UINT i;

    if (package->platform == PLATFORM_INTEL64)
        return ERROR_INSTALL_PLATFORM_UNSUPPORTED;
#ifndef __arm__
    if (package->platform == PLATFORM_ARM)
        return ERROR_INSTALL_PLATFORM_UNSUPPORTED;
#endif
    if (package->platform == PLATFORM_X64)
    {
        if (!is_64bit && !is_wow64)
            return ERROR_INSTALL_PLATFORM_UNSUPPORTED;
        if (package->version < 200)
            return ERROR_INSTALL_PACKAGE_INVALID;
    }
    if (!package->num_langids)
    {
        return ERROR_SUCCESS;
    }
    for (i = 0; i < package->num_langids; i++)
    {
        LANGID langid = package->langids[i];

        if (PRIMARYLANGID( langid ) == LANG_NEUTRAL)
        {
            langid = MAKELANGID( PRIMARYLANGID( GetSystemDefaultLangID() ), SUBLANGID( langid ) );
        }
        if (SUBLANGID( langid ) == SUBLANG_NEUTRAL)
        {
            langid = MAKELANGID( PRIMARYLANGID( langid ), SUBLANGID( GetSystemDefaultLangID() ) );
        }
        if (IsValidLocale( langid, LCID_INSTALLED ))
            return ERROR_SUCCESS;
    }
    return ERROR_INSTALL_LANGUAGE_UNSUPPORTED;
}

int msi_track_tempfile( MSIPACKAGE *package, const WCHAR *path )
{
    MSITEMPFILE *temp;

    TRACE("%s\n", debugstr_w(path));

    LIST_FOR_EACH_ENTRY( temp, &package->tempfiles, MSITEMPFILE, entry )
    {
        if (!strcmpW( path, temp->Path )) return 0;
    }
    if (!(temp = msi_alloc_zero( sizeof (MSITEMPFILE) ))) return -1;
    list_add_head( &package->tempfiles, &temp->entry );
    temp->Path = strdupW( path );
    return 0;
}

static WCHAR *get_product_code( MSIDATABASE *db )
{
    static const WCHAR query[] = {
        'S','E','L','E','C','T',' ','`','V','a','l','u','e','`',' ',
        'F','R','O','M',' ','`','P','r','o','p','e','r','t','y','`',' ',
        'W','H','E','R','E',' ','`','P','r','o','p','e','r','t','y','`','=',
        '\'','P','r','o','d','u','c','t','C','o','d','e','\'',0};
    MSIQUERY *view;
    MSIRECORD *rec;
    WCHAR *ret = NULL;

    if (MSI_DatabaseOpenViewW( db, query, &view ) != ERROR_SUCCESS)
    {
        return NULL;
    }
    if (MSI_ViewExecute( view, 0 ) != ERROR_SUCCESS)
    {
        MSI_ViewClose( view );
        msiobj_release( &view->hdr );
        return NULL;
    }
    if (MSI_ViewFetch( view, &rec ) == ERROR_SUCCESS)
    {
        ret = strdupW( MSI_RecordGetString( rec, 1 ) );
        msiobj_release( &rec->hdr );
    }
    MSI_ViewClose( view );
    msiobj_release( &view->hdr );
    return ret;
}

static WCHAR *get_package_code( MSIDATABASE *db )
{
    WCHAR *ret;
    MSISUMMARYINFO *si;

    if (!(si = MSI_GetSummaryInformationW( db->storage, 0 )))
    {
        WARN("failed to load summary info\n");
        return NULL;
    }
    ret = msi_suminfo_dup_string( si, PID_REVNUMBER );
    msiobj_release( &si->hdr );
    return ret;
}

UINT MSI_OpenPackageW(LPCWSTR szPackage, MSIPACKAGE **pPackage)
{
    static const WCHAR dotmsi[] = {'.','m','s','i',0};
    MSIDATABASE *db;
    MSIPACKAGE *package;
    MSIHANDLE handle;
    LPWSTR ptr, base_url = NULL;
    UINT r;
    WCHAR localfile[MAX_PATH], cachefile[MAX_PATH];
    LPCWSTR file = szPackage;
    DWORD index = 0;
    MSISUMMARYINFO *si;
    BOOL delete_on_close = FALSE;

    TRACE("%s %p\n", debugstr_w(szPackage), pPackage);

    localfile[0] = 0;
    if( szPackage[0] == '#' )
    {
        handle = atoiW(&szPackage[1]);
        db = msihandle2msiinfo( handle, MSIHANDLETYPE_DATABASE );
        if( !db )
            return ERROR_INVALID_HANDLE;
    }
    else
    {
        r = MSI_OpenDatabaseW( file, MSIDBOPEN_TRANSACT, &db );
        if (r != ERROR_SUCCESS)
            return r;
    }
    package = MSI_CreatePackage( db, base_url );
    msi_free( base_url );
    msiobj_release( &db->hdr );
    if (!package) return ERROR_INSTALL_PACKAGE_INVALID;
    package->localfile = strdupW( localfile );
    package->delete_on_close = delete_on_close;

    si = MSI_GetSummaryInformationW( db->storage, 0 );
    if (!si)
    {
        WARN("failed to load summary info\n");
        msiobj_release( &package->hdr );
        return ERROR_INSTALL_PACKAGE_INVALID;
    }
    r = msi_parse_summary( si, package );
    msiobj_release( &si->hdr );
    if (r != ERROR_SUCCESS)
    {
        WARN("failed to parse summary info %u\n", r);
        msiobj_release( &package->hdr );
        return r;
    }
    r = validate_package( package );
    if (r != ERROR_SUCCESS)
    {
        msiobj_release( &package->hdr );
        return r;
    }
    msi_set_property( package->db, szDatabase, db->path );

    if( szPackage[0] == '#' )
        msi_set_property( package->db, szOriginalDatabase, db->path );
    else
    {
        WCHAR fullpath[MAX_PATH];

        GetFullPathNameW( szPackage, MAX_PATH, fullpath, NULL );
        msi_set_property( package->db, szOriginalDatabase, fullpath );
    }

    if (gszLogFile)
        package->log_file = CreateFileW( gszLogFile, GENERIC_WRITE, FILE_SHARE_WRITE, NULL,
                                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    *pPackage = package;
    return ERROR_SUCCESS;
}

UINT WINAPI MsiOpenPackageExW(LPCWSTR szPackage, DWORD dwOptions, MSIHANDLE *phPackage)
{
    MSIPACKAGE *package = NULL;
    UINT ret;

    TRACE("%s %08x %p\n", debugstr_w(szPackage), dwOptions, phPackage );

    if( !szPackage || !phPackage )
        return ERROR_INVALID_PARAMETER;

    if ( !*szPackage )
    {
        FIXME("Should create an empty database and package\n");
        return ERROR_FUNCTION_FAILED;
    }

    if( dwOptions )
        FIXME("dwOptions %08x not supported\n", dwOptions);

    ret = MSI_OpenPackageW( szPackage, &package );
    if( ret == ERROR_SUCCESS )
    {
        *phPackage = alloc_msihandle( &package->hdr );
        if (! *phPackage)
            ret = ERROR_NOT_ENOUGH_MEMORY;
        msiobj_release( &package->hdr );
    }

    return ret;
}

UINT WINAPI MsiOpenPackageW(LPCWSTR szPackage, MSIHANDLE *phPackage)
{
    return MsiOpenPackageExW( szPackage, 0, phPackage );
}

UINT WINAPI MsiOpenPackageExA(LPCSTR szPackage, DWORD dwOptions, MSIHANDLE *phPackage)
{
    LPWSTR szwPack = NULL;
    UINT ret;

    if( szPackage )
    {
        szwPack = strdupAtoW( szPackage );
        if( !szwPack )
            return ERROR_OUTOFMEMORY;
    }

    ret = MsiOpenPackageExW( szwPack, dwOptions, phPackage );

    msi_free( szwPack );

    return ret;
}

UINT WINAPI MsiOpenPackageA(LPCSTR szPackage, MSIHANDLE *phPackage)
{
    return MsiOpenPackageExA( szPackage, 0, phPackage );
}

MSIHANDLE WINAPI MsiGetActiveDatabase(MSIHANDLE hInstall)
{
    MSIPACKAGE *package;
    MSIHANDLE handle = 0;
    IUnknown *remote_unk;

    TRACE("(%d)\n",hInstall);

    package = msihandle2msiinfo( hInstall, MSIHANDLETYPE_PACKAGE);
    if( package)
    {
        handle = alloc_msihandle( &package->db->hdr );
        msiobj_release( &package->hdr );
    }

    return handle;
}

/* property code */

UINT WINAPI MsiSetPropertyA( MSIHANDLE hInstall, LPCSTR szName, LPCSTR szValue )
{
    LPWSTR szwName = NULL, szwValue = NULL;
    UINT r = ERROR_OUTOFMEMORY;

    szwName = strdupAtoW( szName );
    if( szName && !szwName )
        goto end;

    szwValue = strdupAtoW( szValue );
    if( szValue && !szwValue )
        goto end;

    r = MsiSetPropertyW( hInstall, szwName, szwValue);

end:
    msi_free( szwName );
    msi_free( szwValue );

    return r;
}

void msi_reset_folders( MSIPACKAGE *package, BOOL source )
{
    MSIFOLDER *folder;

    LIST_FOR_EACH_ENTRY( folder, &package->folders, MSIFOLDER, entry )
    {
        if ( source )
        {
            msi_free( folder->ResolvedSource );
            folder->ResolvedSource = NULL;
        }
        else
        {
            msi_free( folder->ResolvedTarget );
            folder->ResolvedTarget = NULL;
        }
    }
}

UINT msi_set_property( MSIDATABASE *db, LPCWSTR szName, LPCWSTR szValue )
{
    static const WCHAR insert_query[] = {
        'I','N','S','E','R','T',' ','I','N','T','O',' ',
        '`','_','P','r','o','p','e','r','t','y','`',' ',
        '(','`','_','P','r','o','p','e','r','t','y','`',',','`','V','a','l','u','e','`',')',' ',
        'V','A','L','U','E','S',' ','(','?',',','?',')',0};
    static const WCHAR update_query[] = {
        'U','P','D','A','T','E',' ','`','_','P','r','o','p','e','r','t','y','`',' ',
        'S','E','T',' ','`','V','a','l','u','e','`',' ','=',' ','?',' ','W','H','E','R','E',' ',
        '`','_','P','r','o','p','e','r','t','y','`',' ','=',' ','\'','%','s','\'',0};
    static const WCHAR delete_query[] = {
        'D','E','L','E','T','E',' ','F','R','O','M',' ',
        '`','_','P','r','o','p','e','r','t','y','`',' ','W','H','E','R','E',' ',
        '`','_','P','r','o','p','e','r','t','y','`',' ','=',' ','\'','%','s','\'',0};
    MSIQUERY *view;
    MSIRECORD *row = NULL;
    DWORD sz = 0;
    WCHAR query[1024];
    UINT rc;

    TRACE("%p %s %s\n", db, debugstr_w(szName), debugstr_w(szValue));

    if (!szName)
        return ERROR_INVALID_PARAMETER;

    /* this one is weird... */
    if (!szName[0])
        return szValue ? ERROR_FUNCTION_FAILED : ERROR_SUCCESS;

    rc = msi_get_property(db, szName, 0, &sz);
    if (!szValue || !*szValue)
    {
        sprintfW(query, delete_query, szName);
    }
    else if (rc == ERROR_MORE_DATA || rc == ERROR_SUCCESS)
    {
        sprintfW(query, update_query, szName);

        row = MSI_CreateRecord(1);
        MSI_RecordSetStringW(row, 1, szValue);
    }
    else
    {
        strcpyW(query, insert_query);

        row = MSI_CreateRecord(2);
        MSI_RecordSetStringW(row, 1, szName);
        MSI_RecordSetStringW(row, 2, szValue);
    }

    rc = MSI_DatabaseOpenViewW(db, query, &view);
    if (rc == ERROR_SUCCESS)
    {
        rc = MSI_ViewExecute(view, row);
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
    }
    if (row) msiobj_release(&row->hdr);
    return rc;
}

UINT WINAPI MsiSetPropertyW( MSIHANDLE hInstall, LPCWSTR szName, LPCWSTR szValue)
{
    MSIPACKAGE *package;
    UINT ret;

    package = msihandle2msiinfo( hInstall, MSIHANDLETYPE_PACKAGE);
    if( !package )
        return ERROR_INVALID_HANDLE;
    ret = msi_set_property( package->db, szName, szValue );
    if (ret == ERROR_SUCCESS && !strcmpW( szName, szSourceDir ))
        msi_reset_folders( package, TRUE );

    msiobj_release( &package->hdr );
    return ret;
}

static MSIRECORD *msi_get_property_row( MSIDATABASE *db, LPCWSTR name )
{
    static const WCHAR query[]= {
        'S','E','L','E','C','T',' ','`','V','a','l','u','e','`',' ',
        'F','R','O','M',' ' ,'`','_','P','r','o','p','e','r','t','y','`',' ',
        'W','H','E','R','E',' ' ,'`','_','P','r','o','p','e','r','t','y','`','=','?',0};
    MSIRECORD *rec, *row = NULL;
    MSIQUERY *view;
    UINT r;

    if (!name || !*name)
        return NULL;

    rec = MSI_CreateRecord(1);
    if (!rec)
        return NULL;

    MSI_RecordSetStringW(rec, 1, name);

    r = MSI_DatabaseOpenViewW(db, query, &view);
    if (r == ERROR_SUCCESS)
    {
        MSI_ViewExecute(view, rec);
        MSI_ViewFetch(view, &row);
        MSI_ViewClose(view);
        msiobj_release(&view->hdr);
    }
    msiobj_release(&rec->hdr);
    return row;
}

/* internal function, not compatible with MsiGetPropertyW */
UINT msi_get_property( MSIDATABASE *db, LPCWSTR szName,
                       LPWSTR szValueBuf, LPDWORD pchValueBuf )
{
    MSIRECORD *row;
    UINT rc = ERROR_FUNCTION_FAILED;

    row = msi_get_property_row( db, szName );

    if (*pchValueBuf > 0)
        szValueBuf[0] = 0;

    if (row)
    {
        rc = MSI_RecordGetStringW(row, 1, szValueBuf, pchValueBuf);
        msiobj_release(&row->hdr);
    }

    if (rc == ERROR_SUCCESS)
        TRACE("returning %s for property %s\n", debugstr_w(szValueBuf),
            debugstr_w(szName));
    else if (rc == ERROR_MORE_DATA)
        TRACE("need %d sized buffer for %s\n", *pchValueBuf,
            debugstr_w(szName));
    else
    {
        *pchValueBuf = 0;
        TRACE("property %s not found\n", debugstr_w(szName));
    }

    return rc;
}

LPWSTR msi_dup_property(MSIDATABASE *db, LPCWSTR prop)
{
    DWORD sz = 0;
    LPWSTR str;
    UINT r;

    r = msi_get_property(db, prop, NULL, &sz);
    if (r != ERROR_SUCCESS && r != ERROR_MORE_DATA)
        return NULL;

    sz++;
    str = msi_alloc(sz * sizeof(WCHAR));
    r = msi_get_property(db, prop, str, &sz);
    if (r != ERROR_SUCCESS)
    {
        msi_free(str);
        str = NULL;
    }

    return str;
}

int msi_get_property_int( MSIDATABASE *db, LPCWSTR prop, int def )
{
    LPWSTR str = msi_dup_property( db, prop );
    int val = str ? atoiW(str) : def;
    msi_free(str);
    return val;
}

static UINT MSI_GetProperty( MSIHANDLE handle, LPCWSTR name,
                             awstring *szValueBuf, LPDWORD pchValueBuf )
{
    MSIPACKAGE *package;
    MSIRECORD *row = NULL;
    UINT r = ERROR_FUNCTION_FAILED;
    LPCWSTR val = NULL;

    TRACE("%u %s %p %p\n", handle, debugstr_w(name),
          szValueBuf->str.w, pchValueBuf );

    if (!name)
        return ERROR_INVALID_PARAMETER;

    package = msihandle2msiinfo( handle, MSIHANDLETYPE_PACKAGE );
    if (!package)
        return ERROR_INVALID_HANDLE;
    row = msi_get_property_row( package->db, name );
    if (row)
        val = MSI_RecordGetString( row, 1 );

    if (!val)
        val = szEmpty;

    r = msi_strcpy_to_awstring( val, szValueBuf, pchValueBuf );

    if (row)
        msiobj_release( &row->hdr );
    msiobj_release( &package->hdr );

    return r;
}

UINT WINAPI MsiGetPropertyA( MSIHANDLE hInstall, LPCSTR szName,
                             LPSTR szValueBuf, LPDWORD pchValueBuf )
{
    awstring val;
    LPWSTR name;
    UINT r;

    val.unicode = FALSE;
    val.str.a = szValueBuf;

    name = strdupAtoW( szName );
    if (szName && !name)
        return ERROR_OUTOFMEMORY;

    r = MSI_GetProperty( hInstall, name, &val, pchValueBuf );
    msi_free( name );
    return r;
}

UINT WINAPI MsiGetPropertyW( MSIHANDLE hInstall, LPCWSTR szName,
                             LPWSTR szValueBuf, LPDWORD pchValueBuf )
{
    awstring val;

    val.unicode = TRUE;
    val.str.w = szValueBuf;

    return MSI_GetProperty( hInstall, szName, &val, pchValueBuf );
}

UINT msi_package_add_info(MSIPACKAGE *package, DWORD context, DWORD options,
                          LPCWSTR property, LPWSTR value)
{
    MSISOURCELISTINFO *info;

    LIST_FOR_EACH_ENTRY( info, &package->sourcelist_info, MSISOURCELISTINFO, entry )
    {
        if (!strcmpW( info->value, value )) return ERROR_SUCCESS;
    }

    info = msi_alloc(sizeof(MSISOURCELISTINFO));
    if (!info)
        return ERROR_OUTOFMEMORY;

    info->context = context;
    info->options = options;
    info->property = property;
    info->value = strdupW(value);
    list_add_head(&package->sourcelist_info, &info->entry);

    return ERROR_SUCCESS;
}

UINT msi_package_add_media_disk(MSIPACKAGE *package, DWORD context, DWORD options,
                                DWORD disk_id, LPWSTR volume_label, LPWSTR disk_prompt)
{
    MSIMEDIADISK *disk;

    LIST_FOR_EACH_ENTRY( disk, &package->sourcelist_media, MSIMEDIADISK, entry )
    {
        if (disk->disk_id == disk_id) return ERROR_SUCCESS;
    }

    disk = msi_alloc(sizeof(MSIMEDIADISK));
    if (!disk)
        return ERROR_OUTOFMEMORY;

    disk->context = context;
    disk->options = options;
    disk->disk_id = disk_id;
    disk->volume_label = strdupW(volume_label);
    disk->disk_prompt = strdupW(disk_prompt);
    list_add_head(&package->sourcelist_media, &disk->entry);

    return ERROR_SUCCESS;
}
