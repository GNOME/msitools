/*
 * Copyright (C) 2005 Mike McCormack for CodeWeavers
 *
 * A test program for MSI database files.
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

#include <stdint.h>
#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include <libmsi.h>
#include <objidl.h>

#include "test.h"

static const char *msifile = "winetest-suminfo.msi";
static const WCHAR msifileW[] = {
    'w','i','n','e','t','e','s','t','-','s','u','m','i','n','f','o','.','m','s','i',0 };

static void test_suminfo(void)
{
    GError *error = NULL;
    GArray *props;
    LibmsiDatabase *hdb = 0;
    LibmsiSummaryInfo *hsuminfo;
    unsigned r, count, type;
    unsigned sz;
    int val;
    uint64_t ft;
    const gchar *str;

    DeleteFile(msifile);

    /* just libmsi_database_new should not create a file */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_OPEN_CREATE, NULL);
    ok(hdb, "libmsi_database_new failed\n");

    hsuminfo = libmsi_summary_info_new(hdb, 0, NULL);
    ok(hsuminfo, "libmsi_database_get_summary_info failed\n");

    g_object_unref(hsuminfo);

    hsuminfo = libmsi_summary_info_new(hdb, 0, NULL);
    ok(hsuminfo, "libmsi_database_get_summary_info failed\n");

    props = libmsi_summary_info_get_properties (hsuminfo);
    ok(props, "getpropcount failed\n");
    ok(props->len == 0, "count should be zero\n");
    g_array_unref (props);

    type = libmsi_summary_info_get_property_type(hsuminfo, LIBMSI_PROPERTY_UUID, &error);
    ok(!error, "returned error");
    ok(type == LIBMSI_PROPERTY_TYPE_EMPTY, "should be empty\n");

    libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_TITLE, "Mike", &error);
    ok(error, "libmsi_summary_info_set_property wrong error\n");
    g_clear_error(error);

    libmsi_summary_info_set_int(hsuminfo, LIBMSI_PROPERTY_CODEPAGE, 1, &error);
    ok(error, "libmsi_summary_info_set_property wrong error\n");
    g_clear_error(error);

    g_object_unref(hsuminfo);

    /* try again with the update count set */
    hsuminfo = libmsi_summary_info_new(hdb, 1, NULL);
    ok(hsuminfo, "libmsi_database_get_summary_info failed\n");

    libmsi_summary_info_set_string(hsuminfo, 0, NULL, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(error);

    libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_CODEPAGE, NULL, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(error);

    libmsi_summary_info_set_int(hsuminfo, LIBMSI_PROPERTY_TITLE, NULL, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(error);

    libmsi_summary_info_set_int(hsuminfo, LIBMSI_PROPERTY_AUTHOR, 0, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(error);

    libmsi_summary_info_set_int(hsuminfo, LIBMSI_PROPERTY_KEYWORDS, 0, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(error);

    libmsi_summary_info_set_filetime(hsuminfo, LIBMSI_PROPERTY_COMMENTS, 0, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(error);

    libmsi_summary_info_set_int(hsuminfo, LIBMSI_PROPERTY_TEMPLATE, 0, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(error);

    libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_LASTAUTHOR, NULL, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_INVALID_PARAMETER);
    g_clear_error(error);

    libmsi_summary_info_set_int(hsuminfo, LIBMSI_PROPERTY_UUID, 0, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(error);

    libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_VERSION, NULL, &error);
    g_assert(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(error);

    libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_TITLE, "Mike", &error);
    ok(!error, "libmsi_summary_info_set_property failed\n");

    str = libmsi_summary_info_get_string(hsuminfo, LIBMSI_PROPERTY_TITLE, &error);
    ok(!error, "got error");
    ok(!strcpy(str, "Mike"));

    libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_TITLE, "JungAh", &error);
    ok(!error, "libmsi_summary_info_set_property failed\n");

    g_object_unref(hsuminfo);

    /* try again with a higher update count */
    hsuminfo = libmsi_summary_info_new(hdb, 10, NULL);
    ok(hsuminfo, "libmsi_database_get_summary_info failed\n");

    libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_TITLE, "JungAh", &error);
    ok(!error, "libmsi_summary_info_set_property failed\n");

    libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_CODEPAGE, "", &error);
    g_assert(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(&error);

    libmsi_summary_info_set_int(hsuminfo, LIBMSI_PROPERTY_CODEPAGE, 1, &error);
    ok(!error, "libmsi_summary_info_set_property failed\n");

    libmsi_summary_info_set_int(hsuminfo, LIBMSI_PROPERTY_CODEPAGE, 1, &error);
    ok(!error, "libmsi_summary_info_set_property failed\n");

    libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_AUTHOR, "Mike", &error);
    ok(!error, "libmsi_summary_info_set_property failed\n");

    r = libmsi_summary_info_persist(hsuminfo);
    ok(r, "libmsi_summary_info_persist failed\n");

    libmsi_database_commit(hdb);

    g_object_unref(hsuminfo);

    g_object_unref(hdb);

    /* reread, non-zero update count */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_OPEN_TRANSACT, NULL);
    ok(hdb, "libmsi_database_new failed\n");

    hsuminfo = libmsi_summary_info_new(hdb, 1, NULL);
    ok(hsuminfo, "libmsi_database_get_summary_info failed\n");

    r = libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_AUTHOR, "Mike", error);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_summary_info_set_property wrong error\n");

    r = libmsi_summary_info_persist(hsuminfo);
    ok(r, "libmsi_summary_info_persist failed %u\n", r);

    g_object_unref(hsuminfo);

    /* now with zero update count */
    g_object_unref(hdb);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_OPEN_READONLY, NULL);
    ok(hdb, "libmsi_database_new failed\n");

    hsuminfo = libmsi_summary_info_new(hdb, 0, NULL);
    ok(hsuminfo, "libmsi_database_get_summary_info failed\n");

    libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_AUTHOR, "Mike", &error);
    todo_wine ok(error, "libmsi_summary_info_set_property wrong error\n");
    g_clear_error(&error);

    r = libmsi_summary_info_persist(hsuminfo);
    ok(!r, "libmsi_summary_info_persist wrong error %u\n", r);

    g_object_unref(hsuminfo);

    g_object_unref(hdb);

    r = DeleteFile(msifile);
    ok(r, "DeleteFile failed\n");
}

static const WCHAR tb[] = { 0x4840, 0x3f7f, 0x4164, 0x422f, 0x4836, 0 }; /* _Tables */
static const WCHAR sd[] = { 0x4840, 0x3f3f, 0x4577, 0x446c, 0x3b6a, 0x45e4, 0x4824, 0 }; /* _StringData */
static const WCHAR sp[] = { 0x4840, 0x3f3f, 0x4577, 0x446c, 0x3e6a, 0x44b2, 0x482f, 0 }; /* _StringPool */

#define LOSE_CONST(x) ((char *)(uintptr_t)(x))

static void test_create_database_binary(void)
{
    static const CLSID CLSID_MsiDatabase =
        { 0xc1084, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
    static const CLSID IID_IPropertySetStorage =
        { 0x13a, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
    static const CLSID FMTID_SummaryInformation =
        { 0xf29f85e0, 0x4ff9, 0x1068, {0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9}};
    unsigned mode = STGM_CREATE | STGM_READWRITE | STGM_DIRECT | STGM_SHARE_EXCLUSIVE;
    IPropertySetStorage *pss = NULL;
    IPropertyStorage *ps = NULL;
    IStorage *stg = NULL;
    IStream *stm = NULL;
    HRESULT r;
    PROPSPEC propspec[10];
    PROPVARIANT propvar[10];
    uint16_t data[2] = { 0, 0 };

    r = StgCreateDocfile( msifileW, mode, 0, &stg );
    ok( r == S_OK, "failed to create database\n");

    r = IStorage_SetClass( stg, &CLSID_MsiDatabase );
    ok( r == S_OK, "failed to set clsid\n");

    /* create the _StringData stream */
    r = IStorage_CreateStream( stg, sd, STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm );
    ok( r == S_OK, "failed to create stream\n");

    IStream_Release( stm );

    /* create the _StringPool stream */
    r = IStorage_CreateStream( stg, sp, STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm );
    ok( r == S_OK, "failed to create stream\n");

    r = IStream_Write( stm, data, sizeof data, NULL );
    ok( r == S_OK, "failed to write stream\n");

    IStream_Release( stm );

    /* create the _Tables stream */
    r = IStorage_CreateStream( stg, tb, STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm );
    ok( r == S_OK, "failed to create stream\n");

    IStream_Release( stm );

    r = IStorage_QueryInterface( stg, &IID_IPropertySetStorage, (void**) &pss );
    ok( r == S_OK, "failed to set clsid\n");

    r = IPropertySetStorage_Create( pss, &FMTID_SummaryInformation, NULL, 0, mode, &ps );
    ok( r == S_OK, "failed to create property set\n");

    r = IPropertyStorage_SetClass( ps, &FMTID_SummaryInformation );
    ok( r == S_OK, "failed to set class\n");

    propspec[0].ulKind = PRSPEC_PROPID;
    propspec[0].propid = LIBMSI_PROPERTY_TITLE;
    propvar[0].vt = LIBMSI_PROPERTY_TYPE_STRING;
    propvar[0].pszVal = LOSE_CONST("test title");

    propspec[1].ulKind = PRSPEC_PROPID;
    propspec[1].propid = LIBMSI_PROPERTY_SUBJECT;
    propvar[1].vt = LIBMSI_PROPERTY_TYPE_STRING;
    propvar[1].pszVal = LOSE_CONST("msi suminfo / property storage test");

    propspec[2].ulKind = PRSPEC_PROPID;
    propspec[2].propid = LIBMSI_PROPERTY_AUTHOR;
    propvar[2].vt = LIBMSI_PROPERTY_TYPE_STRING;
    propvar[2].pszVal = LOSE_CONST("mike_m");

    propspec[3].ulKind = PRSPEC_PROPID;
    propspec[3].propid = LIBMSI_PROPERTY_TEMPLATE;
    propvar[3].vt = LIBMSI_PROPERTY_TYPE_STRING;
    propvar[3].pszVal = LOSE_CONST(";1033");  /* actually the string table's codepage */

    propspec[4].ulKind = PRSPEC_PROPID;
    propspec[4].propid = LIBMSI_PROPERTY_UUID;
    propvar[4].vt = LIBMSI_PROPERTY_TYPE_STRING;
    propvar[4].pszVal = LOSE_CONST("{913B8D18-FBB6-4CAC-A239-C74C11E3FA74}");

    propspec[5].ulKind = PRSPEC_PROPID;
    propspec[5].propid = LIBMSI_PROPERTY_VERSION;
    propvar[5].vt = LIBMSI_PROPERTY_TYPE_INT;
    propvar[5].lVal = 100;

    propspec[6].ulKind = PRSPEC_PROPID;
    propspec[6].propid = LIBMSI_PROPERTY_SOURCE;
    propvar[6].vt = LIBMSI_PROPERTY_TYPE_INT;
    propvar[6].lVal = 0;

    /* MSDN says that LIBMSI_PROPERTY_LASTPRINTED should be a VT_FILETIME... */
    propspec[7].ulKind = PRSPEC_PROPID;
    propspec[7].propid = LIBMSI_PROPERTY_LASTPRINTED;
    propvar[7].vt = LIBMSI_PROPERTY_TYPE_STRING;
    propvar[7].pszVal = LOSE_CONST("7/1/1999 5:17");

    r = IPropertyStorage_WriteMultiple( ps, 8, propspec, propvar, PID_FIRST_USABLE );
    ok( r == S_OK, "failed to write properties\n");

    IPropertyStorage_Commit( ps, STGC_DEFAULT );

    IPropertyStorage_Release( ps );
    IPropertySetStorage_Release( pss );

    IStorage_Commit( stg, STGC_DEFAULT );
    IStorage_Release( stg );
}

static void test_summary_binary(void)
{
    GError *error = NULL;
    LibmsiDatabase *hdb = 0;
    LibmsiSummaryInfo *hsuminfo = 0;
    unsigned r, type, count;
    int ival;
    const gchar *str;

    DeleteFile( msifile );

    test_create_database_binary();

    ok( INVALID_FILE_ATTRIBUTES != GetFileAttributes(msifile), "file doesn't exist!\n");

    /* just libmsi_database_new should not create a file */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_OPEN_READONLY, NULL);
    ok(hdb, "libmsi_database_new failed\n");

    hsuminfo = libmsi_summary_info_new(hdb, 0, NULL);
    ok(hsuminfo, "libmsi_database_get_summary_info failed\n");

    /*
     * Check what reading LIBMSI_PROPERTY_LASTPRINTED does...
     * The string value is written to the msi file
     * but it appears that we're not allowed to read it back again.
     * We can still read its type though...?
     */
    str = libmsi_summary_info_get_string(hsuminfo, LIBMSI_PROPERTY_LASTPRINTED, error);
    ok(!error);
    ok(!strcmp(str, "") || !strcmp(str, "7"),
        "Expected empty string or \"7\", got \"%s\"\n", str);

    ival = libmsi_summary_info_get_int(hsuminfo, LIBMSI_PROPERTY_SOURCE, &error);
    ok(!error, "libmsi_summary_info_get_property failed\n");
    todo_wine ok( ival == 0, "value incorrect\n");

    /* looks like msi adds some of its own values in here */
    props = libmsi_summary_info_get_properties (hsuminfo);
    ok(props, "getpropcount failed\n");
    todo_wine ok(props->len == 10, "prop count incorrect\n");
    g_array_unref (props);

    libmsi_summary_info_set_string(hsuminfo, LIBMSI_PROPERTY_TITLE, "Mike", &error);
    ok(error, "libmsi_summary_info_set_property failed\n");
    g_clear_error(&error);

    r = libmsi_summary_info_persist( hsuminfo );
    ok(!r, "libmsi_summary_info_persist failed %u\n", r);

    g_object_unref( hsuminfo );
    g_object_unref( hdb );

    DeleteFile( msifile );
}

int main()
{
#if !GLIB_CHECK_VERSION(2,35,1)
    g_type_init ();
#endif

    test_suminfo();
    test_summary_binary();
}
