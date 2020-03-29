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

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#include <objidl.h>
#endif

#include <libmsi.h>
#include <glib/gstdio.h>

#include "test.h"

#ifndef _WIN32
typedef uint16_t WCHAR;
#define O_BINARY 0
#endif

static const char *msifile = "winetest-db.msi";
static const char *msifile2 = "winetst2-db.msi";
static const char *mstfile = "winetst-db.mst";

static const WCHAR msifileW[] = {'w','i','n','e','t','e','s','t','-','d','b','.','m','s','i',0};


static void test_msidatabase(void)
{
    LibmsiDatabase *hdb = 0;
    LibmsiDatabase *hdb2 = 0;
    unsigned res;

    unlink(msifile);

    hdb = libmsi_database_new( msifile, LIBMSI_DB_FLAGS_TRANSACT, msifile2, NULL);
    ok(!hdb, "expected failure\n");

    /* create an empty database */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL );
    ok(hdb , "Failed to create database\n" );

    res = libmsi_database_commit(hdb, NULL);
    ok(res, "Failed to commit database\n");

    ok( -1 != access( msifile, F_OK ), "database should exist\n");

    g_object_unref( hdb );

    hdb2 = libmsi_database_new( msifile, LIBMSI_DB_FLAGS_TRANSACT, msifile2, NULL );
    ok(hdb2 , "Failed to open database\n" );

    res = libmsi_database_commit(hdb2, NULL);
    ok(res , "Failed to commit database\n");

    ok( -1 != access( msifile2, F_OK ), "database should exist\n");

    g_object_unref( hdb2 );

    hdb2 = libmsi_database_new( msifile, LIBMSI_DB_FLAGS_TRANSACT, msifile2, NULL );
    ok( hdb2 , "Failed to open database\n" );

    g_object_unref( hdb2 );

    ok( -1 == access( msifile2, F_OK ), "uncommitted database should not exist\n");

    hdb2 = libmsi_database_new( msifile, LIBMSI_DB_FLAGS_TRANSACT, msifile2, NULL );
    ok( hdb2 , "Failed to close database\n" );

    res = libmsi_database_commit(hdb2, NULL);
    ok(res, "Failed to commit database\n");

    g_object_unref( hdb2 );

    ok( -1 != access( msifile2, F_OK ), "committed database should exist\n");

    hdb = libmsi_database_new( msifile, LIBMSI_DB_FLAGS_READONLY, NULL, NULL );
    ok(hdb , "Failed to open database\n" );

    res = libmsi_database_commit(hdb, NULL);
    ok(res, "Failed to commit database\n");

    g_object_unref( hdb );

    hdb = libmsi_database_new( msifile, LIBMSI_DB_FLAGS_TRANSACT, NULL, NULL );
    ok(hdb , "Failed to open database\n" );

    g_object_unref( hdb );
    ok( -1 != access( msifile, F_OK ), "database should exist\n");

    unlink( msifile );

    /* LIBMSI_DB_FLAGS_CREATE deletes the database if MsiCommitDatabase isn't called */
    hdb = libmsi_database_new( msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL );
    ok(hdb , "Failed to open database\n" );

    g_object_unref( hdb );

    ok( -1 == access( msifile, F_OK ), "database should not exist\n");

    hdb = libmsi_database_new( msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL );
    ok(hdb , "Failed to open database\n" );

    res = libmsi_database_commit(hdb, NULL);
    ok(res, "Failed to commit database\n", res);

    ok( -1 != access( msifile, F_OK ), "database should exist\n");

    g_object_unref( hdb );

    res = unlink( msifile2 );
    ok( res == 0, "Failed to delete database\n" );

    res = unlink( msifile );
    ok( res == 0, "Failed to delete database\n" );
}

int do_query(LibmsiDatabase *hdb, const char *sql, LibmsiRecord **rec)
{
    GError *error = NULL;
    LibmsiQuery *hquery = 0;
    int ret = LIBMSI_RESULT_SUCCESS;

    /* open a select query */
    hquery = libmsi_query_new(hdb, sql, &error);
    if (error)
        goto error;
    if (!libmsi_query_execute(hquery, 0, &error))
        goto error;
    *rec = libmsi_query_fetch(hquery, &error);
    if (error)
        goto error;

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);
    return ret;

error:
    ret = error->code;
    g_clear_error(&error);
    return ret;
}

static unsigned run_query( LibmsiDatabase *hdb, LibmsiRecord *hrec, const char *sql )
{
    GError *error = NULL;
    LibmsiQuery *hquery = 0;
    unsigned r = LIBMSI_RESULT_SUCCESS;

    hquery = libmsi_query_new(hdb, sql, &error);
    if (error)
        goto end;

    if (!libmsi_query_execute(hquery, hrec, &error) ||
        !libmsi_query_close(hquery, &error))
        goto end;

end:
    if (error)
        r = error->code;
    g_clear_error(&error);
    if (hquery)
        g_object_unref(hquery);

    return r;
}

static unsigned create_component_table( LibmsiDatabase *hdb )
{
    return run_query( hdb, 0,
            "CREATE TABLE `Component` ( "
            "`Component` CHAR(72) NOT NULL, "
            "`ComponentId` CHAR(38), "
            "`Directory_` CHAR(72) NOT NULL, "
            "`Attributes` SHORT NOT NULL, "
            "`Condition` CHAR(255), "
            "`KeyPath` CHAR(72) "
            "PRIMARY KEY `Component`)" );
}

static unsigned create_custom_action_table( LibmsiDatabase *hdb )
{
    return run_query( hdb, 0,
            "CREATE TABLE `CustomAction` ( "
            "`Action` CHAR(72) NOT NULL, "
            "`Type` SHORT NOT NULL, "
            "`Source` CHAR(72), "
            "`Target` CHAR(255) "
            "PRIMARY KEY `Action`)" );
}

static unsigned create_directory_table( LibmsiDatabase *hdb )
{
    return run_query( hdb, 0,
            "CREATE TABLE `Directory` ( "
            "`Directory` CHAR(255) NOT NULL, "
            "`Directory_Parent` CHAR(255), "
            "`DefaultDir` CHAR(255) NOT NULL "
            "PRIMARY KEY `Directory`)" );
}

static unsigned create_feature_components_table( LibmsiDatabase *hdb )
{
    return run_query( hdb, 0,
            "CREATE TABLE `FeatureComponents` ( "
            "`Feature_` CHAR(38) NOT NULL, "
            "`Component_` CHAR(72) NOT NULL "
            "PRIMARY KEY `Feature_`, `Component_` )" );
}

static unsigned create_std_dlls_table( LibmsiDatabase *hdb )
{
    return run_query( hdb, 0,
            "CREATE TABLE `StdDlls` ( "
            "`File` CHAR(255) NOT NULL, "
            "`Binary_` CHAR(72) NOT NULL "
            "PRIMARY KEY `File` )" );
}

static unsigned create_binary_table( LibmsiDatabase *hdb )
{
    return run_query( hdb, 0,
            "CREATE TABLE `Binary` ( "
            "`Name` CHAR(72) NOT NULL, "
            "`Data` CHAR(72) NOT NULL "
            "PRIMARY KEY `Name` )" );
}

#define make_add_entry(type, qtext) \
    static unsigned add##_##type##_##entry( LibmsiDatabase *hdb, const char *values ) \
    { \
        char insert[] = qtext; \
        char *sql; \
        unsigned sz, r; \
        sz = strlen(values) + sizeof insert; \
        sql = malloc(sz); \
        sprintf(sql,insert,values); \
        r = run_query( hdb, 0, sql ); \
        free(sql); \
        return r; \
    }

make_add_entry(component,
               "INSERT INTO `Component`  "
               "(`Component`, `ComponentId`, `Directory_`, "
               "`Attributes`, `Condition`, `KeyPath`) VALUES( %s )")

make_add_entry(custom_action,
               "INSERT INTO `CustomAction`  "
               "(`Action`, `Type`, `Source`, `Target`) VALUES( %s )")

make_add_entry(feature_components,
               "INSERT INTO `FeatureComponents` "
               "(`Feature_`, `Component_`) VALUES( %s )")

make_add_entry(std_dlls,
               "INSERT INTO `StdDlls` (`File`, `Binary_`) VALUES( %s )")

make_add_entry(binary,
               "INSERT INTO `Binary` (`Name`, `Data`) VALUES( %s )")

static void query_check_no_more(LibmsiQuery *query)
{
    GError *error = NULL;
    LibmsiRecord *hrec;

    hrec = libmsi_query_fetch(query, &error);
    g_assert_no_error(error);
    ok(hrec == NULL, "hrec should be null\n");
}

static void test_msiinsert(void)
{
    LibmsiDatabase *hdb = 0;
    LibmsiQuery *hquery = 0;
    LibmsiQuery *hquery2 = 0;
    LibmsiRecord *hrec = 0;
    gchar *str;
    unsigned r;
    const char *sql;
    char buf[80];
    unsigned sz;
    GError *error = NULL;

    unlink(msifile);

    /* just libmsi_database_open should not create a file */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "libmsi_database_new failed\n");

    /* create a table */
    sql = "CREATE TABLE `phone` ( "
            "`id` INT, `name` CHAR(32), `number` CHAR(32) "
            "PRIMARY KEY `id`)";
    hquery = libmsi_query_new(hdb, sql, &error);
    ok(hquery, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");
    r = libmsi_query_close(hquery, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(hquery);

    sql = "SELECT * FROM phone WHERE number = '8675309'";
    hquery2 = libmsi_query_new(hdb, sql, &error);
    ok(hquery2, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(hquery2, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");
    query_check_no_more(hquery2);

    /* insert a value into it */
    sql = "INSERT INTO `phone` ( `id`, `name`, `number` )"
        "VALUES('1', 'Abe', '8675309')";
    hquery = libmsi_query_new(hdb, sql, &error);
    ok(hquery, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");
    r = libmsi_query_close(hquery, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(hquery);

    query_check_no_more(hquery2);
    r = libmsi_query_execute(hquery2, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");
    hrec = libmsi_query_fetch(hquery2, NULL);
    ok(hrec, "libmsi_query_fetch failed\n");

    g_object_unref(hrec);
    r = libmsi_query_close(hquery2, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(hquery2);

    sql = "SELECT * FROM `phone` WHERE `id` = 1";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_query_fetch failed\n");

    /* check the record contains what we put in it */
    r = libmsi_record_get_field_count(hrec);
    ok(r == 3, "record count wrong\n");

    r = libmsi_record_is_null(hrec, 0);
    ok(r == true, "Expected true, got %d\n", r);

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 1, "field 1 contents wrong\n");

    check_record_string (hrec, 2, "Abe");
    check_record_string (hrec, 3, "8675309");

    g_object_unref(hrec);

    /* open a select query */
    hrec = NULL;
    sql = "SELECT * FROM `phone` WHERE `id` >= 10";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_query_fetch failed\n");
    ok(hrec == NULL, "hrec should be null\n");

    sql = "SELECT * FROM `phone` WHERE `id` < 0";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_query_fetch failed\n");
    ok(hrec == NULL, "hrec should be null\n");

    sql = "SELECT * FROM `phone` WHERE `id` <= 0";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_query_fetch failed\n");
    ok(hrec == NULL, "hrec should be null\n");

    sql = "SELECT * FROM `phone` WHERE `id` <> 1";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_query_fetch failed\n");
    ok(hrec == NULL, "hrec should be null\n");

    sql = "SELECT * FROM `phone` WHERE `id` > 10";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_query_fetch failed\n");
    ok(hrec == NULL, "hrec should be null\n");

    /* now try a few bad INSERT xqueries */
    sql = "INSERT INTO `phone` ( `id`, `name`, `number` )"
        "VALUES(?, ?)";
    hquery = libmsi_query_new(hdb, sql, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_BAD_QUERY_SYNTAX);
    g_clear_error(&error);

    /* construct a record to insert */
    hrec = libmsi_record_new(4);
    r = libmsi_record_set_int(hrec, 1, 2);
    ok(r, "libmsi_record_set_int failed\n");
    r = libmsi_record_set_string(hrec, 2, "Adam");
    ok(r, "libmsi_record_set_string failed\n");
    r = libmsi_record_set_string(hrec, 3, "96905305");
    ok(r, "libmsi_record_set_string failed\n");

    /* insert another value, using a record and wildcards */
    sql = "INSERT INTO `phone` ( `id`, `name`, `number` )"
        "VALUES(?, ?, ?)";
    hquery = libmsi_query_new(hdb, sql, &error);
    ok(hquery, "libmsi_database_open_query failed\n");

    r = libmsi_query_execute(hquery, hrec, NULL);
    ok(r, "libmsi_query_execute failed\n");
    r = libmsi_query_close(hquery, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(hquery);
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(0, NULL);
    ok(!hrec, "libmsi_query_fetch must fail\n");

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "libmsi_database_commit failed\n");

    g_object_unref(hdb);

    r = unlink(msifile);
    ok(r == 0, "file didn't exist after commit\n");
}

static unsigned try_query_param( LibmsiDatabase *hdb, const char *szQuery, LibmsiRecord *hrec )
{
    GError *error = NULL;
    LibmsiQuery *htab = 0;
    unsigned res = LIBMSI_RESULT_SUCCESS;

    htab = libmsi_query_new( hdb, szQuery, &error );
    if(!htab)
        goto end;

    if (!libmsi_query_execute (htab, hrec, &error))
        goto end;

    if (!libmsi_query_close (htab, &error))
        goto end;

 end:
    if (htab)
        g_object_unref( htab );
    if (error)
        res = error->code;
    g_clear_error(&error);
    return res;
}

static unsigned try_query( LibmsiDatabase *hdb, const char *szQuery )
{
    return try_query_param( hdb, szQuery, 0 );
}

static unsigned try_insert_query( LibmsiDatabase *hdb, const char *szQuery )
{
    LibmsiRecord *hrec = 0;
    unsigned r;

    hrec = libmsi_record_new( 1 );
    libmsi_record_set_string( hrec, 1, "Hello");

    r = try_query_param( hdb, szQuery, hrec );

    g_object_unref( hrec );
    return r;
}

static void test_msibadqueries(void)
{
    LibmsiDatabase *hdb = 0;
    unsigned r;

    unlink(msifile);

    /* just libmsi_database_open should not create a file */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "libmsi_database_open failed\n");

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "Failed to commit database\n");

    g_object_unref( hdb );

    /* open it readonly */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_READONLY, NULL, NULL );
    ok(hdb , "Failed to open database r/o\n");

    /* add a table to it */
    r = try_query( hdb, "select * from _Tables");
    ok(r == LIBMSI_RESULT_SUCCESS , "query 1 failed\n");

    g_object_unref( hdb );

    /* open it read/write */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_TRANSACT, NULL, NULL );
    ok(hdb , "Failed to open database r/w\n");

    /* a bunch of test queries that fail with the native MSI */

    r = try_query( hdb, "CREATE TABLE");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2a return code\n");

    r = try_query( hdb, "CREATE TABLE `a`");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2b return code\n");

    r = try_query( hdb, "CREATE TABLE `a` ()");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2c return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b`)");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2d return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(72) )");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2e return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(72) NOT NULL)");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2f return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(72) NOT NULL PRIMARY)");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2g return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(72) NOT NULL PRIMARY KEY)");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2h return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(72) NOT NULL PRIMARY KEY)");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2i return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(72) NOT NULL PRIMARY KEY 'b')");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2j return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(72) NOT NULL PRIMARY KEY `b')");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2k return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(72) NOT NULL PRIMARY KEY `b')");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2l return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHA(72) NOT NULL PRIMARY KEY `b`)");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2m return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(-1) NOT NULL PRIMARY KEY `b`)");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2n return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(720) NOT NULL PRIMARY KEY `b`)");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2o return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(72) NOT NULL KEY `b`)");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2p return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`` CHAR(72) NOT NULL PRIMARY KEY `b`)");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "invalid query 2p return code\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(72) NOT NULL PRIMARY KEY `b`)");
    ok(r == LIBMSI_RESULT_SUCCESS , "valid query 2z failed\n");

    r = try_query( hdb, "CREATE TABLE `a` (`b` CHAR(72) NOT NULL PRIMARY KEY `b`)");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX , "created same table again\n");

    r = try_query( hdb, "CREATE TABLE `aa` (`b` CHAR(72) NOT NULL, `c` "
          "CHAR(72), `d` CHAR(255) NOT NULL LOCALIZABLE PRIMARY KEY `b`)");
    ok(r == LIBMSI_RESULT_SUCCESS , "query 4 failed\n");

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "Failed to commit database after write\n");

    r = try_query( hdb, "CREATE TABLE `blah` (`foo` CHAR(72) NOT NULL "
                          "PRIMARY KEY `foo`)");
    ok(r == LIBMSI_RESULT_SUCCESS , "query 4 failed\n");

    r = try_insert_query( hdb, "insert into a  ( `b` ) VALUES ( ? )");
    ok(r == LIBMSI_RESULT_SUCCESS , "failed to insert record in db\n");

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "Failed to commit database after write\n");

    r = try_query( hdb, "CREATE TABLE `boo` (`foo` CHAR(72) NOT NULL "
                          "PRIMARY KEY `ba`)");
    ok(r != LIBMSI_RESULT_SUCCESS , "query 5 succeeded\n");

    r = try_query( hdb,"CREATE TABLE `bee` (`foo` CHAR(72) NOT NULL )");
    ok(r != LIBMSI_RESULT_SUCCESS , "query 6 succeeded\n");

    r = try_query( hdb, "CREATE TABLE `temp` (`t` CHAR(72) NOT NULL "
                          "PRIMARY KEY `t`)");
    ok(r == LIBMSI_RESULT_SUCCESS , "query 7 failed\n");

    r = try_query( hdb, "CREATE TABLE `c` (`b` CHAR NOT NULL PRIMARY KEY `b`)");
    ok(r == LIBMSI_RESULT_SUCCESS , "query 8 failed\n");

    r = try_query( hdb, "select * from c");
    ok(r == LIBMSI_RESULT_SUCCESS , "query failed\n");

    r = try_query( hdb, "select * from c where b = 'x");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed\n");

    r = try_query( hdb, "select * from c where b = 'x'");
    ok(r == LIBMSI_RESULT_SUCCESS, "query failed\n");

    r = try_query( hdb, "select * from 'c'");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed\n");

    r = try_query( hdb, "select * from ''");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed\n");

    r = try_query( hdb, "select * from c where b = x");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed\n");

    r = try_query( hdb, "select * from c where b = \"x\"");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed\n");

    r = try_query( hdb, "select * from c where b = 'x'");
    ok(r == LIBMSI_RESULT_SUCCESS, "query failed\n");

    r = try_query( hdb, "select * from c where b = '\"x'");
    ok(r == LIBMSI_RESULT_SUCCESS, "query failed\n");

    if (0)  /* FIXME: this query causes trouble with other tests */
    {
        r = try_query( hdb, "select * from c where b = '\\\'x'");
        ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed\n");
    }

    r = try_query( hdb, "select * from 'c'");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed\n");

    r = try_query( hdb, "select `c`.`b` from `c` order by `c`.`order`");
    ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %u\n", r );

    r = try_query( hdb, "select `c`.b` from `c`");
    ok( r == LIBMSI_RESULT_SUCCESS, "query failed: %u\n", r );

    r = try_query( hdb, "select `c`.`b from `c`");
    ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %u\n", r );

    r = try_query( hdb, "select `c`.b from `c`");
    ok( r == LIBMSI_RESULT_SUCCESS, "query failed: %u\n", r );

    r = try_query( hdb, "select `c.`b` from `c`");
    ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %u\n", r );

    r = try_query( hdb, "select c`.`b` from `c`");
    ok( r == LIBMSI_RESULT_SUCCESS, "query failed: %u\n", r );

    r = try_query( hdb, "select c.`b` from `c`");
    ok( r == LIBMSI_RESULT_SUCCESS, "query failed: %u\n", r );

    r = try_query( hdb, "select `c`.`b` from c`");
    ok( r == LIBMSI_RESULT_SUCCESS, "query failed: %u\n", r );

    r = try_query( hdb, "select `c`.`b` from `c");
    ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %u\n", r );

    r = try_query( hdb, "select `c`.`b` from c");
    ok( r == LIBMSI_RESULT_SUCCESS, "query failed: %u\n", r );

    r = try_query( hdb, "CREATE TABLE `\5a` (`b` CHAR NOT NULL PRIMARY KEY `b`)" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "SELECT * FROM \5a" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "CREATE TABLE `a\5` (`b` CHAR NOT NULL PRIMARY KEY `b`)" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "SELECT * FROM a\5" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "CREATE TABLE `-a` (`b` CHAR NOT NULL PRIMARY KEY `b`)" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "SELECT * FROM -a" );
    todo_wine ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "CREATE TABLE `a-` (`b` CHAR NOT NULL PRIMARY KEY `b`)" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "SELECT * FROM a-" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    g_object_unref( hdb );

    r = unlink( msifile );
    ok(r == 0, "file didn't exist after commit\n");
}

static LibmsiDatabase *create_db(void)
{
    LibmsiDatabase *hdb = 0;
    unsigned res;

    unlink(msifile);

    /* create an empty database */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL );
    ok(hdb , "Failed to create database\n" );

    res = libmsi_database_commit(hdb, NULL);
    ok(res, "Failed to commit database\n");

    return hdb;
}

static void test_getcolinfo(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *hquery = 0;
    LibmsiRecord *rec = 0;
    unsigned r;
    unsigned sz;
    char buffer[0x20];

    /* create an empty db */
    hdb = create_db();
    ok( hdb, "failed to create db\n");

    /* tables should be present */
    hquery = libmsi_query_new(hdb, "select * from _Tables", NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query\n");

    /* check that NAMES works */
    rec = libmsi_query_get_column_info( hquery, LIBMSI_COL_INFO_NAMES, NULL );
    ok(rec, "failed to get names\n");

    check_record_string (rec, 1, "Name");
    g_object_unref( rec );

    /* check that TYPES works */
    rec = libmsi_query_get_column_info( hquery, LIBMSI_COL_INFO_TYPES, NULL );
    ok(rec, "failed to get names\n");

    check_record_string (rec, 1, "s64");
    g_object_unref( rec );

    /* check that invalid values fail */
    rec = libmsi_query_get_column_info( hquery, 100, NULL);
    ok(!rec, "returned a record\n");

    rec = libmsi_query_get_column_info( 0, LIBMSI_COL_INFO_TYPES, NULL);
    ok(!rec, "wrong return value\n");

    r = libmsi_query_close(hquery, NULL);
    ok(r, "failed to close query\n");
    g_object_unref(hquery);
    g_object_unref(hdb);
}

static LibmsiRecord *get_column_info(LibmsiDatabase *hdb, const char *sql, LibmsiColInfo type)
{
    LibmsiQuery *hquery = 0;
    LibmsiRecord *rec = 0;
    unsigned r;

    hquery = libmsi_query_new(hdb, sql, NULL);
    if(!hquery)
        return NULL;

    r = libmsi_query_execute(hquery, 0, NULL);
    if( r )
    {
        rec = libmsi_query_get_column_info( hquery, type, NULL );
    }
    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);
    return rec;
}

static unsigned get_columns_table_type(LibmsiDatabase *hdb, const char *table, unsigned field)
{
    GError *error = NULL;
    LibmsiQuery *hquery = 0;
    LibmsiRecord *rec = 0;
    unsigned r, type = 0;
    char sql[0x100];

    sprintf(sql, "select * from `_Columns` where  `Table` = '%s'", table );

    hquery = libmsi_query_new(hdb, sql, NULL);
    if (!hquery)
        return type;

    r = libmsi_query_execute(hquery, 0, NULL);
    if( r )
    {
        while (1)
        {
            rec = libmsi_query_fetch(hquery, NULL);
            if (!rec)
                break;
            r = libmsi_record_get_int( rec, 2 );
            if (r == field)
                type = libmsi_record_get_int( rec, 4 );
            g_object_unref( rec );
        }
    }
    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);
    return type;
}

static bool check_record( LibmsiRecord *rec, unsigned field, const char *val )
{
    bool result;
    gchar *str;

    str = libmsi_record_get_string(rec, field);
    result = g_strcmp0(val, str) == 0;
    g_free(str);

    return result;
}

static void test_querygetcolumninfo(void)
{
    LibmsiDatabase *hdb = 0;
    LibmsiRecord *rec;
    unsigned r;

    hdb = create_db();
    ok( hdb, "failed to create db\n");

    r = run_query( hdb, 0,
            "CREATE TABLE `Properties` "
            "( `Property` CHAR(255), "
	    "  `Value` CHAR(1), "
	    "  `Intvalue` INT, "
	    "  `Integervalue` INTEGER, "
	    "  `Shortvalue` SHORT, "
	    "  `Longvalue` LONG, "
	    "  `Longcharvalue` LONGCHAR "
	    "  PRIMARY KEY `Property`)" );
    ok( r == LIBMSI_RESULT_SUCCESS , "Failed to create table\n" );

    /* check the column types */
    rec = get_column_info( hdb, "select * from `Properties`", LIBMSI_COL_INFO_TYPES );
    ok( rec, "failed to get column info record\n" );

    ok( check_record( rec, 1, "S255"), "wrong record type\n");
    ok( check_record( rec, 2, "S1"), "wrong record type\n");
    ok( check_record( rec, 3, "I2"), "wrong record type\n");
    ok( check_record( rec, 4, "I2"), "wrong record type\n");
    ok( check_record( rec, 5, "I2"), "wrong record type\n");
    ok( check_record( rec, 6, "I4"), "wrong record type\n");
    ok( check_record( rec, 7, "S0"), "wrong record type\n");

    g_object_unref( rec );

    /* check the type in _Columns */
    ok( 0x3dff == get_columns_table_type(hdb, "Properties", 1 ), "_columns table wrong\n");
    ok( 0x1d01 == get_columns_table_type(hdb, "Properties", 2 ), "_columns table wrong\n");
    ok( 0x1502 == get_columns_table_type(hdb, "Properties", 3 ), "_columns table wrong\n");
    ok( 0x1502 == get_columns_table_type(hdb, "Properties", 4 ), "_columns table wrong\n");
    ok( 0x1502 == get_columns_table_type(hdb, "Properties", 5 ), "_columns table wrong\n");
    ok( 0x1104 == get_columns_table_type(hdb, "Properties", 6 ), "_columns table wrong\n");
    ok( 0x1d00 == get_columns_table_type(hdb, "Properties", 7 ), "_columns table wrong\n");

    /* now try the names */
    rec = get_column_info( hdb, "select * from `Properties`", LIBMSI_COL_INFO_NAMES );
    ok( rec, "failed to get column info record\n" );

    ok( check_record( rec, 1, "Property"), "wrong record type\n");
    ok( check_record( rec, 2, "Value"), "wrong record type\n");
    ok( check_record( rec, 3, "Intvalue"), "wrong record type\n");
    ok( check_record( rec, 4, "Integervalue"), "wrong record type\n");
    ok( check_record( rec, 5, "Shortvalue"), "wrong record type\n");
    ok( check_record( rec, 6, "Longvalue"), "wrong record type\n");
    ok( check_record( rec, 7, "Longcharvalue"), "wrong record type\n");

    g_object_unref( rec );

    r = run_query( hdb, 0,
            "CREATE TABLE `Binary` "
            "( `Name` CHAR(255), `Data` OBJECT  PRIMARY KEY `Name`)" );
    ok( r == LIBMSI_RESULT_SUCCESS , "Failed to create table\n" );

    /* check the column types */
    rec = get_column_info( hdb, "select * from `Binary`", LIBMSI_COL_INFO_TYPES );
    ok( rec, "failed to get column info record\n" );

    ok( check_record( rec, 1, "S255"), "wrong record type\n");
    ok( check_record( rec, 2, "V0"), "wrong record type\n");

    g_object_unref( rec );

    /* check the type in _Columns */
    ok( 0x3dff == get_columns_table_type(hdb, "Binary", 1 ), "_columns table wrong\n");
    ok( 0x1900 == get_columns_table_type(hdb, "Binary", 2 ), "_columns table wrong\n");

    /* now try the names */
    rec = get_column_info( hdb, "select * from `Binary`", LIBMSI_COL_INFO_NAMES );
    ok( rec, "failed to get column info record\n" );

    ok( check_record( rec, 1, "Name"), "wrong record type\n");
    ok( check_record( rec, 2, "Data"), "wrong record type\n");
    g_object_unref( rec );

    r = run_query( hdb, 0,
            "CREATE TABLE `UIText` "
            "( `Key` CHAR(72) NOT NULL, `Text` CHAR(255) LOCALIZABLE PRIMARY KEY `Key`)" );
    ok( r == LIBMSI_RESULT_SUCCESS , "Failed to create table\n" );

    ok( 0x2d48 == get_columns_table_type(hdb, "UIText", 1 ), "_columns table wrong\n");
    ok( 0x1fff == get_columns_table_type(hdb, "UIText", 2 ), "_columns table wrong\n");

    rec = get_column_info( hdb, "select * from `UIText`", LIBMSI_COL_INFO_NAMES );
    ok( rec, "failed to get column info record\n" );
    ok( check_record( rec, 1, "Key"), "wrong record type\n");
    ok( check_record( rec, 2, "Text"), "wrong record type\n");
    g_object_unref( rec );

    rec = get_column_info( hdb, "select * from `UIText`", LIBMSI_COL_INFO_TYPES );
    ok( rec, "failed to get column info record\n" );
    ok( check_record( rec, 1, "s72"), "wrong record type\n");
    ok( check_record( rec, 2, "L255"), "wrong record type\n");
    g_object_unref( rec );

    g_object_unref( hdb );
}

static void test_msiexport(void)
{
    LibmsiDatabase *hdb = 0;
    LibmsiQuery *hquery = 0;
    unsigned r;
    const char *sql;
    int fd;
    const char file[] = "phone.txt";
    char buffer[0x100];
    unsigned length;
    const char expected[] =
        "id\tname\tnumber\r\n"
        "I2\tS32\tS32\r\n"
        "phone\tid\r\n"
        "1\tAbe\t8675309\r\n";

    unlink(msifile);

    /* just libmsi_database_open should not create a file */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "libmsi_database_open failed\n");

    /* create a table */
    sql = "CREATE TABLE `phone` ( "
            "`id` INT, `name` CHAR(32), `number` CHAR(32) "
            "PRIMARY KEY `id`)";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");
    r = libmsi_query_close(hquery, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(hquery);

    /* insert a value into it */
    sql = "INSERT INTO `phone` ( `id`, `name`, `number` )"
        "VALUES('1', 'Abe', '8675309')";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");
    r = libmsi_query_close(hquery, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(hquery);

    fd = open(file, O_WRONLY | O_BINARY | O_CREAT, 0644);
    ok(fd != -1, "open failed\n");

    r = libmsi_database_export(hdb, "phone", fd, NULL);
    ok(r, "libmsi_database_export failed\n");

    close(fd);

    g_object_unref(hdb);

    /* check the data that was written */
    length = 0;
    memset(buffer, 0, sizeof buffer);
    fd = open(file, O_RDONLY | O_BINARY);
    if (fd != -1)
    {
        length = read(fd, buffer, sizeof buffer);
        close(fd);
        unlink(file);
    }
    else
        ok(0, "failed to open file %s\n", file);

    ok( length == strlen(expected), "length of data wrong\n");
    ok( g_str_equal(buffer, expected), "data doesn't match\n");
    unlink(msifile);
}

static void test_longstrings(void)
{
    const char insert_query[] =
        "INSERT INTO `strings` ( `id`, `val` ) VALUES('1', 'Z')";
    char *str;
    LibmsiDatabase *hdb = 0;
    LibmsiQuery *hquery = 0;
    LibmsiRecord *hrec = 0;
    unsigned len;
    unsigned r;
    const unsigned STRING_LENGTH = 0x10005;

    unlink(msifile);
    /* just libmsi_database_open should not create a file */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "libmsi_database_open failed\n");

    /* create a table */
    r = try_query( hdb,
        "CREATE TABLE `strings` ( `id` INT, `val` CHAR(0) PRIMARY KEY `id`)");
    ok(r == LIBMSI_RESULT_SUCCESS, "query failed\n");

    /* try a insert a very long string */
    str = malloc(STRING_LENGTH+sizeof insert_query);
    len = strchr(insert_query, 'Z') - insert_query;
    strcpy(str, insert_query);
    memset(str+len, 'Z', STRING_LENGTH);
    strcpy(str+len+STRING_LENGTH, insert_query+len+1);
    r = try_query( hdb, str );
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_database_open_query failed\n");

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "libmsi_database_commit failed\n");
    g_object_unref(hdb);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_READONLY, NULL, NULL);
    ok(hdb, "libmsi_database_open failed\n");

    hquery = libmsi_query_new(hdb, "select * from `strings` where `id` = 1", NULL);
    ok(hquery, "libmsi_database_open_query failed\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "libmsi_query_fetch failed");

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    str[len+STRING_LENGTH] = '\0';
    check_record_string (hrec, 2, str+len);

    g_free(str);
    g_object_unref(hrec);
    g_object_unref(hdb);
    unlink(msifile);
}

static void create_file_data(const char *name, const char *data, unsigned size)
{
    int file;

    file = open(name, O_CREAT | O_WRONLY | O_BINARY, 0644);
    if (file == -1)
        return;

    write(file, data, size ? size : strlen(data));
    if (size == 0)
        write(file, "\n", strlen("\n"));
    assert(size == 0 || size == lseek(file, 0, SEEK_CUR));

    close(file);
}

#define create_file(name) create_file_data(name, name, 0)

static void test_streamtable(void)
{
    GError *error = NULL;
    GInputStream *in;
    LibmsiDatabase *hdb = 0;
    LibmsiRecord *rec;
    LibmsiQuery *query;
    LibmsiSummaryInfo *hsi;
    char file[256];
    char buf[256];
    unsigned size;
    unsigned r;

    hdb = create_db();
    ok( hdb, "failed to create db\n");

    r = run_query( hdb, 0,
            "CREATE TABLE `Properties` "
            "( `Property` CHAR(255), `Value` CHAR(1)  PRIMARY KEY `Property`)" );
    ok( r == LIBMSI_RESULT_SUCCESS , "Failed to create table\n" );

    r = run_query( hdb, 0,
            "INSERT INTO `Properties` "
            "( `Value`, `Property` ) VALUES ( 'Prop', 'value' )" );
    ok( r == LIBMSI_RESULT_SUCCESS, "Failed to add to table\n" );

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "Failed to commit database\n");

    g_object_unref( hdb );

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_TRANSACT, NULL, NULL );
    ok(hdb , "Failed to open database\n" );

    /* check the column types */
    rec = get_column_info( hdb, "select * from `_Streams`", LIBMSI_COL_INFO_TYPES );
    ok( rec, "failed to get column info record\n" );

    ok( check_record( rec, 1, "s62"), "wrong record type\n");
    ok( check_record( rec, 2, "V0"), "wrong record type\n");

    g_object_unref( rec );

    /* now try the names */
    rec = get_column_info( hdb, "select * from `_Streams`", LIBMSI_COL_INFO_NAMES );
    ok( rec, "failed to get column info record\n" );

    ok( check_record( rec, 1, "Name"), "wrong record type\n");
    ok( check_record( rec, 2, "Data"), "wrong record type\n");

    g_object_unref( rec );

    query = libmsi_query_new( hdb,
            "SELECT * FROM `_Streams` WHERE `Name` = '\5SummaryInformation'", NULL );
    ok(query, "Failed to open database query\n");

    r = libmsi_query_execute( query, 0 , NULL);
    ok( r, "Failed to execute query: %u\n", r );

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref( query );

    /* create a summary information stream */
    hsi = libmsi_summary_info_new (hdb, 1, NULL);
    ok(hsi, "Failed to get summary information handle: %u\n", r );

    libmsi_summary_info_set_int(hsi, LIBMSI_PROPERTY_SECURITY, 2, &error);
    ok(!error, "Failed to set property\n");

    r = libmsi_summary_info_persist( hsi, NULL );
    ok(r, "Failed to save summary information: %u\n", r );

    g_object_unref( hsi );

    query = NULL;
    query = libmsi_query_new( hdb,
            "SELECT * FROM `_Streams` WHERE `Name` = '\5SummaryInformation'", NULL );
    ok(query, "Failed to open database query\n");

    r = libmsi_query_execute( query, 0 , NULL);
    ok( r, "Failed to execute query: %u\n", r );

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Unexpected result\n");

    g_object_unref( rec );
    libmsi_query_close(query, NULL);
    g_object_unref( query );

    /* insert a file into the _Streams table */
    create_file( "test.txt" );

    rec = libmsi_record_new( 2 );
    libmsi_record_set_string( rec, 1, "data" );

    r = libmsi_record_load_stream( rec, 2, "test.txt" );
    ok(r, "Failed to add stream data to the record: %d\n", r);

    unlink("test.txt");

    query = libmsi_query_new( hdb,
            "INSERT INTO `_Streams` ( `Name`, `Data` ) VALUES ( ?, ? )", NULL );
    ok(query, "Failed to open database query\n");

    r = libmsi_query_execute( query, rec , NULL);
    ok( r, "Failed to execute query: %d\n", r);

    g_object_unref( rec );
    libmsi_query_close(query, NULL);
    g_object_unref( query );

    /* insert another one */
    create_file( "test1.txt" );

    rec = libmsi_record_new( 2 );
    libmsi_record_set_string( rec, 1, "data1" );

    r = libmsi_record_load_stream( rec, 2, "test1.txt" );
    ok(r, "Failed to add stream data to the record: %d\n", r);

    unlink("test1.txt");

    query = libmsi_query_new( hdb,
            "INSERT INTO `_Streams` ( `Name`, `Data` ) VALUES ( ?, ? )", NULL );
    ok(query, "Failed to open database query\n");

    r = libmsi_query_execute( query, rec , NULL);
    ok( r, "Failed to execute query: %d\n", r);

    g_object_unref( rec );
    libmsi_query_close(query, NULL);
    g_object_unref( query );

    query = libmsi_query_new( hdb,
            "SELECT `Name`, `Data` FROM `_Streams` WHERE `Name` = 'data'", NULL );
    ok(query, "Failed to open database query\n");

    r = libmsi_query_execute( query, 0 , NULL);
    ok( r, "Failed to execute query: %d\n", r);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Failed to fetch record\n");

    check_record_string (rec, 1, "data");

    size = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    in = libmsi_record_get_stream(rec, 2);
    ok(in, "Failed to get stream\n");
    size = g_input_stream_read(in, buf, sizeof(buf), NULL, NULL);
    ok( g_str_equal(buf, "test.txt\n"), "Expected 'test.txt\\n', got %s\n", buf);
    g_object_unref(in);

    g_object_unref( rec );
    libmsi_query_close(query, NULL);
    g_object_unref( query );

    query = libmsi_query_new( hdb,
            "SELECT `Name`, `Data` FROM `_Streams` WHERE `Name` = 'data1'", NULL);
    ok(query, "Failed to open database query\n");

    r = libmsi_query_execute( query, 0 , NULL);
    ok( r, "Failed to execute query: %d\n", r);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result");

    check_record_string (rec, 1, "data1");

    size = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    in = libmsi_record_get_stream(rec, 2);
    ok(in, "Failed to get stream\n");
    size = g_input_stream_read(in, buf, sizeof(buf), NULL, NULL);
    ok( g_str_equal(buf, "test1.txt\n"), "Expected 'test1.txt\\n', got %s\n", buf);
    g_object_unref(in);

    g_object_unref( rec );
    libmsi_query_close(query, NULL);
    g_object_unref( query );

    /* perform an update */
    create_file( "test2.txt" );
    rec = libmsi_record_new( 1 );

    r = libmsi_record_load_stream( rec, 1, "test2.txt" );
    ok(r, "Failed to add stream data to the record: %d\n", r);

    unlink("test2.txt");

    query = libmsi_query_new( hdb,
            "UPDATE `_Streams` SET `Data` = ? WHERE `Name` = 'data1'", NULL);
    ok(query, "Failed to open database query\n");

    r = libmsi_query_execute( query, rec , NULL);
    ok( r, "Failed to execute query: %d\n", r);

    g_object_unref( rec );
    libmsi_query_close(query, NULL);
    g_object_unref( query );

    query = libmsi_query_new( hdb,
            "SELECT `Name`, `Data` FROM `_Streams` WHERE `Name` = 'data1'", NULL);
    ok(query, "Failed to open database query\n");

    r = libmsi_query_execute( query, 0 , NULL);
    ok( r, "Failed to execute query: %d\n", r);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Failed to fetch record\n");

    check_record_string (rec, 1, "data1");

    size = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    in = libmsi_record_get_stream(rec, 2);
    ok(in, "Failed to get stream\n");
    size = g_input_stream_read(in, buf, sizeof(buf), NULL, NULL);
    todo_wine ok( g_str_equal(buf, "test2.txt\n"), "Expected 'test2.txt\\n', got %s\n", buf);
    g_object_unref(in);

    g_object_unref( rec );
    libmsi_query_close(query, NULL);
    g_object_unref( query );

    r = run_query( hdb, 0, "DELETE FROM `_Streams` WHERE `Name` = 'data1'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "Cannot create Binary table: %d\n", r );

    query = libmsi_query_new( hdb,
            "SELECT `Name`, `Data` FROM `_Streams` WHERE `Name` = 'data1'", NULL);
    ok(query, "Failed to open database query\n");

    r = libmsi_query_execute( query, 0 , NULL);
    ok( r, "Failed to execute query: %d\n", r);

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref( query );
    g_object_unref( hdb );
    unlink(msifile);
}

static void test_binary(void)
{
    GInputStream *in;
    LibmsiDatabase *hdb = 0;
    LibmsiRecord *rec;
    char file[256];
    char buf[256];
    unsigned size;
    const char *sql;
    unsigned r;

    /* insert a file into the Binary table */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL );
    ok(hdb , "Failed to open database\n" );

    sql = "CREATE TABLE `Binary` ( `Name` CHAR(72) NOT NULL, `ID` INT NOT NULL, `Data` OBJECT  PRIMARY KEY `Name`, `ID`)";
    r = run_query( hdb, 0, sql );
    ok( r == LIBMSI_RESULT_SUCCESS, "Cannot create Binary table: %d\n", r );

    create_file( "test.txt" );
    rec = libmsi_record_new( 1 );
    r = libmsi_record_load_stream( rec, 1, "test.txt" );
    ok(r, "Failed to add stream data to the record: %d\n", r);
    unlink( "test.txt" );

    sql = "INSERT INTO `Binary` ( `Name`, `ID`, `Data` ) VALUES ( 'filename1', 1, ? )";
    r = run_query( hdb, rec, sql );
    ok( r == LIBMSI_RESULT_SUCCESS, "Insert into Binary table failed: %d\n", r );

    g_object_unref( rec );

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "Failed to commit database\n" );

    g_object_unref( hdb );

    /* read file from the Stream table */
    hdb = libmsi_database_new( msifile, LIBMSI_DB_FLAGS_READONLY, NULL, NULL );
    ok(hdb , "Failed to open database\n" );

    sql = "SELECT * FROM `_Streams`";
    r = do_query( hdb, sql, &rec );
    ok( r == LIBMSI_RESULT_SUCCESS, "SELECT query failed: %d\n", r );

    check_record_string (rec, 1, "Binary.filename1.1");

    size = sizeof(buf);
    memset( buf, 0, sizeof(buf) );
    in = libmsi_record_get_stream(rec, 2);
    ok(in, "Failed to get stream\n");
    size = g_input_stream_read(in, buf, sizeof(buf), NULL, NULL);
    ok( g_str_equal(buf, "test.txt\n"), "Expected 'test.txt\\n', got %s\n", buf );
    g_object_unref(in);

    g_object_unref( rec );

    /* read file from the Binary table */
    sql = "SELECT * FROM `Binary`";
    r = do_query( hdb, sql, &rec );
    ok( r == LIBMSI_RESULT_SUCCESS, "SELECT query failed: %d\n", r );

    check_record_string (rec, 1, "filename1");

    size = sizeof(buf);
    memset( buf, 0, sizeof(buf) );
    in = libmsi_record_get_stream(rec, 3);
    ok(in, "Failed to get stream\n");
    size = g_input_stream_read(in, buf, sizeof(buf), NULL, NULL);
    ok( g_str_equal(buf, "test.txt\n"), "Expected 'test.txt\\n', got %s\n", buf );
    g_object_unref(in);

    g_object_unref( rec );

    g_object_unref( hdb );

    unlink( msifile );
}

static void test_where_not_in_selected(void)
{
    LibmsiDatabase *hdb = 0;
    LibmsiRecord *rec;
    LibmsiQuery *query;
    const char *sql;
    unsigned r;

    hdb = create_db();
    ok( hdb, "failed to create db\n");

    r = run_query(hdb, 0,
            "CREATE TABLE `IESTable` ("
            "`Action` CHAR(64), "
            "`Condition` CHAR(64), "
            "`Sequence` LONG PRIMARY KEY `Sequence`)");
    ok( r == LIBMSI_RESULT_SUCCESS, "Cannot create IESTable table: %d\n", r);

    r = run_query(hdb, 0,
            "CREATE TABLE `CATable` ("
            "`Action` CHAR(64), "
            "`Type` LONG PRIMARY KEY `Type`)");
    ok( r == LIBMSI_RESULT_SUCCESS, "Cannot create CATable table: %d\n", r);

    r = run_query(hdb, 0, "INSERT INTO `IESTable` "
            "( `Action`, `Condition`, `Sequence`) "
            "VALUES ( 'clean', 'cond4', 4)");
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add entry to IESTable table:%d\n", r );

    r = run_query(hdb, 0, "INSERT INTO `IESTable` "
            "( `Action`, `Condition`, `Sequence`) "
            "VALUES ( 'depends', 'cond1', 1)");
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add entry to IESTable table:%d\n", r );

    r = run_query(hdb, 0, "INSERT INTO `IESTable` "
            "( `Action`, `Condition`, `Sequence`) "
            "VALUES ( 'build', 'cond2', 2)");
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add entry to IESTable table:%d\n", r );

    r = run_query(hdb, 0, "INSERT INTO `IESTable` "
            "( `Action`, `Condition`, `Sequence`) "
            "VALUES ( 'build2', 'cond6', 6)");
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add entry to IESTable table:%d\n", r );

    r = run_query(hdb, 0, "INSERT INTO `IESTable` "
            "( `Action`, `Condition`, `Sequence`) "
            "VALUES ( 'build', 'cond3', 3)");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add entry to IESTable table:%d\n", r );

    r = run_query(hdb, 0, "INSERT INTO `CATable` "
            "( `Action`, `Type` ) "
            "VALUES ( 'build', 32)");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add entry to CATable table:%d\n", r );

    r = run_query(hdb, 0, "INSERT INTO `CATable` "
            "( `Action`, `Type` ) "
            "VALUES ( 'depends', 64)");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add entry to CATable table:%d\n", r );

    r = run_query(hdb, 0, "INSERT INTO `CATable` "
            "( `Action`, `Type` ) "
            "VALUES ( 'clean', 63)");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add entry to CATable table:%d\n", r );

    r = run_query(hdb, 0, "INSERT INTO `CATable` "
            "( `Action`, `Type` ) "
            "VALUES ( 'build2', 34)");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add entry to CATable table:%d\n", r );

    query = NULL;
    sql = "Select IESTable.Condition from CATable, IESTable where "
            "CATable.Action = IESTable.Action and CATable.Type = 32";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "failed to open query: %d\n");

    r = libmsi_query_execute(query, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "failed to fetch query\n" );

    ok( check_record( rec, 1, "cond2"), "wrong condition\n");

    g_object_unref( rec );
    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "failed to fetch query\n");

    ok( check_record( rec, 1, "cond3"), "wrong condition\n");

    g_object_unref( rec );
    libmsi_query_close(query, NULL);
    g_object_unref(query);

    g_object_unref( hdb );
    unlink(msifile);

}


static void test_where(void)
{
    LibmsiDatabase *hdb = 0;
    LibmsiRecord *rec;
    LibmsiQuery *query;
    const char *sql;
    unsigned r;
    unsigned size;
    char buf[256];
    unsigned count;

    hdb = create_db();
    ok( hdb, "failed to create db\n");

    r = run_query( hdb, 0,
            "CREATE TABLE `Media` ("
            "`DiskId` SHORT NOT NULL, "
            "`LastSequence` LONG, "
            "`DiskPrompt` CHAR(64) LOCALIZABLE, "
            "`Cabinet` CHAR(255), "
            "`VolumeLabel` CHAR(32), "
            "`Source` CHAR(72) "
            "PRIMARY KEY `DiskId`)" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot create Media table: %d\n", r );

    r = run_query( hdb, 0, "INSERT INTO `Media` "
            "( `DiskId`, `LastSequence`, `DiskPrompt`, `Cabinet`, `VolumeLabel`, `Source` ) "
            "VALUES ( 1, 0, '', 'zero.cab', '', '' )" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add file to the Media table: %d\n", r );

    r = run_query( hdb, 0, "INSERT INTO `Media` "
            "( `DiskId`, `LastSequence`, `DiskPrompt`, `Cabinet`, `VolumeLabel`, `Source` ) "
            "VALUES ( 2, 1, '', 'one.cab', '', '' )" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add file to the Media table: %d\n", r );

    r = run_query( hdb, 0, "INSERT INTO `Media` "
            "( `DiskId`, `LastSequence`, `DiskPrompt`, `Cabinet`, `VolumeLabel`, `Source` ) "
            "VALUES ( 3, 2, '', 'two.cab', '', '' )" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add file to the Media table: %d\n", r );

    sql = "SELECT * FROM `Media`";
    r = do_query(hdb, sql, &rec);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_query_fetch failed: %d\n", r);
    ok( check_record( rec, 4, "zero.cab"), "wrong cabinet\n");
    g_object_unref( rec );
    rec = NULL;

    sql = "SELECT * FROM `Media` WHERE `LastSequence` >= 1";
    r = do_query(hdb, sql, &rec);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_query_fetch failed: %d\n", r);
    ok( check_record( rec, 4, "one.cab"), "wrong cabinet\n");

    r = libmsi_record_get_int(rec, 1);
    ok( 2 == r, "field wrong\n");
    r = libmsi_record_get_int(rec, 2);
    ok( 1 == r, "field wrong\n");
    g_object_unref( rec );
    rec = NULL;

    sql = "SELECT `DiskId` FROM `Media` WHERE `LastSequence` >= 1 AND DiskId >= 0";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "failed to open query: %d\n", r );

    r = libmsi_query_execute(query, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "failed to fetch query\n");

    count = libmsi_record_get_field_count( rec );
    ok( count == 1, "Expected 1 record fields, got %d\n", count );

    check_record_string(rec, 1, "2");
    g_object_unref(rec);
    rec = NULL;

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "failed to fetch query: %d\n", r );

    check_record_string(rec, 1, "3");
    g_object_unref(rec);
    rec = NULL;

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref(query);

    sql = "SELECT * FROM `Media` WHERE `DiskPrompt` IS NULL";
    r = do_query(hdb, sql, &rec);
    ok( r == LIBMSI_RESULT_SUCCESS, "query failed: %d\n", r );
    g_object_unref( rec );

    rec = 0;
    sql = "SELECT * FROM `Media` WHERE `DiskPrompt` < 'Cabinet'";
    r = do_query(hdb, sql, &rec);
    ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %d\n", r );
    ok(rec == NULL, "Must be null");

    sql = "SELECT * FROM `Media` WHERE `DiskPrompt` > 'Cabinet'";
    r = do_query(hdb, sql, &rec);
    ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %d\n", r );
    ok(rec == NULL, "Must be null");

    sql = "SELECT * FROM `Media` WHERE `DiskPrompt` <> 'Cabinet'";
    r = do_query(hdb, sql, &rec);
    ok( r == LIBMSI_RESULT_SUCCESS, "query failed: %d\n", r );
    g_object_unref( rec );
    rec = NULL;

    sql = "SELECT * FROM `Media` WHERE `DiskPrompt` = 'Cabinet'";
    r = do_query(hdb, sql, &rec);
    ok( r == LIBMSI_RESULT_SUCCESS, "query failed: %d\n", r );
    ok(rec == NULL, "Must be null");

    rec = libmsi_record_new(1);
    libmsi_record_set_string(rec, 1, "");

    sql = "SELECT * FROM `Media` WHERE `DiskPrompt` = ?";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_query_execute(query, rec, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    g_object_unref(rec);
    libmsi_query_close(query, NULL);
    g_object_unref(query);

    g_object_unref( hdb );
    unlink(msifile);
}

static const char test_data[] = "FirstPrimaryColumn\tSecondPrimaryColumn\tShortInt\tShortIntNullable\tLongInt\tLongIntNullable\tString\tLocalizableString\tLocalizableStringNullable\n"
                                "s255\ti2\ti2\tI2\ti4\tI4\tS255\tS0\ts0\n"
                                "TestTable\tFirstPrimaryColumn\n"
                                "stringage\t5\t2\t\t2147483640\t-2147483640\tanother string\tlocalizable\tduh\n";

static const char two_primary[] = "PrimaryOne\tPrimaryTwo\n"
                                  "s255\ts255\n"
                                  "TwoPrimary\tPrimaryOne\tPrimaryTwo\n"
                                  "papaya\tleaf\n"
                                  "papaya\tflower\n";

static const char endlines1[] = "A\tB\tC\tD\tE\tF\r\n"
                                "s72\ts72\ts72\ts72\ts72\ts72\n"
                                "Table\tA\r\n"
                                "a\tb\tc\td\te\tf\n"
                                "g\th\ti\t\rj\tk\tl\r\n";

static const char endlines2[] = "A\tB\tC\tD\tE\tF\r"
                                "s72\ts72\ts72\ts72\ts72\ts72\n"
                                "Table2\tA\r\n"
                                "a\tb\tc\td\te\tf\n"
                                "g\th\ti\tj\tk\tl\r\n";

static const char suminfo[] = "PropertyId\tValue\n"
                              "i2\tl255\n"
                              "_SummaryInformation\tPropertyId\n"
                              "1\t1252\n"
                              "2\tInstaller Database\n"
                              "3\tInstaller description\n"
                              "4\tWineHQ\n"
                              "5\tInstaller\n"
                              "6\tInstaller comments\n"
                              "7\tIntel;1033,2057\n"
                              "9\t{12345678-1234-1234-1234-123456789012}\n"
                              "12\t2009/04/12 15:46:11\n"
                              "13\t2009/04/12 15:46:11\n"
                              "14\t200\n"
                              "15\t2\n"
                              "18\tVim\n"
                              "19\t2\n";

static void write_file(const char *filename, const char *data, int data_size)
{
    int file;

    file = open(filename, O_CREAT | O_WRONLY | O_BINARY, 0644);
    if (file == -1)
        return;

    write(file, data, data_size);
    close(file);
}

static unsigned add_table_to_db(LibmsiDatabase *hdb, const char *table_data)
{
    GError *err = NULL;
    unsigned r = LIBMSI_RESULT_SUCCESS;

    write_file("temp_file", table_data, (strlen(table_data) - 1) * sizeof(char));
    if (!libmsi_database_import(hdb, "temp_file", &err))
        r = err->code;

    g_clear_error(&err);
    unlink("temp_file");

    return r;
}

static void test_suminfo_import(void)
{
    GError *error = NULL;
    GArray *props;
    LibmsiDatabase *hdb;
    LibmsiSummaryInfo *hsi;
    LibmsiQuery *query = 0;
    const char *sql;
    unsigned r = 0, type;
    const char *str_value;
    int int_value;
    guint64 ft_value;

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %u\n", r);

    r = add_table_to_db(hdb, suminfo);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %u\n", r);

    /* _SummaryInformation is not imported as a regular table... */

    query = NULL;
    sql = "SELECT * FROM `_SummaryInformation`";
    query = libmsi_query_new(hdb, sql, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_BAD_QUERY_SYNTAX);
    ok(query == NULL, "Must be null");
    g_clear_error(&error);

    /* ...its data is added to the special summary information stream */

    hsi = libmsi_summary_info_new(hdb, 0, NULL);
    ok(hsi, "libmsi_summary_info_new() failed");

    props = libmsi_summary_info_get_properties (hsi);
    ok(props->len == 14, "Expected 14, got %u\n", props->len);
    g_array_unref (props);

    int_value = libmsi_summary_info_get_int (hsi, LIBMSI_PROPERTY_CODEPAGE, &error);
    ok(!error, "Expected success\n");
    ok(int_value == 1252, "Expected 1252, got %d\n", int_value);

    str_value = libmsi_summary_info_get_string (hsi, LIBMSI_PROPERTY_TITLE, &error);
    ok(!error, "Expected no error\n");
    g_assert_cmpstr (str_value, ==, "Installer Database");

    str_value = libmsi_summary_info_get_string (hsi, LIBMSI_PROPERTY_SUBJECT, &error);
    ok(!error, "Expected no error\n");
    g_assert_cmpstr (str_value, ==, "Installer description");

    str_value = libmsi_summary_info_get_string (hsi, LIBMSI_PROPERTY_AUTHOR, &error);
    ok(!error, "Expected no error\n");
    g_assert_cmpstr (str_value, ==, "WineHQ");

    str_value = libmsi_summary_info_get_string (hsi, LIBMSI_PROPERTY_KEYWORDS, &error);
    ok(!error, "Expected no error\n");
    g_assert_cmpstr (str_value, ==, "Installer");

    str_value = libmsi_summary_info_get_string (hsi, LIBMSI_PROPERTY_COMMENTS, &error);
    ok(!error, "Expected no error\n");
    g_assert_cmpstr (str_value, ==, "Installer comments");

    str_value = libmsi_summary_info_get_string (hsi, LIBMSI_PROPERTY_TEMPLATE, &error);
    ok(!error, "Expected no error\n");
    g_assert_cmpstr (str_value, ==, "Intel;1033,2057");

    str_value = libmsi_summary_info_get_string (hsi, LIBMSI_PROPERTY_UUID, &error);
    ok(!error, "Expected no error\n");
    g_assert_cmpstr (str_value, ==, "{12345678-1234-1234-1234-123456789012}");

    ft_value = libmsi_summary_info_get_filetime (hsi, LIBMSI_PROPERTY_CREATED_TM, &error);
    ok(!error, "Expected no error\n");

    ft_value = libmsi_summary_info_get_filetime (hsi, LIBMSI_PROPERTY_LASTSAVED_TM, &error);
    ok(!error, "Expected no error\n");

    int_value = libmsi_summary_info_get_int (hsi, LIBMSI_PROPERTY_VERSION, &error);
    ok(!error, "Expected success\n");
    ok(int_value == 200, "Expected 200, got %d\n", int_value);

    int_value = libmsi_summary_info_get_int (hsi, LIBMSI_PROPERTY_SOURCE, &error);
    ok(!error, "Expected success\n");
    ok(int_value == 2, "Expected 2, got %d\n", int_value);

    int_value = libmsi_summary_info_get_int (hsi, LIBMSI_PROPERTY_SECURITY, &error);
    ok(!error, "Expected success\n");
    ok(int_value == 2, "Expected 2, got %d\n", int_value);

    str_value = libmsi_summary_info_get_string (hsi, LIBMSI_PROPERTY_APPNAME, &error);
    ok(!error, "Expected no error\n");
    g_assert_cmpstr (str_value, ==, "Vim");

    g_object_unref(hsi);
    g_object_unref(hdb);
    unlink(msifile);
}

static void test_msiimport(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *query;
    LibmsiRecord *rec;
    const char *sql;
    unsigned r = 0, count;
    signed int i;

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = add_table_to_db(hdb, test_data);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = add_table_to_db(hdb, two_primary);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = add_table_to_db(hdb, endlines1);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = add_table_to_db(hdb, endlines2);
    ok(r == LIBMSI_RESULT_FUNCTION_FAILED, "Expected LIBMSI_RESULT_FUNCTION_FAILED, got %d\n", r);

    query = NULL;
    sql = "SELECT * FROM `TestTable`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");

    rec = NULL;
    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_NAMES, NULL);
    ok(rec, "Expected result\n");
    count = libmsi_record_get_field_count(rec);
    ok(count == 9, "Expected 9, got %d\n", count);
    ok(check_record(rec, 1, "FirstPrimaryColumn"), "Expected FirstPrimaryColumn\n");
    ok(check_record(rec, 2, "SecondPrimaryColumn"), "Expected SecondPrimaryColumn\n");
    ok(check_record(rec, 3, "ShortInt"), "Expected ShortInt\n");
    ok(check_record(rec, 4, "ShortIntNullable"), "Expected ShortIntNullalble\n");
    ok(check_record(rec, 5, "LongInt"), "Expected LongInt\n");
    ok(check_record(rec, 6, "LongIntNullable"), "Expected LongIntNullalble\n");
    ok(check_record(rec, 7, "String"), "Expected String\n");
    ok(check_record(rec, 8, "LocalizableString"), "Expected LocalizableString\n");
    ok(check_record(rec, 9, "LocalizableStringNullable"), "Expected LocalizableStringNullable\n");
    g_object_unref(rec);

    rec = NULL;
    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_TYPES, NULL);
    ok(rec, "Expected result\n");
    count = libmsi_record_get_field_count(rec);
    ok(count == 9, "Expected 9, got %d\n", count);
    ok(check_record(rec, 1, "s255"), "Expected s255\n");
    ok(check_record(rec, 2, "i2"), "Expected i2\n");
    ok(check_record(rec, 3, "i2"), "Expected i2\n");
    ok(check_record(rec, 4, "I2"), "Expected I2\n");
    ok(check_record(rec, 5, "i4"), "Expected i4\n");
    ok(check_record(rec, 6, "I4"), "Expected I4\n");
    ok(check_record(rec, 7, "S255"), "Expected S255\n");
    ok(check_record(rec, 8, "S0"), "Expected S0\n");
    ok(check_record(rec, 9, "s0"), "Expected s0\n");
    g_object_unref(rec);

    sql = "SELECT * FROM `TestTable`";
    r = do_query(hdb, sql, &rec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    ok(check_record(rec, 1, "stringage"), "Expected 'stringage'\n");
    ok(check_record(rec, 7, "another string"), "Expected 'another string'\n");
    ok(check_record(rec, 8, "localizable"), "Expected 'localizable'\n");
    ok(check_record(rec, 9, "duh"), "Expected 'duh'\n");

    i = libmsi_record_get_int(rec, 2);
    ok(i == 5, "Expected 5, got %d\n", i);

    i = libmsi_record_get_int(rec, 3);
    ok(i == 2, "Expected 2, got %d\n", i);

    i = libmsi_record_get_int(rec, 4);
    ok(i == LIBMSI_NULL_INT, "Expected LIBMSI_NULL_INT, got %d\n", i);

    i = libmsi_record_get_int(rec, 5);
    ok(i == 2147483640, "Expected 2147483640, got %d\n", i);

    i = libmsi_record_get_int(rec, 6);
    ok(i == -2147483640, "Expected -2147483640, got %d\n", i);

    g_object_unref(rec);
    libmsi_query_close(query, NULL);
    g_object_unref(query);

    query = NULL;
    sql = "SELECT * FROM `TwoPrimary`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");

    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_NAMES, NULL);
    ok(rec, "Expected result\n");
    count = libmsi_record_get_field_count(rec);
    ok(count == 2, "Expected 2, got %d\n", count);
    ok(check_record(rec, 1, "PrimaryOne"), "Expected PrimaryOne\n");
    ok(check_record(rec, 2, "PrimaryTwo"), "Expected PrimaryTwo\n");

    g_object_unref(rec);

    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_TYPES, NULL);
    ok(rec, "Expected result\n");
    count = libmsi_record_get_field_count(rec);
    ok(count == 2, "Expected 2, got %d\n", count);
    ok(check_record(rec, 1, "s255"), "Expected s255\n");
    ok(check_record(rec, 2, "s255"), "Expected s255\n");
    g_object_unref(rec);

    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    ok(check_record(rec, 1, "papaya"), "Expected 'papaya'\n");
    ok(check_record(rec, 2, "leaf"), "Expected 'leaf'\n");

    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    ok(check_record(rec, 1, "papaya"), "Expected 'papaya'\n");
    ok(check_record(rec, 2, "flower"), "Expected 'flower'\n");

    g_object_unref(rec);

    query_check_no_more(query);

    r = libmsi_query_close(query, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    g_object_unref(query);

    query = NULL;
    sql = "SELECT * FROM `Table`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");

    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_NAMES, NULL);
    ok(rec, "Expected result\n");
    count = libmsi_record_get_field_count(rec);
    ok(count == 6, "Expected 6, got %d\n", count);
    ok(check_record(rec, 1, "A"), "Expected A\n");
    ok(check_record(rec, 2, "B"), "Expected B\n");
    ok(check_record(rec, 3, "C"), "Expected C\n");
    ok(check_record(rec, 4, "D"), "Expected D\n");
    ok(check_record(rec, 5, "E"), "Expected E\n");
    ok(check_record(rec, 6, "F"), "Expected F\n");
    g_object_unref(rec);

    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_TYPES, NULL);
    ok(rec, "Expected result\n");
    count = libmsi_record_get_field_count(rec);
    ok(count == 6, "Expected 6, got %d\n", count);
    ok(check_record(rec, 1, "s72"), "Expected s72\n");
    ok(check_record(rec, 2, "s72"), "Expected s72\n");
    ok(check_record(rec, 3, "s72"), "Expected s72\n");
    ok(check_record(rec, 4, "s72"), "Expected s72\n");
    ok(check_record(rec, 5, "s72"), "Expected s72\n");
    ok(check_record(rec, 6, "s72"), "Expected s72\n");
    g_object_unref(rec);

    libmsi_query_close(query, NULL);
    g_object_unref(query);

    query = NULL;
    sql = "SELECT * FROM `Table`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");

    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");
    ok(check_record(rec, 1, "a"), "Expected 'a'\n");
    ok(check_record(rec, 2, "b"), "Expected 'b'\n");
    ok(check_record(rec, 3, "c"), "Expected 'c'\n");
    ok(check_record(rec, 4, "d"), "Expected 'd'\n");
    ok(check_record(rec, 5, "e"), "Expected 'e'\n");
    ok(check_record(rec, 6, "f"), "Expected 'f'\n");

    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");
    ok(check_record(rec, 1, "g"), "Expected 'g'\n");
    ok(check_record(rec, 2, "h"), "Expected 'h'\n");
    ok(check_record(rec, 3, "i"), "Expected 'i'\n");
    ok(check_record(rec, 4, "j"), "Expected 'j'\n");
    ok(check_record(rec, 5, "k"), "Expected 'k'\n");
    ok(check_record(rec, 6, "l"), "Expected 'l'\n");

    g_object_unref(rec);

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref(query);
    g_object_unref(hdb);
    unlink(msifile);
}

static const char bin_import_dat[] = "Name\tData\r\n"
                                     "s72\tV0\r\n"
                                     "Binary\tName\r\n"
                                     "filename1\tfilename1.ibd\r\n";

static void test_binary_import(void)
{
    GInputStream *in;
    LibmsiDatabase *hdb = 0;
    LibmsiRecord *rec;
    char file[256];
    char buf[256];
    unsigned size;
    const char *sql;
    unsigned r = 0;

    /* create files to import */
    write_file("bin_import.idt", bin_import_dat,
          (sizeof(bin_import_dat) - 1) * sizeof(char));
    mkdir("Binary", 0755);
    create_file_data("Binary/filename1.ibd", "just some words", 15);

    /* import files into database */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb , "Failed to open database\n");

    r = libmsi_database_import(hdb, "bin_import.idt", NULL);
    ok(r , "Failed to import Binary table\n");

    /* read file from the Binary table */
    sql = "SELECT * FROM `Binary`";
    r = do_query(hdb, sql, &rec);
    ok(r == LIBMSI_RESULT_SUCCESS, "SELECT query failed: %d\n", r);

    check_record_string(rec, 1, "filename1");

    size = sizeof(buf);
    memset(buf, 0, size);
    in = libmsi_record_get_stream(rec, 2);
    ok(in, "Failed to get stream\n");
    size = g_input_stream_read(in, buf, sizeof(buf), NULL, NULL);
    ok(g_str_equal(buf, "just some words"),
        "Expected 'just some words', got %s\n", buf);
    g_object_unref(in);
    g_object_unref(rec);

    g_object_unref(hdb);

    unlink("bin_import/filename1.ibd");
    rmdir("bin_import");
    unlink("bin_import.idt");
}

static void test_markers(void)
{
    LibmsiDatabase *hdb;
    LibmsiRecord *rec;
    const char *sql;
    unsigned r = 0;

    hdb = create_db();
    ok( hdb, "failed to create db\n");

    rec = libmsi_record_new(3);
    libmsi_record_set_string(rec, 1, "Table");
    libmsi_record_set_string(rec, 2, "Apples");
    libmsi_record_set_string(rec, 3, "Oranges");

    /* try a legit create */
    sql = "CREATE TABLE `Table` ( `One` SHORT NOT NULL, `Two` CHAR(255) PRIMARY KEY `One`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    g_object_unref(rec);

    /* try table name as marker */
    rec = libmsi_record_new(1);
    libmsi_record_set_string(rec, 1, "Fable");
    sql = "CREATE TABLE `?` ( `One` SHORT NOT NULL, `Two` CHAR(255) PRIMARY KEY `One`)";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* verify that we just created a table called '?', not 'Fable' */
    r = try_query(hdb, "SELECT * from `Fable`");
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    r = try_query(hdb, "SELECT * from `?`");
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* try table name as marker without backticks */
    libmsi_record_set_string(rec, 1, "Mable");
    sql = "CREATE TABLE ? ( `One` SHORT NOT NULL, `Two` CHAR(255) PRIMARY KEY `One`)";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    /* try one column name as marker */
    libmsi_record_set_string(rec, 1, "One");
    sql = "CREATE TABLE `Mable` ( `?` SHORT NOT NULL, `Two` CHAR(255) PRIMARY KEY `One`)";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);
    g_object_unref(rec);

    /* try column names as markers */
    rec = libmsi_record_new(2);
    libmsi_record_set_string(rec, 1, "One");
    libmsi_record_set_string(rec, 2, "Two");
    sql = "CREATE TABLE `Mable` ( `?` SHORT NOT NULL, `?` CHAR(255) PRIMARY KEY `One`)";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);
    g_object_unref(rec);

    /* try names with backticks */
    rec = libmsi_record_new(3);
    libmsi_record_set_string(rec, 1, "One");
    libmsi_record_set_string(rec, 2, "Two");
    libmsi_record_set_string(rec, 3, "One");
    sql = "CREATE TABLE `Mable` ( `?` SHORT NOT NULL, `?` CHAR(255) PRIMARY KEY `?`)";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    /* try names with backticks, minus definitions */
    sql = "CREATE TABLE `Mable` ( `?`, `?` PRIMARY KEY `?`)";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    /* try names without backticks */
    sql = "CREATE TABLE `Mable` ( ? SHORT NOT NULL, ? CHAR(255) PRIMARY KEY ?)";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);
    g_object_unref(rec);

    /* try one long marker */
    rec = libmsi_record_new(1);
    libmsi_record_set_string(rec, 1, "`One` SHORT NOT NULL, `Two` CHAR(255) PRIMARY KEY `One`");
    sql = "CREATE TABLE `Mable` ( ? )";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);
    g_object_unref(rec);

    /* try all names as markers */
    rec = libmsi_record_new(4);
    libmsi_record_set_string(rec, 1, "Mable");
    libmsi_record_set_string(rec, 2, "One");
    libmsi_record_set_string(rec, 3, "Two");
    libmsi_record_set_string(rec, 4, "One");
    sql = "CREATE TABLE `?` ( `?` SHORT NOT NULL, `?` CHAR(255) PRIMARY KEY `?`)";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);
    g_object_unref(rec);

    /* try a legit insert */
    sql = "INSERT INTO `Table` ( `One`, `Two` ) VALUES ( 5, 'hello' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = try_query(hdb, "SELECT * from `Table`");
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* try values as markers */
    rec = libmsi_record_new(2);
    libmsi_record_set_int(rec, 1, 4);
    libmsi_record_set_string(rec, 2, "hi");
    sql = "INSERT INTO `Table` ( `One`, `Two` ) VALUES ( ?, '?' )";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    g_object_unref(rec);

    /* try column names and values as markers */
    rec = libmsi_record_new(4);
    libmsi_record_set_string(rec, 1, "One");
    libmsi_record_set_string(rec, 2, "Two");
    libmsi_record_set_int(rec, 3, 5);
    libmsi_record_set_string(rec, 4, "hi");
    sql = "INSERT INTO `Table` ( `?`, `?` ) VALUES ( ?, '?' )";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);
    g_object_unref(rec);

    /* try column names as markers */
    rec = libmsi_record_new(2);
    libmsi_record_set_string(rec, 1, "One");
    libmsi_record_set_string(rec, 2, "Two");
    sql = "INSERT INTO `Table` ( `?`, `?` ) VALUES ( 3, 'yellow' )";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);
    g_object_unref(rec);

    /* try table name as a marker */
    rec = libmsi_record_new(1);
    libmsi_record_set_string(rec, 1, "Table");
    sql = "INSERT INTO `?` ( `One`, `Two` ) VALUES ( 2, 'green' )";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    g_object_unref(rec);

    /* try table name and values as markers */
    rec = libmsi_record_new(3);
    libmsi_record_set_string(rec, 1, "Table");
    libmsi_record_set_int(rec, 2, 10);
    libmsi_record_set_string(rec, 3, "haha");
    sql = "INSERT INTO `?` ( `One`, `Two` ) VALUES ( ?, '?' )";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_FUNCTION_FAILED, "Expected LIBMSI_RESULT_FUNCTION_FAILED, got %d\n", r);
    g_object_unref(rec);

    /* try all markers */
    rec = libmsi_record_new(5);
    libmsi_record_set_string(rec, 1, "Table");
    libmsi_record_set_string(rec, 1, "One");
    libmsi_record_set_string(rec, 1, "Two");
    libmsi_record_set_int(rec, 2, 10);
    libmsi_record_set_string(rec, 3, "haha");
    sql = "INSERT INTO `?` ( `?`, `?` ) VALUES ( ?, '?' )";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);
    g_object_unref(rec);

    /* insert an integer as a string */
    rec = libmsi_record_new(2);
    libmsi_record_set_string(rec, 1, "11");
    libmsi_record_set_string(rec, 2, "hi");
    sql = "INSERT INTO `Table` ( `One`, `Two` ) VALUES ( ?, '?' )";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    g_object_unref(rec);

    /* leave off the '' for the string */
    rec = libmsi_record_new(2);
    libmsi_record_set_int(rec, 1, 12);
    libmsi_record_set_string(rec, 2, "hi");
    sql = "INSERT INTO `Table` ( `One`, `Two` ) VALUES ( ?, ? )";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    g_object_unref(rec);

    g_object_unref(hdb);
    unlink(msifile);
}

#define MY_NQUERIES 4000    /* Largest installer I've seen uses < 2k */
static void test_handle_limit(void)
{
    int i;
    LibmsiDatabase *hdb;
    LibmsiQuery *hqueries[MY_NQUERIES];
    unsigned r = 0;

    /* create an empty db */
    hdb = create_db();
    ok( hdb, "failed to create db\n");

    memset(hqueries, 0, sizeof(hqueries));

    for (i=0; i<MY_NQUERIES; i++) {
        static char szQueryBuf[256] = "SELECT * from `_Tables`";
        hqueries[i] = libmsi_query_new(hdb, szQueryBuf, NULL);
        if( hqueries[i] == 0 || (i && (hqueries[i] == hqueries[i-1])))
            break;
    }

    ok( i == MY_NQUERIES, "problem opening queries\n");

    for (i=0; i<MY_NQUERIES; i++) {
        if (hqueries[i] != 0 && hqueries[i] != (void*)0xdeadbeeb) {
            libmsi_query_close(hqueries[i], NULL);
            g_object_unref(hqueries[i]);
        }
    }

    ok( i == MY_NQUERIES, "problem closing queries\n");

    g_object_unref(hdb);
}

/* data for generating a transform */

/* tables transform names - encoded as they would be in an msi database file */
static const WCHAR name1[] = { 0x4840, 0x3a8a, 0x481b, 0 }; /* AAR */
static const WCHAR name2[] = { 0x4840, 0x3b3f, 0x43f2, 0x4438, 0x45b1, 0 }; /* _Columns */
static const WCHAR name3[] = { 0x4840, 0x3f7f, 0x4164, 0x422f, 0x4836, 0 }; /* _Tables */
static const WCHAR name4[] = { 0x4840, 0x3f3f, 0x4577, 0x446c, 0x3b6a, 0x45e4, 0x4824, 0 }; /* _StringData */
static const WCHAR name5[] = { 0x4840, 0x3f3f, 0x4577, 0x446c, 0x3e6a, 0x44b2, 0x482f, 0 }; /* _StringPool */
static const WCHAR name6[] = { 0x4840, 0x3e16, 0x4818, 0}; /* MOO */
static const WCHAR name7[] = { 0x4840, 0x3c8b, 0x3a97, 0x409b, 0 }; /* BINARY */
static const WCHAR name8[] = { 0x3c8b, 0x3a97, 0x409b, 0x387e, 0 }; /* BINARY.1 */
static const WCHAR name9[] = { 0x4840, 0x4559, 0x44f2, 0x4568, 0x4737, 0 }; /* Property */

/* data in each table */
static const WCHAR data1[] = { /* AAR */
    0x0201, 0x0008, 0x8001,  /* 0x0201 = add row (1), two shorts */
    0x0201, 0x0009, 0x8002,
};
static const WCHAR data2[] = { /* _Columns */
    0x0401, 0x0001, 0x8003, 0x0002, 0x9502,
    0x0401, 0x0001, 0x8004, 0x0003, 0x9502,
    0x0401, 0x0005, 0x0000, 0x0006, 0xbdff,  /* 0x0401 = add row (1), 4 shorts */
    0x0401, 0x0005, 0x0000, 0x0007, 0x8502,
    0x0401, 0x000a, 0x0000, 0x000a, 0xad48,
    0x0401, 0x000a, 0x0000, 0x000b, 0x9d00,
};
static const WCHAR data3[] = { /* _Tables */
    0x0101, 0x0005, /* 0x0101 = add row (1), 1 short */
    0x0101, 0x000a,
};
static const char data4[] = /* _StringData */
    "MOOCOWPIGcAARCARBARvwbmwPropertyValuepropval";  /* all the strings squashed together */
static const WCHAR data5[] = { /* _StringPool */
/*  len, refs */
    0,   0,    /* string 0 ''    */
    3,   2,    /* string 1 'MOO' */
    3,   1,    /* string 2 'COW' */
    3,   1,    /* string 3 'PIG' */
    1,   1,    /* string 4 'c'   */
    3,   3,    /* string 5 'AAR' */
    3,   1,    /* string 6 'CAR' */
    3,   1,    /* string 7 'BAR' */
    2,   1,    /* string 8 'vw'  */
    3,   1,    /* string 9 'bmw' */
    8,   4,    /* string 10 'Property' */
    5,   1,    /* string 11 'Value' */
    4,   1,    /* string 12 'prop' */
    3,   1,    /* string 13 'val' */
};
/* update row, 0x0002 is a bitmask of present column data, keys are excluded */
static const WCHAR data6[] = { /* MOO */
    0x000a, 0x8001, 0x0004, 0x8005, /* update row */
    0x0000, 0x8003,         /* delete row */
};

static const WCHAR data7[] = { /* BINARY */
    0x0201, 0x8001, 0x0001,
};

static const char data8[] =  /* stream data for the BINARY table */
    "naengmyon";

static const WCHAR data9[] = { /* Property */
    0x0201, 0x000c, 0x000d,
};

static const struct {
    const WCHAR *name;
    const void *data;
    unsigned size;
} table_transform_data[] =
{
    { name1, data1, sizeof data1 },
    { name2, data2, sizeof data2 },
    { name3, data3, sizeof data3 },
    { name4, data4, sizeof data4 - 1 },
    { name5, data5, sizeof data5 },
    { name6, data6, sizeof data6 },
    { name7, data7, sizeof data7 },
    { name8, data8, sizeof data8 - 1 },
    { name9, data9, sizeof data9 },
};

#define NUM_TRANSFORM_TABLES (sizeof table_transform_data/sizeof table_transform_data[0])

static void generate_transform_manual(void)
{
#ifdef _WIN32
    IStorage *stg = NULL;
    IStream *stm;
    WCHAR name[0x20];
    HRESULT r = 0;
    unsigned i, count;
    const unsigned mode = STGM_CREATE|STGM_READWRITE|STGM_DIRECT|STGM_SHARE_EXCLUSIVE;

    const CLSID CLSID_MsiTransform = { 0xc1082,0,0,{0xc0,0,0,0,0,0,0,0x46}};

    MultiByteToWideChar(CP_ACP, 0, mstfile, -1, name, 0x20);

    r = StgCreateDocfile(name, mode, 0, &stg);
    ok(r == S_OK, "failed to create storage\n");
    if (!stg)
        return;

    r = IStorage_SetClass( stg, &CLSID_MsiTransform );
    ok(r == S_OK, "failed to set storage type\n");

    for (i=0; i<NUM_TRANSFORM_TABLES; i++)
    {
        r = IStorage_CreateStream( stg, table_transform_data[i].name,
                            STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm );
        if (FAILED(r))
        {
            ok(0, "failed to create stream %08x\n", r);
            continue;
        }

        r = IStream_Write( stm, table_transform_data[i].data,
                          table_transform_data[i].size, &count );
        if (FAILED(r) || count != table_transform_data[i].size)
            ok(0, "failed to write stream\n");
        IStream_Release(stm);
    }

    IStorage_Release(stg);
#endif
}

static unsigned set_summary_info(LibmsiDatabase *hdb)
{
    GError *error = NULL;
    unsigned res;
    LibmsiSummaryInfo *suminfo;

    /* build summary info */
    suminfo = libmsi_summary_info_new(hdb, 7, NULL);
    ok(suminfo , "Failed to open summaryinfo\n" );

    libmsi_summary_info_set_string (suminfo, 2, "Installation Database", &error);
    ok(!error, "Failed to set summary info\n");

    libmsi_summary_info_set_string (suminfo, 3, "Installation Database", &error);
    ok(!error, "Failed to set summary info\n");

    libmsi_summary_info_set_string (suminfo, 4, "Wine Hackers", &error);
    ok(!error, "Failed to set summary info\n");

    libmsi_summary_info_set_string (suminfo, 7, ";1033,2057", &error);
    ok(!error, "Failed to set summary info\n");

    libmsi_summary_info_set_string (suminfo, 9, "{913B8D18-FBB6-4CAC-A239-C74C11E3FA74}", &error);
    ok(!error, "Failed to set summary info\n");

    libmsi_summary_info_set_int (suminfo, 14, 100, &error);
    ok(!error, "Failed to set summary info\n");

    libmsi_summary_info_set_int (suminfo, 15, 0, &error);
    ok(!error, "Failed to set summary info\n");

    res = libmsi_summary_info_persist(suminfo, NULL);
    ok( res , "Failed to make summary info persist\n" );

    g_object_unref( suminfo);

    return res;
}

static LibmsiDatabase *create_package_db(const char *filename)
{
    LibmsiDatabase *hdb = 0;
    unsigned res;

    unlink(msifile);

    /* create an empty database */
    hdb = libmsi_database_new(filename, LIBMSI_DB_FLAGS_CREATE, NULL, NULL );
    ok(hdb , "Failed to create database\n" );

    res = libmsi_database_commit(hdb, NULL);
    ok(res, "Failed to commit database\n");

    res = set_summary_info(hdb);
    ok( res == LIBMSI_RESULT_SUCCESS , "Failed to set summary info\n" );

    res = create_directory_table(hdb);
    ok( res == LIBMSI_RESULT_SUCCESS , "Failed to create directory table\n" );

    return hdb;
}

static void test_try_transform(void)
{
#ifdef _WIN32
    LibmsiDatabase *hdb;
    LibmsiQuery *hquery;
    LibmsiRecord *hrec;
    const char *sql;
    unsigned r = 0;
    unsigned sz;
    char buffer[MAX_PATH];

    unlink(msifile);
    unlink(mstfile);

    /* create the database */
    hdb = create_package_db(msifile);
    ok(hdb, "Failed to create package db\n");

    sql = "CREATE TABLE `MOO` ( `NOO` SHORT NOT NULL, `OOO` CHAR(255) PRIMARY KEY `NOO`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add table\n");

    sql = "INSERT INTO `MOO` ( `NOO`, `OOO` ) VALUES ( 1, 'a' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add row\n");

    sql = "INSERT INTO `MOO` ( `NOO`, `OOO` ) VALUES ( 2, 'b' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add row\n");

    sql = "INSERT INTO `MOO` ( `NOO`, `OOO` ) VALUES ( 3, 'c' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add row\n");

    sql = "CREATE TABLE `BINARY` ( `ID` SHORT NOT NULL, `BLOB` OBJECT PRIMARY KEY `ID`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add table\n");

    hrec = libmsi_record_new(2);
    r = libmsi_record_set_int(hrec, 1, 2);
    ok(r, "failed to set integer\n");

    write_file("testdata.bin", "lamyon", 6);
    r = libmsi_record_load_stream(hrec, 2, "testdata.bin");
    ok(r, "failed to set stream\n");

    sql = "INSERT INTO `BINARY` ( `ID`, `BLOB` ) VALUES ( ?, ? )";
    r = run_query(hdb, hrec, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add row with blob\n");

    g_object_unref(hrec);

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "Failed to commit database\n");

    g_object_unref( hdb );
    unlink("testdata.bin");

    generate_transform_manual();

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_TRANSACT, NULL, NULL );
    ok(hdb , "Failed to create database\n" );

    r = libmsi_database_apply_transform(hdb, mstfile, NULL);
    ok(r, "libmsi_database_apply_transform() failed\n");

    libmsi_database_commit(hdb, NULL);

    /* check new values */
    hrec = 0;
    sql = "select `BAR`,`CAR` from `AAR` where `BAR` = 1 AND `CAR` = 'vw'";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "select query failed\n");
    g_object_unref(hrec);

    sql = "select `BAR`,`CAR` from `AAR` where `BAR` = 2 AND `CAR` = 'bmw'";
    hrec = 0;
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "select query failed\n");
    g_object_unref(hrec);

    /* check updated values */
    hrec = 0;
    sql = "select `NOO`,`OOO` from `MOO` where `NOO` = 1 AND `OOO` = 'c'";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "select query failed\n");
    g_object_unref(hrec);

    /* check unchanged value */
    hrec = 0;
    sql = "select `NOO`,`OOO` from `MOO` where `NOO` = 2 AND `OOO` = 'b'";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "select query failed\n");
    g_object_unref(hrec);

    /* check deleted value */
    hrec = 0;
    sql = "select * from `MOO` where `NOO` = 3";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "select query failed\n");
    ok(hrec == NULL);

    /* check added stream */
    hrec = 0;
    sql = "select `BLOB` from `BINARY` where `ID` = 1";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "select query failed\n");

    /* check the contents of the stream */
    in = libmsi_record_get_stream(rec, 1);
    ok(in, "Failed to get stream\n");
    sz = g_input_stream_read(in, buffer, sizeof(buffer), NULL, NULL);
    ok(!memcmp(buffer, "naengmyon", 9), "stream data was wrong\n");
    ok(sz == 9, "stream data was wrong size\n");
    if (hrec) g_object_unref(hrec);
    g_object_unref(in);

    /* check the validity of the table with a deleted row */
    hrec = 0;
    sql = "select * from `MOO`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "open query failed\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "query execute failed\n");

    hrec = libmsi_query_fetch(query, NULL);
    ok(hrec, "Expected result\n");

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(hrec, 2, "c");

    r = libmsi_record_get_int(hrec, 3);
    ok(r == 0x80000000, "Expected 0x80000000, got %d\n", r);

    r = libmsi_record_get_int(hrec, 4);
    ok(r == 5, "Expected 5, got %d\n", r);

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(query, NULL);
    ok(hrec, "Expected result\n");

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 2, "Expected 2, got %d\n", r);

    check_record_string(hrec, 2, "b");

    r = libmsi_record_get_int(hrec, 3);
    ok(r == 0x80000000, "Expected 0x80000000, got %d\n", r);

    r = libmsi_record_get_int(hrec, 4);
    ok(r == 0x80000000, "Expected 0x80000000, got %d\n", r);

    g_object_unref(hrec);

    query_check_no_more(hquery);

    g_object_unref(hrec);
    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

#if 0
    LibmsiObject *hpkg = 0;

    /* check that the property was added */
    r = package_from_db(hdb, &hpkg);
    if (r == ERROR_INSTALL_PACKAGE_REJECTED)
    {
        skip("Not enough rights to perform tests\n");
        goto error;
    }
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %u\n", r);

    sz = MAX_PATH;
    r = MsiGetProperty(hpkg, "prop", buffer, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    ok(g_str_equal(buffer, "val"), "Expected val, got %s\n", buffer);

    g_object_unref(hpkg);
#endif

error:
    g_object_unref(hdb);
    unlink(msifile);
    unlink(mstfile);
#endif
}

struct join_res
{
    const char one[32];
    const char two[32];
};

struct join_res_4col
{
    const char one[32];
    const char two[32];
    const char three[32];
    const char four[32];
};

struct join_res_uint
{
    unsigned one;
    unsigned two;
    unsigned three;
    unsigned four;
    unsigned five;
    unsigned six;
};

static const struct join_res join_res_first[] =
{
    { "alveolar", "procerus" },
    { "septum", "procerus" },
    { "septum", "nasalis" },
    { "ramus", "nasalis" },
    { "malar", "mentalis" },
};

static const struct join_res join_res_second[] =
{
    { "nasal", "septum" },
    { "mandible", "ramus" },
};

static const struct join_res join_res_third[] =
{
    { "msvcp.dll", "abcdefgh" },
    { "msvcr.dll", "ijklmnop" },
};

static const struct join_res join_res_fourth[] =
{
    { "msvcp.dll.01234", "single.dll.31415" },
};

static const struct join_res join_res_fifth[] =
{
    { "malar", "procerus" },
};

static const struct join_res join_res_sixth[] =
{
    { "malar", "procerus" },
    { "malar", "procerus" },
    { "malar", "nasalis" },
    { "malar", "nasalis" },
    { "malar", "nasalis" },
    { "malar", "mentalis" },
};

static const struct join_res join_res_seventh[] =
{
    { "malar", "nasalis" },
    { "malar", "nasalis" },
    { "malar", "nasalis" },
};

static const struct join_res_4col join_res_eighth[] =
{
    { "msvcp.dll", "msvcp.dll.01234", "msvcp.dll.01234", "abcdefgh" },
    { "msvcr.dll", "msvcr.dll.56789", "msvcp.dll.01234", "abcdefgh" },
    { "msvcp.dll", "msvcp.dll.01234", "msvcr.dll.56789", "ijklmnop" },
    { "msvcr.dll", "msvcr.dll.56789", "msvcr.dll.56789", "ijklmnop" },
    { "msvcp.dll", "msvcp.dll.01234", "single.dll.31415", "msvcp.dll" },
    { "msvcr.dll", "msvcr.dll.56789", "single.dll.31415", "msvcp.dll" },
};

static const struct join_res_uint join_res_ninth[] =
{
    { 1, 2, 3, 4, 7, 8 },
    { 1, 2, 5, 6, 7, 8 },
    { 1, 2, 3, 4, 9, 10 },
    { 1, 2, 5, 6, 9, 10 },
    { 1, 2, 3, 4, 11, 12 },
    { 1, 2, 5, 6, 11, 12 },
};

static void test_join(void)
{
    GError *error = NULL;
    LibmsiDatabase *hdb;
    LibmsiQuery *hquery;
    LibmsiRecord *hrec;
    const char *sql;
    char buf[256];
    unsigned r = 0, count;
    unsigned size, i;
    bool data_correct;
    gchar *str;

    hdb = create_db();
    ok( hdb, "failed to create db\n");

    r = create_component_table( hdb );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot create Component table: %d\n", r );

    r = add_component_entry( hdb, "'zygomatic', 'malar', 'INSTALLDIR', 0, '', ''" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add component: %d\n", r );

    r = add_component_entry( hdb, "'maxilla', 'alveolar', 'INSTALLDIR', 0, '', ''" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add component: %d\n", r );

    r = add_component_entry( hdb, "'nasal', 'septum', 'INSTALLDIR', 0, '', ''" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add component: %d\n", r );

    r = add_component_entry( hdb, "'mandible', 'ramus', 'INSTALLDIR', 0, '', ''" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add component: %d\n", r );

    r = create_feature_components_table( hdb );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot create FeatureComponents table: %d\n", r );

    r = add_feature_components_entry( hdb, "'procerus', 'maxilla'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add feature components: %d\n", r );

    r = add_feature_components_entry( hdb, "'procerus', 'nasal'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add feature components: %d\n", r );

    r = add_feature_components_entry( hdb, "'nasalis', 'nasal'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add feature components: %d\n", r );

    r = add_feature_components_entry( hdb, "'nasalis', 'mandible'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add feature components: %d\n", r );

    r = add_feature_components_entry( hdb, "'nasalis', 'notacomponent'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add feature components: %d\n", r );

    r = add_feature_components_entry( hdb, "'mentalis', 'zygomatic'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add feature components: %d\n", r );

    r = create_std_dlls_table( hdb );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot create StdDlls table: %d\n", r );

    r = add_std_dlls_entry( hdb, "'msvcp.dll', 'msvcp.dll.01234'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add std dlls: %d\n", r );

    r = add_std_dlls_entry( hdb, "'msvcr.dll', 'msvcr.dll.56789'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add std dlls: %d\n", r );

    r = create_binary_table( hdb );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot create Binary table: %d\n", r );

    r = add_binary_entry( hdb, "'msvcp.dll.01234', 'abcdefgh'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add binary: %d\n", r );

    r = add_binary_entry( hdb, "'msvcr.dll.56789', 'ijklmnop'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add binary: %d\n", r );

    r = add_binary_entry( hdb, "'single.dll.31415', 'msvcp.dll'" );
    ok( r == LIBMSI_RESULT_SUCCESS, "cannot add binary: %d\n", r );

    sql = "CREATE TABLE `One` (`A` SHORT, `B` SHORT PRIMARY KEY `A`)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot create table: %d\n", r );

    sql = "CREATE TABLE `Two` (`C` SHORT, `D` SHORT PRIMARY KEY `C`)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot create table: %d\n", r );

    sql = "CREATE TABLE `Three` (`E` SHORT, `F` SHORT PRIMARY KEY `E`)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot create table: %d\n", r );

    sql = "INSERT INTO `One` (`A`, `B`) VALUES (1, 2)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot insert into table: %d\n", r );

    sql = "INSERT INTO `Two` (`C`, `D`) VALUES (3, 4)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot insert into table: %d\n", r );

    sql = "INSERT INTO `Two` (`C`, `D`) VALUES (5, 6)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot insert into table: %d\n", r );

    sql = "INSERT INTO `Three` (`E`, `F`) VALUES (7, 8)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot insert into table: %d\n", r );

    sql = "INSERT INTO `Three` (`E`, `F`) VALUES (9, 10)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot insert into table: %d\n", r );

    sql = "INSERT INTO `Three` (`E`, `F`) VALUES (11, 12)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot insert into table: %d\n", r );

    sql = "CREATE TABLE `Four` (`G` SHORT, `H` SHORT PRIMARY KEY `G`)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot create table: %d\n", r );

    sql = "CREATE TABLE `Five` (`I` SHORT, `J` SHORT PRIMARY KEY `I`)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot create table: %d\n", r );

    sql = "INSERT INTO `Five` (`I`, `J`) VALUES (13, 14)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot insert into table: %d\n", r );

    sql = "INSERT INTO `Five` (`I`, `J`) VALUES (15, 16)";
    r = run_query( hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot insert into table: %d\n", r );

    sql = "SELECT `Component`.`ComponentId`, `FeatureComponents`.`Feature_` "
            "FROM `Component`, `FeatureComponents` "
            "WHERE `Component`.`Component` = `FeatureComponents`.`Component_` "
            "ORDER BY `Feature_`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    i = 0;
    while ((hrec = libmsi_query_fetch(hquery, &error)) != NULL)
    {
        count = libmsi_record_get_field_count( hrec );
        ok( count == 2, "Expected 2 record fields, got %d\n", count );

        check_record_string(hrec, 1, join_res_first[i].one);
        check_record_string(hrec, 2, join_res_first[i].two);

        i++;
        g_object_unref(hrec);
    }

    ok( i == 5, "Expected 5 rows, got %d\n", i );
    g_assert_no_error(error);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    /* try a join without a WHERE condition */
    sql = "SELECT `Component`.`ComponentId`, `FeatureComponents`.`Feature_` "
            "FROM `Component`, `FeatureComponents` ";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    i = 0;
    while ((hrec = libmsi_query_fetch(hquery, &error)) != NULL)
    {
        i++;
        g_object_unref(hrec);
    }
    ok( i == 24, "Expected 24 rows, got %d\n", i );
    g_clear_error(&error);
    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT DISTINCT Component, ComponentId FROM FeatureComponents, Component "
            "WHERE FeatureComponents.Component_=Component.Component "
            "AND (Feature_='nasalis') ORDER BY Feature_";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    i = 0;
    data_correct = true;
    while ((hrec = libmsi_query_fetch(hquery, &error)) != NULL)
    {
        count = libmsi_record_get_field_count( hrec );
        ok( count == 2, "Expected 2 record fields, got %d\n", count );

        str = libmsi_record_get_string(hrec, 1);
        ok(str, "failed to get record string\n");
        if( strcmp(str, join_res_second[i].one))
            data_correct = false;
        g_free(str);

        str = libmsi_record_get_string(hrec, 2);
        ok(str, "failed to get record string\n");
        if( strcmp(str, join_res_second[i].two))
            data_correct = false;
        g_free(str);

        i++;
        g_object_unref(hrec);
    }

    ok( data_correct, "data returned in the wrong order\n");

    ok( i == 2, "Expected 2 rows, got %d\n", i );
    g_assert_no_error(error);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT `StdDlls`.`File`, `Binary`.`Data` "
            "FROM `StdDlls`, `Binary` "
            "WHERE `StdDlls`.`Binary_` = `Binary`.`Name` "
            "ORDER BY `File`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    i = 0;
    data_correct = true;
    while ((hrec = libmsi_query_fetch(hquery, &error)) != NULL)
    {
        count = libmsi_record_get_field_count( hrec );
        ok( count == 2, "Expected 2 record fields, got %d\n", count );

        str = libmsi_record_get_string(hrec, 1);
        ok(str, "failed to get record string\n", str);
        if (strcmp(str, join_res_third[i].one))
            data_correct = false;
        g_free(str);

        str = libmsi_record_get_string(hrec, 2);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_third[i].two))
            data_correct = false;
        g_free(str);

        i++;
        g_object_unref(hrec);
    }
    ok( data_correct, "data returned in the wrong order\n");

    ok( i == 2, "Expected 2 rows, got %d\n", i );
    g_assert_no_error(error);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT `StdDlls`.`Binary_`, `Binary`.`Name` "
            "FROM `StdDlls`, `Binary` "
            "WHERE `StdDlls`.`File` = `Binary`.`Data` "
            "ORDER BY `Name`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    i = 0;
    data_correct = true;
    while ((hrec = libmsi_query_fetch(hquery, &error)) != NULL)
    {
        count = libmsi_record_get_field_count( hrec );
        ok( count == 2, "Expected 2 record fields, got %d\n", count );

        str = libmsi_record_get_string(hrec, 1);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_fourth[i].one))
            data_correct = false;
        g_free(str);

        str = libmsi_record_get_string( hrec, 2);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_fourth[i].two))
            data_correct = false;
        g_free(str);

        i++;
        g_object_unref(hrec);
    }
    ok( data_correct, "data returned in the wrong order\n");

    ok( i == 1, "Expected 1 rows, got %d\n", i );
    g_assert_no_error(error);
    g_clear_error(&error);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT `Component`.`ComponentId`, `FeatureComponents`.`Feature_` "
            "FROM `Component`, `FeatureComponents` "
            "WHERE `Component`.`Component` = 'zygomatic' "
            "AND `FeatureComponents`.`Component_` = 'maxilla' "
            "ORDER BY `Feature_`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    i = 0;
    data_correct = true;
    while ((hrec = libmsi_query_fetch(hquery, &error)) != NULL)
    {
        count = libmsi_record_get_field_count( hrec );
        ok( count == 2, "Expected 2 record fields, got %d\n", count );

        str = libmsi_record_get_string(hrec, 1);
        ok(str, "failed to get record string: %d\n", r );
        if (strcmp(str, join_res_fifth[i].one))
            data_correct = false;
        g_free(str);

        str = libmsi_record_get_string(hrec, 2);
        ok(str, "failed to get record string\n");
        if(strcmp(str, join_res_fifth[i].two))
            data_correct = false;
        g_free(str);

        i++;
        g_object_unref(hrec);
    }
    ok( data_correct, "data returned in the wrong order\n");

    ok( i == 1, "Expected 1 rows, got %d\n", i );
    g_assert_no_error(error);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT `Component`.`ComponentId`, `FeatureComponents`.`Feature_` "
            "FROM `Component`, `FeatureComponents` "
            "WHERE `Component` = 'zygomatic' "
            "ORDER BY `Feature_`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    i = 0;
    data_correct = true;
    while ((hrec = libmsi_query_fetch(hquery, &error)) != NULL)
    {
        count = libmsi_record_get_field_count( hrec );
        ok( count == 2, "Expected 2 record fields, got %d\n", count );

        str = libmsi_record_get_string(hrec, 1);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_sixth[i].one))
            data_correct = false;
        g_free(str);

        str = libmsi_record_get_string( hrec, 2);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_sixth[i].two))
            data_correct = false;
        g_free(str);

        i++;
        g_object_unref(hrec);
    }
    ok( data_correct, "data returned in the wrong order\n");

    ok( i == 6, "Expected 6 rows, got %d\n", i );
    g_assert_no_error(error);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT `Component`.`ComponentId`, `FeatureComponents`.`Feature_` "
            "FROM `Component`, `FeatureComponents` "
            "WHERE `Component` = 'zygomatic' "
            "AND `Feature_` = 'nasalis' "
            "ORDER BY `Feature_`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    i = 0;
    data_correct = true;
    while ((hrec = libmsi_query_fetch(hquery, &error)) != NULL)
    {
        count = libmsi_record_get_field_count( hrec );
        ok( count == 2, "Expected 2 record fields, got %d\n", count );

        str = libmsi_record_get_string( hrec, 1);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_seventh[i].one))
            data_correct = false;
        g_free(str);

        str = libmsi_record_get_string(hrec, 2);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_seventh[i].two))
            data_correct = false;
        g_free(str);

        i++;
        g_object_unref(hrec);
    }

    ok( data_correct, "data returned in the wrong order\n");
    ok( i == 3, "Expected 3 rows, got %d\n", i );
    g_assert_no_error(error);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT `StdDlls`.`File`, `Binary`.`Data` "
            "FROM `StdDlls`, `Binary` ";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    i = 0;
    data_correct = true;
    while ((hrec = libmsi_query_fetch(hquery, &error)) != NULL)
    {
        count = libmsi_record_get_field_count( hrec );
        ok( count == 2, "Expected 2 record fields, got %d\n", count );

        str = libmsi_record_get_string(hrec, 1);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_eighth[i].one))
            data_correct = false;
        g_free(str);

        str = libmsi_record_get_string(hrec, 2);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_eighth[i].four))
            data_correct = false;
        g_free(str);

        i++;
        g_object_unref(hrec);
    }

    ok( data_correct, "data returned in the wrong order\n");
    ok( i == 6, "Expected 6 rows, got %d\n", i );
    g_assert_no_error(error);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT * FROM `StdDlls`, `Binary` ";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    i = 0;
    data_correct = true;
    while ((hrec = libmsi_query_fetch(hquery, &error)) != NULL)
    {
        count = libmsi_record_get_field_count( hrec );
        ok( count == 4, "Expected 4 record fields, got %d\n", count );

        str = libmsi_record_get_string(hrec, 1);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_eighth[i].one))
            data_correct = false;
        g_free(str);

        str = libmsi_record_get_string(hrec, 2);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_eighth[i].two))
            data_correct = false;
        g_free(str);

        str = libmsi_record_get_string(hrec, 3);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_eighth[i].three))
            data_correct = false;
        g_free(str);

        str = libmsi_record_get_string(hrec, 4);
        ok(str, "failed to get record string\n");
        if (strcmp(str, join_res_eighth[i].four))
            data_correct = false;
        g_free(str);

        i++;
        g_object_unref(hrec);
    }
    ok( data_correct, "data returned in the wrong order\n");

    ok( i == 6, "Expected 6 rows, got %d\n", i );
    g_assert_no_error(error);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT * FROM `One`, `Two`, `Three` ";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    i = 0;
    data_correct = true;
    while ((hrec = libmsi_query_fetch(hquery, &error)) != NULL)
    {
        count = libmsi_record_get_field_count( hrec );
        ok( count == 6, "Expected 6 record fields, got %d\n", count );

        r = libmsi_record_get_int( hrec, 1 );
        if( r != join_res_ninth[i].one )
            data_correct = false;

        r = libmsi_record_get_int( hrec, 2 );
        if( r != join_res_ninth[i].two )
            data_correct = false;

        r = libmsi_record_get_int( hrec, 3 );
        if( r != join_res_ninth[i].three )
            data_correct = false;

        r = libmsi_record_get_int( hrec, 4 );
        if( r != join_res_ninth[i].four )
            data_correct = false;

        r = libmsi_record_get_int( hrec, 5 );
        if( r != join_res_ninth[i].five )
            data_correct = false;

        r = libmsi_record_get_int( hrec, 6);
        if( r != join_res_ninth[i].six )
            data_correct = false;

        i++;
        g_object_unref(hrec);
    }
    ok( data_correct, "data returned in the wrong order\n");

    ok( i == 6, "Expected 6 rows, got %d\n", i );
    g_assert_no_error(error);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT * FROM `Four`, `Five`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "failed to open query\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok( r, "failed to execute query: %d\n", r );

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT * FROM `Nonexistent`, `One`";
    hquery = libmsi_query_new(hdb, sql, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_BAD_QUERY_SYNTAX);
    g_clear_error(&error);
    g_object_unref(hdb);
    unlink(msifile);
}

static void test_temporary_table(void)
{
    GError *error = NULL;
    gboolean cond;
    LibmsiDatabase *hdb = 0;
    LibmsiQuery *query = 0;
    LibmsiRecord *rec;
    const char *sql;
    unsigned r = 0;
    char buf[0x10];
    unsigned sz;

    hdb = create_db();
    ok( hdb, "failed to create db\n");

    cond = libmsi_database_is_table_persistent(hdb, "_Tables", &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_INVALID_TABLE);
    g_clear_error(&error);

    cond = libmsi_database_is_table_persistent(hdb, "_Columns", &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_INVALID_TABLE);
    g_clear_error(&error);

    cond = libmsi_database_is_table_persistent(hdb, "_Storages", &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_INVALID_TABLE);
    g_clear_error(&error);

    cond = libmsi_database_is_table_persistent(hdb, "_Streams", &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_INVALID_TABLE);
    g_clear_error(&error);

    sql = "CREATE TABLE `P` ( `B` SHORT NOT NULL, `C` CHAR(255) PRIMARY KEY `C`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add table\n");

    cond = libmsi_database_is_table_persistent(hdb, "P", NULL);
    ok(cond, "wrong return condition\n");

    sql = "CREATE TABLE `P2` ( `B` SHORT NOT NULL, `C` CHAR(255) PRIMARY KEY `C`) HOLD";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add table\n");

    cond = libmsi_database_is_table_persistent(hdb, "P2", NULL);
    ok(cond, "wrong return condition\n");

    sql = "CREATE TABLE `T` ( `B` SHORT NOT NULL TEMPORARY, `C` CHAR(255) TEMPORARY PRIMARY KEY `C`) HOLD";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add table\n");

    cond = libmsi_database_is_table_persistent(hdb, "T", NULL);
    ok(!cond, "wrong return condition\n");

    sql = "CREATE TABLE `T2` ( `B` SHORT NOT NULL TEMPORARY, `C` CHAR(255) TEMPORARY PRIMARY KEY `C`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add table\n");

    query = NULL;
    sql = "SELECT * FROM `T2`";
    query = libmsi_query_new(hdb, sql, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_BAD_QUERY_SYNTAX);
    g_clear_error(&error);

    cond = libmsi_database_is_table_persistent(hdb, "T2", &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_INVALID_TABLE);
    g_clear_error(&error);

    sql = "CREATE TABLE `T3` ( `B` SHORT NOT NULL TEMPORARY, `C` CHAR(255) PRIMARY KEY `C`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add table\n");

    cond = libmsi_database_is_table_persistent(hdb, "T3", NULL);
    ok(cond, "wrong return condition\n");

    sql = "CREATE TABLE `T4` ( `B` SHORT NOT NULL, `C` CHAR(255) TEMPORARY PRIMARY KEY `C`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_FUNCTION_FAILED, "failed to add table\n");

    cond = libmsi_database_is_table_persistent(hdb, "T4", &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_INVALID_TABLE);
    g_clear_error(&error);

    sql = "CREATE TABLE `T5` ( `B` SHORT NOT NULL TEMP, `C` CHAR(255) TEMP PRIMARY KEY `C`) HOLD";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "failed to add table\n");

    sql = "select * from `T`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "failed to query table\n");

    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_TYPES, NULL);
    ok(rec, "failed to get column info\n");

    check_record_string(rec, 1, "G255");
    check_record_string(rec, 2, "j2");

    g_object_unref( rec );
    libmsi_query_close(query , NULL);
    g_object_unref( query );

    /* query the table data */
    rec = 0;
    r = do_query(hdb, "select * from `_Tables` where `Name` = 'T'", &rec);
    ok( r == LIBMSI_RESULT_SUCCESS, "temporary table exists in _Tables\n");
    g_object_unref( rec );

    /* query the column data */
    rec = 0;
    r = do_query(hdb, "select * from `_Columns` where `Table` = 'T' AND `Name` = 'B'", &rec);
    ok( r == LIBMSI_RESULT_SUCCESS, "temporary table exists in _Columns\n");
    g_assert(rec == NULL);

    r = do_query(hdb, "select * from `_Columns` where `Table` = 'T' AND `Name` = 'C'", &rec);
    ok( r == LIBMSI_RESULT_SUCCESS, "temporary table exists in _Columns\n");
    g_assert(rec == NULL);

    g_object_unref( hdb );

    unlink(msifile);
}

static void test_alter(void)
{
    gboolean cond;
    LibmsiDatabase *hdb = 0;
    const char *sql;
    unsigned r = 0;

    hdb = create_db();
    ok( hdb, "failed to create db\n");

    sql = "CREATE TABLE `T` ( `B` SHORT NOT NULL TEMPORARY, `C` CHAR(255) TEMPORARY PRIMARY KEY `C`) HOLD";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add table\n");

    cond = libmsi_database_is_table_persistent(hdb, "T", NULL);
    ok(!cond, "wrong return condition\n");

    sql = "ALTER TABLE `T` HOLD";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to hold table %d\n", r);

    sql = "ALTER TABLE `T` FREE";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to free table\n");

    sql = "ALTER TABLE `T` FREE";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to free table\n");

    sql = "ALTER TABLE `T` FREE";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "failed to free table\n");

    sql = "ALTER TABLE `T` HOLD";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "failed to hold table %d\n", r);

    /* table T is removed */
    sql = "SELECT * FROM `T`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    /* create the table again */
    sql = "CREATE TABLE `U` ( `A` INTEGER, `B` INTEGER PRIMARY KEY `B`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* up the ref count */
    sql = "ALTER TABLE `U` HOLD";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to free table\n");

    /* add column, no data type */
    sql = "ALTER TABLE `U` ADD `C`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "ALTER TABLE `U` ADD `C` INTEGER";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* add column C again */
    sql = "ALTER TABLE `U` ADD `C` INTEGER";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "ALTER TABLE `U` ADD `D` INTEGER TEMPORARY";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `U` ( `A`, `B`, `C`, `D` ) VALUES ( 1, 2, 3, 4 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "ALTER TABLE `U` ADD `D` INTEGER TEMPORARY HOLD";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `U` ( `A`, `B`, `C`, `D` ) VALUES ( 5, 6, 7, 8 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `U` WHERE `D` = 8";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "ALTER TABLE `U` ADD `D` INTEGER TEMPORARY FREE";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "ALTER COLUMN `D` FREE";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    /* drop the ref count */
    sql = "ALTER TABLE `U` FREE";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* table is not empty */
    sql = "SELECT * FROM `U`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* column D is removed */
    sql = "SELECT * FROM `U` WHERE `D` = 8";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `U` ( `A`, `B`, `C`, `D` ) VALUES ( 9, 10, 11, 12 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    /* add the column again */
    sql = "ALTER TABLE `U` ADD `E` INTEGER TEMPORARY HOLD";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* up the ref count */
    sql = "ALTER TABLE `U` HOLD";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `U` ( `A`, `B`, `C`, `E` ) VALUES ( 13, 14, 15, 16 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `U` WHERE `E` = 16";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* drop the ref count */
    sql = "ALTER TABLE `U` FREE";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `U` ( `A`, `B`, `C`, `E` ) VALUES ( 17, 18, 19, 20 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `U` WHERE `E` = 20";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* drop the ref count */
    sql = "ALTER TABLE `U` FREE";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* table still exists */
    sql = "SELECT * FROM `U`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* col E is removed */
    sql = "SELECT * FROM `U` WHERE `E` = 20";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `U` ( `A`, `B`, `C`, `E` ) VALUES ( 20, 21, 22, 23 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    /* drop the ref count once more */
    sql = "ALTER TABLE `U` FREE";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* table still exists */
    sql = "SELECT * FROM `U`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    g_object_unref( hdb );
    unlink(msifile);
}

static void test_integers(void)
{
    LibmsiDatabase *hdb = 0;
    LibmsiQuery *query = 0;
    LibmsiRecord *rec = 0;
    unsigned count, i;
    const char *sql;
    unsigned r = 0;

    /* just libmsi_database_open should not create a file */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "libmsi_database_open failed\n");

    /* create a table */
    sql = "CREATE TABLE `integers` ( "
            "`one` SHORT, `two` INT, `three` INTEGER, `four` LONG, "
            "`five` SHORT NOT NULL, `six` INT NOT NULL, "
            "`seven` INTEGER NOT NULL, `eight` LONG NOT NULL "
            "PRIMARY KEY `one`)";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "libmsi_query failed\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");
    r = libmsi_query_close(query, NULL);
    ok(r, "libmsi_query_close failed\n", NULL);
    g_object_unref(query);

    sql = "SELECT * FROM `integers`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");

    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_NAMES, NULL);
    ok(rec, "Expected result\n");
    count = libmsi_record_get_field_count(rec);
    ok(count == 8, "Expected 8, got %d\n", count);
    ok(check_record(rec, 1, "one"), "Expected one\n");
    ok(check_record(rec, 2, "two"), "Expected two\n");
    ok(check_record(rec, 3, "three"), "Expected three\n");
    ok(check_record(rec, 4, "four"), "Expected four\n");
    ok(check_record(rec, 5, "five"), "Expected five\n");
    ok(check_record(rec, 6, "six"), "Expected six\n");
    ok(check_record(rec, 7, "seven"), "Expected seven\n");
    ok(check_record(rec, 8, "eight"), "Expected eight\n");
    g_object_unref(rec);

    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_TYPES, NULL);
    ok(rec, "Expected result\n");
    count = libmsi_record_get_field_count(rec);
    ok(count == 8, "Expected 8, got %d\n", count);
    ok(check_record(rec, 1, "I2"), "Expected I2\n");
    ok(check_record(rec, 2, "I2"), "Expected I2\n");
    ok(check_record(rec, 3, "I2"), "Expected I2\n");
    ok(check_record(rec, 4, "I4"), "Expected I4\n");
    ok(check_record(rec, 5, "i2"), "Expected i2\n");
    ok(check_record(rec, 6, "i2"), "Expected i2\n");
    ok(check_record(rec, 7, "i2"), "Expected i2\n");
    ok(check_record(rec, 8, "i4"), "Expected i4\n");
    g_object_unref(rec);

    libmsi_query_close(query, NULL);
    g_object_unref(query);

    /* insert values into it, NULL where NOT NULL is specified */
    query = NULL;
    sql = "INSERT INTO `integers` ( `one`, `two`, `three`, `four`, `five`, `six`, `seven`, `eight` )"
        "VALUES('', '', '', '', '', '', '', '')";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(!r, "Expected LIBMSI_RESULT_FUNCTION_FAILED, got %d\n", r);

    libmsi_query_close(query, NULL);
    g_object_unref(query);

    sql = "SELECT * FROM `integers`";
    r = do_query(hdb, sql, &rec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_NO_MORE_ITEMS, got %d\n", r);
    ok(rec == NULL, "Must be null");

    /* insert legitimate values into it */
    query = NULL;
    sql = "INSERT INTO `integers` ( `one`, `two`, `three`, `four`, `five`, `six`, `seven`, `eight` )"
        "VALUES('', '2', '', '4', '5', '6', '7', '8')";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `integers`";
    r = do_query(hdb, sql, &rec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_record_get_field_count(rec);
    ok(r == 8, "record count wrong: %d\n", r);

    i = libmsi_record_get_int(rec, 1);
    ok(i == LIBMSI_NULL_INT, "Expected LIBMSI_NULL_INT, got %d\n", i);
    i = libmsi_record_get_int(rec, 3);
    ok(i == LIBMSI_NULL_INT, "Expected LIBMSI_NULL_INT, got %d\n", i);
    i = libmsi_record_get_int(rec, 2);
    ok(i == 2, "Expected 2, got %d\n", i);
    i = libmsi_record_get_int(rec, 4);
    ok(i == 4, "Expected 4, got %d\n", i);
    i = libmsi_record_get_int(rec, 5);
    ok(i == 5, "Expected 5, got %d\n", i);
    i = libmsi_record_get_int(rec, 6);
    ok(i == 6, "Expected 6, got %d\n", i);
    i = libmsi_record_get_int(rec, 7);
    ok(i == 7, "Expected 7, got %d\n", i);
    i = libmsi_record_get_int(rec, 8);
    ok(i == 8, "Expected 8, got %d\n", i);

    g_object_unref(rec);
    libmsi_query_close(query, NULL);
    g_object_unref(query);

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "libmsi_database_commit failed\n");

    g_object_unref(hdb);

    r = unlink(msifile);
    ok(r == 0, "file didn't exist after commit\n");
}

static void test_update(void)
{
    GError *error = NULL;
    LibmsiDatabase *hdb = 0;
    LibmsiQuery *query = 0;
    LibmsiRecord *rec = 0;
    char result[256];
    const char *sql;
    unsigned size;
    unsigned r = 0;

    /* just libmsi_database_open should not create a file */
    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "libmsi_database_open failed\n");

    /* create the Control table */
    sql = "CREATE TABLE `Control` ( "
        "`Dialog_` CHAR(72) NOT NULL, `Control` CHAR(50) NOT NULL, `Type` SHORT NOT NULL, "
        "`X` SHORT NOT NULL, `Y` SHORT NOT NULL, `Width` SHORT NOT NULL, `Height` SHORT NOT NULL,"
        "`Attributes` LONG, `Property` CHAR(50), `Text` CHAR(0) LOCALIZABLE, "
        "`Control_Next` CHAR(50), `Help` CHAR(50) LOCALIZABLE PRIMARY KEY `Dialog_`, `Control`)";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");
    r = libmsi_query_close(query, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(query);

    /* add a control */
    query = NULL;
    sql = "INSERT INTO `Control` ( "
        "`Dialog_`, `Control`, `Type`, `X`, `Y`, `Width`, `Height`, "
        "`Property`, `Text`, `Control_Next`, `Help` )"
        "VALUES('ErrorDialog', 'ErrorText', '1', '5', '5', '5', '5', '', '', '', '')";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    r = libmsi_query_close(query, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(query);

    /* add a second control */
    query = NULL;
    sql = "INSERT INTO `Control` ( "
        "`Dialog_`, `Control`, `Type`, `X`, `Y`, `Width`, `Height`, "
        "`Property`, `Text`, `Control_Next`, `Help` )"
        "VALUES('ErrorDialog', 'Button', '1', '5', '5', '5', '5', '', '', '', '')";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    r = libmsi_query_close(query, NULL);
    ok(r , "libmsi_query_close failed\n");
    g_object_unref(query);

    /* add a third control */
    query = NULL;
    sql = "INSERT INTO `Control` ( "
        "`Dialog_`, `Control`, `Type`, `X`, `Y`, `Width`, `Height`, "
        "`Property`, `Text`, `Control_Next`, `Help` )"
        "VALUES('AnotherDialog', 'ErrorText', '1', '5', '5', '5', '5', '', '', '', '')";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    r = libmsi_query_close(query, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(query);

    /* bad table */
    query = NULL;
    sql = "UPDATE `NotATable` SET `Text` = 'this is text' WHERE `Dialog_` = 'ErrorDialog'";
    query = libmsi_query_new(hdb, sql, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_BAD_QUERY_SYNTAX);
    g_clear_error(&error);

    /* bad set column */
    query = NULL;
    sql = "UPDATE `Control` SET `NotAColumn` = 'this is text' WHERE `Dialog_` = 'ErrorDialog'";
    query = libmsi_query_new(hdb, sql, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_BAD_QUERY_SYNTAX);
    g_clear_error(&error);

    /* bad where condition */
    query = NULL;
    sql = "UPDATE `Control` SET `Text` = 'this is text' WHERE `NotAColumn` = 'ErrorDialog'";
    query = libmsi_query_new(hdb, sql, &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_BAD_QUERY_SYNTAX);
    g_clear_error(&error);

    /* just the dialog_ specified */
    query = NULL;
    sql = "UPDATE `Control` SET `Text` = 'this is text' WHERE `Dialog_` = 'ErrorDialog'";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    r = libmsi_query_close(query, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(query);

    /* check the modified text */
    query = NULL;
    sql = "SELECT `Text` FROM `Control` WHERE `Control` = 'ErrorText'";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "query fetch failed\n");

    check_record_string(rec, 1, "this is text");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "query fetch failed\n");

    check_record_string(rec, 1, "");
    g_object_unref(rec);

    query_check_no_more(query);

    r = libmsi_query_close(query, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(query);

    /* dialog_ and control specified */
    query = NULL;
    sql = "UPDATE `Control` SET `Text` = 'this is text' WHERE `Dialog_` = 'ErrorDialog' AND `Control` = 'ErrorText'";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    r = libmsi_query_close(query, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(query);

    /* check the modified text */
    query = NULL;
    sql = "SELECT `Text` FROM `Control` WHERE `Control` = 'ErrorText'";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "query fetch failed\n");

    check_record_string(rec, 1, "this is text");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "query fetch failed\n");

    check_record_string(rec, 1, "");
    g_object_unref(rec);

    query_check_no_more(query);

    r = libmsi_query_close(query, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(query);

    /* no where condition */
    query = NULL;
    sql = "UPDATE `Control` SET `Text` = 'this is text'";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    r = libmsi_query_close(query, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(query);

    /* check the modified text */
    query = NULL;
    sql = "SELECT `Text` FROM `Control`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "query fetch failed\n");

    check_record_string(rec, 1, "this is text");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "query fetch failed\n");

    check_record_string(rec, 1, "this is text");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "query fetch failed\n");

    check_record_string(rec, 1, "this is text");
    g_object_unref(rec);

    query_check_no_more(query);

    r = libmsi_query_close(query, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(query);

    sql = "CREATE TABLE `Apple` ( `Banana` CHAR(72) NOT NULL, "
        "`Orange` CHAR(72),  `Pear` INT PRIMARY KEY `Banana`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Apple` ( `Banana`, `Orange`, `Pear` )"
        "VALUES('one', 'two', 3)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Apple` ( `Banana`, `Orange`, `Pear` )"
        "VALUES('three', 'four', 5)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Apple` ( `Banana`, `Orange`, `Pear` )"
        "VALUES('six', 'two', 7)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    rec = libmsi_record_new(2);
    libmsi_record_set_int(rec, 1, 8);
    libmsi_record_set_string(rec, 2, "two");

    sql = "UPDATE `Apple` SET `Pear` = ? WHERE `Orange` = ?";
    r = run_query(hdb, rec, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    g_object_unref(rec);

    query = NULL;
    sql = "SELECT `Pear` FROM `Apple` ORDER BY `Orange`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "query fetch failed\n");

    r = libmsi_record_get_int(rec, 1);
    ok(r == 8, "Expected 8, got %d\n", r);

    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "query fetch failed\n");

    r = libmsi_record_get_int(rec, 1);
    ok(r == 8, "Expected 8, got %d\n", r);

    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "query fetch failed\n");

    r = libmsi_record_get_int(rec, 1);
    ok(r == 5, "Expected 5, got %d\n", r);

    g_object_unref(rec);

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref(query);

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "libmsi_database_commit failed\n");
    g_object_unref(hdb);

    unlink(msifile);
}

static void test_special_tables(void)
{
    const char *sql;
    LibmsiDatabase *hdb = 0;
    unsigned r = 0;

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "libmsi_database_open failed\n");

    sql = "CREATE TABLE `_Properties` ( "
        "`foo` INT NOT NULL, `bar` INT LOCALIZABLE PRIMARY KEY `foo`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to create table\n");

    sql = "CREATE TABLE `_Storages` ( "
        "`foo` INT NOT NULL, `bar` INT LOCALIZABLE PRIMARY KEY `foo`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "created _Streams table\n");

    sql = "CREATE TABLE `_Streams` ( "
        "`foo` INT NOT NULL, `bar` INT LOCALIZABLE PRIMARY KEY `foo`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "created _Streams table\n");

    sql = "CREATE TABLE `_Tables` ( "
        "`foo` INT NOT NULL, `bar` INT LOCALIZABLE PRIMARY KEY `foo`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "created _Tables table\n");

    sql = "CREATE TABLE `_Columns` ( "
        "`foo` INT NOT NULL, `bar` INT LOCALIZABLE PRIMARY KEY `foo`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "created _Columns table\n");

    g_object_unref(hdb);
}

static void test_tables_order(void)
{
    const char *sql;
    LibmsiDatabase *hdb = 0;
    LibmsiQuery *hquery = 0;
    LibmsiRecord *hrec = 0;
    gchar *str;
    unsigned r = 0;
    char buffer[100];
    unsigned sz;

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "libmsi_database_open failed\n");

    sql = "CREATE TABLE `foo` ( "
        "`baz` INT NOT NULL PRIMARY KEY `baz`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to create table\n");

    sql = "CREATE TABLE `bar` ( "
        "`foo` INT NOT NULL PRIMARY KEY `foo`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to create table\n");

    sql = "CREATE TABLE `baz` ( "
        "`bar` INT NOT NULL, "
        "`baz` INT NOT NULL, "
        "`foo` INT NOT NULL PRIMARY KEY `bar`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to create table\n");

    /* The names of the tables in the _Tables table must
       be in the same order as these names are created in
       the strings table. */
    sql = "SELECT * FROM `_Tables`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "foo");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "baz");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    sz = sizeof(buffer);
    check_record_string(hrec, 1, "bar");
    g_object_unref(hrec);

    r = libmsi_query_close(hquery, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(hquery);

    /* The names of the tables in the _Columns table must
       be in the same order as these names are created in
       the strings table. */
    sql = "SELECT * FROM `_Columns`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "foo");
    check_record_string(hrec, 3, "baz");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "baz");
    check_record_string(hrec, 3, "bar");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "baz");
    check_record_string(hrec, 3, "baz");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "baz");
    check_record_string(hrec, 3, "foo");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "bar");
    check_record_string(hrec, 3, "foo");
    g_object_unref(hrec);

    r = libmsi_query_close(hquery, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(hquery);

    g_object_unref(hdb);

    unlink(msifile);
}

static void test_rows_order(void)
{
    const char *sql;
    LibmsiDatabase *hdb = 0;
    LibmsiQuery *hquery = 0;
    LibmsiRecord *hrec = 0;
    unsigned r = 0;
    char buffer[100];
    unsigned sz;

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "libmsi_database_open failed\n");

    sql = "CREATE TABLE `foo` ( "
        "`bar` LONGCHAR NOT NULL PRIMARY KEY `bar`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to create table\n");

    r = run_query(hdb, 0, "INSERT INTO `foo` "
            "( `bar` ) VALUES ( 'A' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table\n");

    r = run_query(hdb, 0, "INSERT INTO `foo` "
            "( `bar` ) VALUES ( 'B' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table\n");

    r = run_query(hdb, 0, "INSERT INTO `foo` "
            "( `bar` ) VALUES ( 'C' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table\n");

    r = run_query(hdb, 0, "INSERT INTO `foo` "
            "( `bar` ) VALUES ( 'D' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table\n");

    r = run_query(hdb, 0, "INSERT INTO `foo` "
            "( `bar` ) VALUES ( 'E' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table\n");

    r = run_query(hdb, 0, "INSERT INTO `foo` "
            "( `bar` ) VALUES ( 'F' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table\n");

    sql = "CREATE TABLE `bar` ( "
        "`foo` LONGCHAR NOT NULL, "
        "`baz` LONGCHAR NOT NULL "
        "PRIMARY KEY `foo` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to create table\n");

    r = run_query(hdb, 0, "INSERT INTO `bar` "
            "( `foo`, `baz` ) VALUES ( 'C', 'E' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table\n");

    r = run_query(hdb, 0, "INSERT INTO `bar` "
            "( `foo`, `baz` ) VALUES ( 'F', 'A' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table\n");

    r = run_query(hdb, 0, "INSERT INTO `bar` "
            "( `foo`, `baz` ) VALUES ( 'A', 'B' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table\n");

    r = run_query(hdb, 0, "INSERT INTO `bar` "
            "( `foo`, `baz` ) VALUES ( 'D', 'E' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table\n");

    /* The rows of the table must be ordered by the column values of
       each row. For strings, the column value is the string id
       in the string table.  */

    sql = "SELECT * FROM `bar`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "A");
    check_record_string(hrec, 2, "B");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "C");
    check_record_string(hrec, 2, "E");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "D");
    check_record_string(hrec, 2, "E");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "F");
    check_record_string(hrec, 2, "A");
    g_object_unref(hrec);

    r = libmsi_query_close(hquery, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(hquery);

    g_object_unref(hdb);

    unlink(msifile);
}

static void test_collation(void)
{
    static const char sql1[] =
	    "INSERT INTO `bar` (`foo`, `baz`) VALUES ('a\xcc\x8a', 'C')";
    static const char sql2[] =
	    "INSERT INTO `bar` (`foo`, `baz`) VALUES ('\xc3\xa5', 'D')";
    static const char sql3[] =
	    "CREATE TABLE `baz` (`a\xcc\x8a` LONGCHAR NOT NULL, `\xc3\xa5` LONGCHAR NOT NULL PRIMARY KEY `a\xcc\x8a`)";
    static const char sql4[] =
	    "CREATE TABLE `a\xcc\x8a` (`foo` LONGCHAR NOT NULL, `\xc3\xa5` LONGCHAR NOT NULL PRIMARY KEY `foo`)";
    static const char sql5[] =
	    "CREATE TABLE `\xc3\xa5` (`foo` LONGCHAR NOT NULL, `\xc3\xa5` LONGCHAR NOT NULL PRIMARY KEY `foo`)";
    static const char sql6[] =
	    "SELECT * FROM `bar` WHERE `foo` = '\xc3\xa5'";
    static const char letter_a_ring[] = "a\xcc\x8a";
    static const char letter_a_with_ring[] = "\xc3\xa5";
    const char *sql;
    LibmsiDatabase *hdb = 0;
    LibmsiQuery *hquery = 0;
    LibmsiRecord *hrec = 0;
    unsigned r = 0;
    char buffer[100];
    unsigned sz;
    gchar *str;

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "libmsi_database_open failed\n");

    sql = "CREATE TABLE `bar` ( "
        "`foo` LONGCHAR NOT NULL, "
        "`baz` LONGCHAR NOT NULL "
        "PRIMARY KEY `foo` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to create table\n");

    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "wrong error %u\n", r);

    r = run_query(hdb, 0, "INSERT INTO `bar` "
            "( `foo`, `baz` ) VALUES ( '\2', 'A' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table %u\n", r);

    r = run_query(hdb, 0, "INSERT INTO `bar` "
            "( `foo`, `baz` ) VALUES ( '\1', 'B' )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table %u\n", r);

    r = run_query(hdb, 0, sql1);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table %u\n", r);

    r = run_query(hdb, 0, sql2);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add value to table %u\n", r);

    r = run_query(hdb, 0, sql3);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot create table %u\n", r);

    r = run_query(hdb, 0, sql4);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot create table %u\n", r);

    r = run_query(hdb, 0, sql5);
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot create table %u\n", r);

    sql = "SELECT * FROM `bar`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "\2");
    check_record_string(hrec, 2, "A");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    check_record_string(hrec, 1, "\1");
    check_record_string(hrec, 2, "B");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    str = libmsi_record_get_string(hrec, 1);
    ok(str, "Expected string\n");
    ok(!memcmp(str, letter_a_ring, sizeof(letter_a_ring)),
       "Expected %s, got %s\n", letter_a_ring, str);
    check_record_string(hrec, 2, "C");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    str = libmsi_record_get_string(hrec, 1);
    ok(str, "Expected string\n");
    ok(!memcmp(str, letter_a_with_ring, sizeof(letter_a_with_ring)),
       "Expected %s, got %s\n", letter_a_with_ring, str);
    g_free(str);
    check_record_string(hrec, 2, "D");
    g_object_unref(hrec);

    r = libmsi_query_close(hquery, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(hquery);

    hquery = libmsi_query_new(hdb, sql6, NULL);
    ok(hquery, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "libmsi_query_execute failed\n");

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");
    sz = sizeof(buffer);
    str = libmsi_record_get_string(hrec, 1);
    ok(str, "Expected string\n");
    ok(!memcmp(str, letter_a_with_ring, sizeof(letter_a_with_ring)),
       "Expected %s, got %s\n", letter_a_with_ring, str);
    check_record_string(hrec, 2, "D");
    g_object_unref(hrec);

    query_check_no_more(hquery);

    r = libmsi_query_close(hquery, NULL);
    ok(r, "libmsi_query_close failed\n");
    g_object_unref(hquery);

    g_object_unref(hdb);

    unlink(msifile);
}

static void test_select_markers(void)
{
    LibmsiDatabase *hdb = 0;
    LibmsiRecord *rec;
    LibmsiQuery *query;
    LibmsiRecord *res;
    const char *sql;
    unsigned r = 0;
    unsigned size;
    char buf[256];

    hdb = create_db();
    ok( hdb, "failed to create db\n");

    r = run_query(hdb, 0,
            "CREATE TABLE `Table` (`One` CHAR(72), `Two` CHAR(72), `Three` SHORT PRIMARY KEY `One`, `Two`, `Three`)");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot create table: %d\n", r);

    r = run_query(hdb, 0, "INSERT INTO `Table` "
            "( `One`, `Two`, `Three` ) VALUES ( 'apple', 'one', 1 )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add file to the Media table: %d\n", r);

    r = run_query(hdb, 0, "INSERT INTO `Table` "
            "( `One`, `Two`, `Three` ) VALUES ( 'apple', 'two', 1 )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add file to the Media table: %d\n", r);

    r = run_query(hdb, 0, "INSERT INTO `Table` "
            "( `One`, `Two`, `Three` ) VALUES ( 'apple', 'two', 2 )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add file to the Media table: %d\n", r);

    r = run_query(hdb, 0, "INSERT INTO `Table` "
            "( `One`, `Two`, `Three` ) VALUES ( 'banana', 'three', 3 )");
    ok(r == LIBMSI_RESULT_SUCCESS, "cannot add file to the Media table: %d\n", r);

    rec = libmsi_record_new(2);
    libmsi_record_set_string(rec, 1, "apple");
    libmsi_record_set_string(rec, 2, "two");

    query = NULL;
    sql = "SELECT * FROM `Table` WHERE `One`=? AND `Two`=? ORDER BY `Three`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");

    r = libmsi_query_execute(query, rec, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    res = libmsi_query_fetch(query, NULL);
    ok(res, "query fetch failed\n");

    check_record_string(res, 1, "apple");
    check_record_string(res, 2, "two");

    r = libmsi_record_get_int(res, 3);
    ok(r == 1, "Expected 1, got %d\n", r);

    g_object_unref(res);

    res = libmsi_query_fetch(query, NULL);
    ok(res, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    check_record_string(res, 1, "apple");

    check_record_string(res, 2, "two");

    r = libmsi_record_get_int(res, 3);
    ok(r == 2, "Expected 2, got %d\n", r);

    g_object_unref(res);

    query_check_no_more(query);

    g_object_unref(rec);
    libmsi_query_close(query, NULL);
    g_object_unref(query);

    rec = libmsi_record_new(2);
    libmsi_record_set_string(rec, 1, "one");
    libmsi_record_set_int(rec, 2, 1);

    query = NULL;
    sql = "SELECT * FROM `Table` WHERE `Two`<>? AND `Three`>? ORDER BY `Three`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, rec, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    res = libmsi_query_fetch(query, NULL);
    ok(res, "query fetch failed\n");

    check_record_string(res, 1, "apple");
    check_record_string(res, 2, "two");

    r = libmsi_record_get_int(res, 3);
    ok(r == 2, "Expected 2, got %d\n", r);

    g_object_unref(res);

    res = libmsi_query_fetch(query, NULL);
    ok(res, "query fetch failed\n");

    check_record_string(res, 1, "banana");
    check_record_string(res, 2, "three");

    r = libmsi_record_get_int(res, 3);
    ok(r == 3, "Expected 3, got %d\n", r);

    g_object_unref(res);

    query_check_no_more(query);

    g_object_unref(rec);
    libmsi_query_close(query, NULL);
    g_object_unref(query);
    g_object_unref(hdb);
    unlink(msifile);
}

static const WCHAR data10[] = { /* MOO */
    0x8001, 0x000b,
};
static const WCHAR data11[] = { /* AAR */
    0x8002, 0x8005,
    0x000c, 0x000f,
};
static const char data12[] = /* _StringData */
    "MOOABAARCDonetwofourfive";
static const WCHAR data13[] = { /* _StringPool */
/*  len, refs */
    0,   0,    /* string 0 ''     */
    0,   0,    /* string 1 ''     */
    0,   0,    /* string 2 ''     */
    0,   0,    /* string 3 ''     */
    0,   0,    /* string 4 ''     */
    3,   3,    /* string 5 'MOO'  */
    1,   1,    /* string 6 'A'    */
    1,   1,    /* string 7 'B'    */
    3,   3,    /* string 8 'AAR'  */
    1,   1,    /* string 9 'C'    */
    1,   1,    /* string a 'D'    */
    3,   1,    /* string b 'one'  */
    3,   1,    /* string c 'two'  */
    0,   0,    /* string d ''     */
    4,   1,    /* string e 'four' */
    4,   1,    /* string f 'five' */
};

static void test_stringtable(void)
{
#ifdef _WIN32
    LibmsiDatabase *hdb = 0;
    LibmsiQuery *hquery = 0;
    LibmsiRecord *hrec = 0;
    IStorage *stg = NULL;
    IStream *stm;
    WCHAR name[0x20];
    HRESULT hr;
    const char *sql;
    char buffer[MAX_PATH];
    WCHAR data[MAX_PATH];
    unsigned sz, read;
    unsigned r = 0;

    static const unsigned mode = STGM_DIRECT | STGM_READ | STGM_SHARE_DENY_WRITE;
    static const WCHAR stringdata[] = {0x4840, 0x3f3f, 0x4577, 0x446c, 0x3b6a, 0x45e4, 0x4824, 0}; /* _StringData */
    static const WCHAR stringpool[] = {0x4840, 0x3f3f, 0x4577, 0x446c, 0x3e6a, 0x44b2, 0x482f, 0}; /* _StringPool */
    static const WCHAR moo[] = {0x4840, 0x3e16, 0x4818, 0}; /* MOO */
    static const WCHAR aar[] = {0x4840, 0x3a8a, 0x481b, 0}; /* AAR */

    unlink(msifile);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `MOO` (`A` INT, `B` CHAR(72) PRIMARY KEY `A`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `AAR` (`C` INT, `D` CHAR(72) PRIMARY KEY `C`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* insert persistent row */
    sql = "INSERT INTO `MOO` (`A`, `B`) VALUES (1, 'one')";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* insert persistent row */
    sql = "INSERT INTO `AAR` (`C`, `D`) VALUES (2, 'two')";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* insert persistent row */
    sql = "INSERT INTO `MOO` (`A`, `B`) VALUES (4, 'four')";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* delete persistent row */
    sql = "DELETE FROM `MOO` WHERE `A` = 4";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* insert persistent row */
    sql = "INSERT INTO `AAR` (`C`, `D`) VALUES (5, 'five')";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS\n");

    g_object_unref(hdb);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_READONLY, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `MOO`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    r = libmsi_record_get_field_count(hrec);
    ok(r == 2, "Expected 2, got %d\n", r);

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(hrec, 2, "one");

    g_object_unref(hrec);

    query_check_no_more(hquery);

    r = libmsi_query_close(hquery, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    g_object_unref(hquery);
    g_object_unref(hrec);

    sql = "SELECT * FROM `AAR`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    r = libmsi_record_get_field_count(hrec);
    ok(r == 2, "Expected 2, got %d\n", r);

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 2, "Expected 2, got %d\n", r);

    check_record_string(hrec, 2, "two");

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    r = libmsi_record_get_field_count(hrec);
    ok(r == 2, "Expected 2, got %d\n", r);

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 5, "Expected 5, got %d\n", r);

    check_record_string(hrec, 2, "five");

    g_object_unref(hrec);

    query_check_no_more(hquery);

    r = libmsi_query_close(hquery, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    g_object_unref(hquery);
    g_object_unref(hrec);
    g_object_unref(hdb);

    MultiByteToWideChar(CP_ACP, 0, msifile, -1, name, 0x20);
    hr = StgOpenStorage(name, NULL, mode, NULL, 0, &stg);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);
    ok(stg != NULL, "Expected non-NULL storage\n");

    hr = IStorage_OpenStream(stg, moo, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);
    ok(stm != NULL, "Expected non-NULL stream\n");

    hr = IStream_Read(stm, data, MAX_PATH, &read);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);
    ok(read == 4, "Expected 4, got %d\n", read);
    todo_wine ok(!memcmp(data, data10, read), "Unexpected data\n");

    hr = IStream_Release(stm);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);

    hr = IStorage_OpenStream(stg, aar, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);
    ok(stm != NULL, "Expected non-NULL stream\n");

    hr = IStream_Read(stm, data, MAX_PATH, &read);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);
    ok(read == 8, "Expected 8, got %d\n", read);
    todo_wine
    {
        ok(!memcmp(data, data11, read), "Unexpected data\n");
    }

    hr = IStream_Release(stm);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);

    hr = IStorage_OpenStream(stg, stringdata, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);
    ok(stm != NULL, "Expected non-NULL stream\n");

    hr = IStream_Read(stm, buffer, MAX_PATH, &read);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);
    ok(read == 24, "Expected 24, got %d\n", read);
    ok(!memcmp(buffer, data12, read), "Unexpected data\n");

    hr = IStream_Release(stm);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);

    hr = IStorage_OpenStream(stg, stringpool, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);
    ok(stm != NULL, "Expected non-NULL stream\n");

    hr = IStream_Read(stm, data, MAX_PATH, &read);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);
    todo_wine
    {
        ok(read == 64, "Expected 64, got %d\n", read);
        ok(!memcmp(data, data13, read), "Unexpected data\n");
    }

    hr = IStream_Release(stm);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);

    hr = IStorage_Release(stg);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);

    unlink(msifile);
#endif
}

static const WCHAR _Tables[] = {0x4840, 0x3f7f, 0x4164, 0x422f, 0x4836, 0};
static const WCHAR _StringData[] = {0x4840, 0x3f3f, 0x4577, 0x446c, 0x3b6a, 0x45e4, 0x4824, 0};
static const WCHAR _StringPool[] = {0x4840, 0x3f3f, 0x4577, 0x446c, 0x3e6a, 0x44b2, 0x482f, 0};

static const WCHAR data14[] = { /* _StringPool */
/*  len, refs */
    0,   0,    /* string 0 ''    */
};

static const struct {
    const WCHAR *name;
    const void *data;
    unsigned size;
} database_table_data[] =
{
    {_Tables, NULL, 0},
    {_StringData, NULL, 0},
    {_StringPool, data14, sizeof data14},
};

#ifdef _WIN32
static void enum_stream_names(IStorage *stg)
{
    IEnumSTATSTG *stgenum = NULL;
    IStream *stm;
    HRESULT hr;
    STATSTG stat;
    unsigned n, count;
    uint8_t data[MAX_PATH];
    uint8_t check[MAX_PATH];
    unsigned sz;

    memset(check, 'a', MAX_PATH);

    hr = IStorage_EnumElements(stg, 0, NULL, 0, &stgenum);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    n = 0;
    while(true)
    {
        count = 0;
        hr = IEnumSTATSTG_Next(stgenum, 1, &stat, &count);
        if(FAILED(hr) || !count)
            break;

        ok(!lstrcmpW(stat.pwcsName, database_table_data[n].name),
           "Expected table %d name to match\n", n);

        stm = NULL;
        hr = IStorage_OpenStream(stg, stat.pwcsName, NULL,
                                 STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm);
        ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
        ok(stm != NULL, "Expected non-NULL stream\n");

        CoTaskMemFree(stat.pwcsName);

        sz = MAX_PATH;
        memset(data, 'a', MAX_PATH);
        hr = IStream_Read(stm, data, sz, &count);
        ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

        ok(count == database_table_data[n].size,
           "Expected %d, got %d\n", database_table_data[n].size, count);

        if (!database_table_data[n].size)
            ok(!memcmp(data, check, MAX_PATH), "data should not be changed\n");
        else
            ok(!memcmp(data, database_table_data[n].data, database_table_data[n].size),
               "Expected table %d data to match\n", n);

        IStream_Release(stm);
        n++;
    }

    ok(n == 3, "Expected 3, got %d\n", n);

    IEnumSTATSTG_Release(stgenum);
}
#endif

static void test_defaultdatabase(void)
{
#ifdef _WIN32
    unsigned r = 0;
    HRESULT hr;
    LibmsiDatabase *hdb;
    IStorage *stg = NULL;

    unlink(msifile);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_database_commit(hdb);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    g_object_unref(hdb);

    hr = StgOpenStorage(msifileW, NULL, STGM_READ | STGM_SHARE_DENY_WRITE, NULL, 0, &stg);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(stg != NULL, "Expected non-NULL stg\n");

    enum_stream_names(stg);

    IStorage_Release(stg);
    unlink(msifile);
#endif
}

static void test_order(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *hquery;
    LibmsiRecord *hrec;
    char buffer[256];
    const char *sql;
    unsigned r = 0, sz;
    int val;

    hdb = create_db();
    ok(hdb, "failed to create db\n");

    sql = "CREATE TABLE `Empty` ( `A` SHORT NOT NULL PRIMARY KEY `A`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `Mesa` ( `A` SHORT NOT NULL, `B` SHORT, `C` SHORT PRIMARY KEY `A`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Mesa` ( `A`, `B`, `C` ) VALUES ( 1, 2, 9 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Mesa` ( `A`, `B`, `C` ) VALUES ( 3, 4, 7 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Mesa` ( `A`, `B`, `C` ) VALUES ( 5, 6, 8 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `Sideboard` ( `D` SHORT NOT NULL, `E` SHORT, `F` SHORT PRIMARY KEY `D`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Sideboard` ( `D`, `E`, `F` ) VALUES ( 10, 11, 18 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Sideboard` ( `D`, `E`, `F` ) VALUES ( 12, 13, 16 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Sideboard` ( `D`, `E`, `F` ) VALUES ( 14, 15, 17 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT `A`, `B` FROM `Mesa` ORDER BY `C`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 3, "Expected 3, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 4, "Expected 3, got %d\n", val);

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 5, "Expected 5, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 6, "Expected 6, got %d\n", val);

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 1, "Expected 1, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 2, "Expected 2, got %d\n", val);

    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT `A`, `D` FROM `Mesa`, `Sideboard` ORDER BY `F`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 1, "Expected 1, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 12, "Expected 12, got %d\n", val);

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 3, "Expected 3, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 12, "Expected 12, got %d\n", val);

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 5, "Expected 5, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 12, "Expected 12, got %d\n", val);

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 1, "Expected 1, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 14, "Expected 14, got %d\n", val);

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 3, "Expected 3, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 14, "Expected 14, got %d\n", val);

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 5, "Expected 5, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 14, "Expected 14, got %d\n", val);

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 1, "Expected 1, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 10, "Expected 10, got %d\n", val);

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 3, "Expected 3, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 10, "Expected 10, got %d\n", val);

    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    val = libmsi_record_get_int(hrec, 1);
    ok(val == 5, "Expected 5, got %d\n", val);

    val = libmsi_record_get_int(hrec, 2);
    ok(val == 10, "Expected 10, got %d\n", val);

    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT * FROM `Empty` ORDER BY `A`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "CREATE TABLE `Buffet` ( `One` CHAR(72), `Two` SHORT PRIMARY KEY `One`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Buffet` ( `One`, `Two` ) VALUES ( 'uno',  2)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Buffet` ( `One`, `Two` ) VALUES ( 'dos',  3)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Buffet` ( `One`, `Two` ) VALUES ( 'tres',  1)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `Buffet` WHERE `One` = 'dos' ORDER BY `Two`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    check_record_string(hrec, 1, "dos");

    r = libmsi_record_get_int(hrec, 2);
    ok(r == 3, "Expected 3, got %d\n", r);

    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);
    g_object_unref(hdb);
}

static void test_deleterow(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *hquery;
    LibmsiRecord *hrec;
    const char *sql;
    char buf[256];
    unsigned r = 0;
    unsigned size;

    unlink(msifile);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Table` (`A`) VALUES ('one')";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Table` (`A`) VALUES ('two')";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DELETE FROM `Table` WHERE `A` = 'one'";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS\n");

    g_object_unref(hdb);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_READONLY, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `Table`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "two");

    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);
    g_object_unref(hdb);
    unlink(msifile);
}

static const char import_dat[] = "A\n"
                                 "s72\n"
                                 "Table\tA\n"
                                 "This is a new 'string' ok\n";

static void test_quotes(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *hquery;
    LibmsiRecord *hrec;
    const char *sql;
    char buf[256];
    unsigned r = 0;
    unsigned size;

    unlink(msifile);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Table` ( `A` ) VALUES ( 'This is a 'string' ok' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `Table` ( `A` ) VALUES ( \"This is a 'string' ok\" )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `Table` ( `A` ) VALUES ( \"test\" )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `Table` ( `A` ) VALUES ( 'This is a ''string'' ok' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `Table` ( `A` ) VALUES ( 'This is a '''string''' ok' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `Table` ( `A` ) VALUES ( 'This is a \'string\' ok' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `Table` ( `A` ) VALUES ( 'This is a \"string\" ok' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `Table`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "This is a \"string\" ok");

    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    write_file("import.idt", import_dat, (sizeof(import_dat) - 1) * sizeof(char));

    r = libmsi_database_import(hdb, "import.idt", NULL);
    ok(r, "libmsi_database_import() failed\n");

    unlink("import.idt");

    sql = "SELECT * FROM `Table`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "This is a new 'string' ok");

    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);
    g_object_unref(hdb);
    unlink(msifile);
}

static void test_carriagereturn(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *hquery;
    LibmsiRecord *hrec;
    const char *sql;
    char buf[256];
    unsigned r = 0;
    unsigned size;

    unlink(msifile);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `Table`\r ( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` \r( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE\r TABLE `Table` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE\r `Table` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` (\r `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A`\r CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72)\r NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT\r NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT \rNULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT NULL\r PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT NULL \rPRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT NULL PRIMARY\r KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT NULL PRIMARY \rKEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT NULL PRIMARY KEY\r `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A`\r )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )\r";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `\rOne` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `Tw\ro` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `Three\r` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `Four` ( `A\r` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Four` ( `\rA` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Four` ( `A` CHAR(72\r) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Four` ( `A` CHAR(\r72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Four` ( `A` CHAR(72) NOT NULL PRIMARY KEY `\rA` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Four` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A\r` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Four` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A\r` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "SELECT * FROM `_Tables`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "\rOne");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "Tw\ro");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "Three\r");
    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    g_object_unref(hdb);
    unlink(msifile);
}

static void test_noquotes(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *hquery;
    LibmsiRecord *hrec;
    const char *sql;
    char buf[256];
    unsigned r = 0;
    unsigned size;

    unlink(msifile);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE Table ( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( A CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `Table2` ( `A` CHAR(72) NOT NULL PRIMARY KEY A )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `Table3` ( A CHAR(72) NOT NULL PRIMARY KEY A )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `_Tables`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "Table");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "Table2");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "Table3");
    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT * FROM `_Columns`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "Table");

    r = libmsi_record_get_int(hrec, 2);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(hrec, 3, "A");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "Table2");

    r = libmsi_record_get_int(hrec, 2);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(hrec, 3, "A");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "Table3");

    r = libmsi_record_get_int(hrec, 2);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(hrec, 3, "A");
    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "INSERT INTO Table ( `A` ) VALUES ( 'hi' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `Table` ( A ) VALUES ( 'hi' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `Table` ( `A` ) VALUES ( hi )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "SELECT * FROM Table WHERE `A` = 'hi'";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "SELECT * FROM `Table` WHERE `A` = hi";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "SELECT * FROM Table";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    hquery = NULL;
    sql = "SELECT * FROM Table2";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT * FROM `Table` WHERE A = 'hi'";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "hi");
    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);
    g_object_unref(hdb);
    unlink(msifile);
}

static void read_file_data(const char *filename, char *buffer)
{
    int fd;

    fd = open( filename, O_RDONLY | O_BINARY );
    memset(buffer, 0, 512);
    read(fd, buffer, 512);
    close(fd);
}

static void test_forcecodepage(void)
{
    LibmsiDatabase *hdb;
    const char *sql;
    char buffer[512];
    unsigned r = 0;
    int fd;

    unlink(msifile);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `_ForceCodepage`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `Table` ( `A` CHAR(72) NOT NULL PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `_ForceCodepage`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS\n");

    sql = "SELECT * FROM `_ForceCodepage`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    g_object_unref(hdb);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_TRANSACT, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `_ForceCodepage`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    fd = open("forcecodepage.idt", O_WRONLY | O_BINARY | O_CREAT, 0644);
    ok(fd != -1, "cannot open file\n");

    r = libmsi_database_export(hdb, "_ForceCodepage", fd, NULL);
    ok(r, "Expected success\n");
    close(fd);

    read_file_data("forcecodepage.idt", buffer);
    ok(g_str_equal(buffer, "\r\n\r\n0\t_ForceCodepage\r\n"),
       "Expected \"\r\n\r\n0\t_ForceCodepage\r\n\", got \"%s\"\n", buffer);

    create_file_data("forcecodepage.idt", "\r\n\r\n850\t_ForceCodepage\r\n", 0);

    r = libmsi_database_import(hdb, "forcecodepage.idt", NULL);
    ok(r, "libmsi_database_import() failed\n");

    fd = open("forcecodepage.idt", O_WRONLY | O_BINARY | O_CREAT, 0644);
    ok(fd != -1, "cannot open file\n");

    r = libmsi_database_export(hdb, "_ForceCodepage", fd, NULL);
    ok(r, "Expected success\n");
    close(fd);

    read_file_data("forcecodepage.idt", buffer);
    ok(g_str_equal(buffer, "\r\n\r\n850\t_ForceCodepage\r\n"),
       "Expected \"\r\n\r\n850\t_ForceCodepage\r\n\", got \"%s\"\n", buffer);

    create_file_data("forcecodepage.idt", "\r\n\r\n9999\t_ForceCodepage\r\n", 0);

    r = libmsi_database_import(hdb, "forcecodepage.idt", NULL);
    ok(!r, "Expected failure\n");

    g_object_unref(hdb);
    unlink(msifile);
    unlink("forcecodepage.idt");
}

static bool create_storage(const char *name)
{
#ifdef _WIN32
    WCHAR nameW[MAX_PATH];
    IStorage *stg;
    IStream *stm;
    HRESULT hr;
    unsigned count;
    bool res = false;

    MultiByteToWideChar(CP_ACP, 0, name, -1, nameW, MAX_PATH);
    hr = StgCreateDocfile(nameW, STGM_CREATE | STGM_READWRITE |
                          STGM_DIRECT | STGM_SHARE_EXCLUSIVE, 0, &stg);
    if (FAILED(hr))
        return false;

    hr = IStorage_CreateStream(stg, nameW, STGM_WRITE | STGM_SHARE_EXCLUSIVE,
                               0, 0, &stm);
    if (FAILED(hr))
        goto done;

    hr = IStream_Write(stm, "stgdata", 8, &count);
    if (SUCCEEDED(hr))
        res = true;

done:
    IStream_Release(stm);
    IStorage_Release(stg);

    return res;
#else
    return true;
#endif
}

static void test_storages_table(void)
{
#ifdef _WIN32
    LibmsiDatabase *hdb;
    LibmsiQuery *hquery;
    LibmsiRecord *hrec;
    IStorage *stg, *inner;
    IStream *stm;
    char file[MAX_PATH];
    char buf[MAX_PATH];
    WCHAR name[MAX_PATH];
    const char *sql;
    HRESULT hr;
    unsigned size;
    unsigned r = 0;

    hdb = create_db();
    ok(hdb, "failed to create db\n");

    r = libmsi_database_commit(hdb, NULL);
    ok(r, "Failed to commit database\n");

    g_object_unref(hdb);

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_TRANSACT, NULL, NULL);
    ok(hdb , "Failed to open database\n");

    /* check the column types */
    hrec = get_column_info(hdb, "SELECT * FROM `_Storages`", LIBMSI_COL_INFO_TYPES);
    ok(hrec, "failed to get column info hrecord\n");
    ok(check_record(hrec, 1, "s62"), "wrong hrecord type\n");
    ok(check_record(hrec, 2, "V0"), "wrong hrecord type\n");

    g_object_unref(hrec);

    /* now try the names */
    hrec = get_column_info(hdb, "SELECT * FROM `_Storages`", LIBMSI_COL_INFO_NAMES);
    ok(hrec, "failed to get column info hrecord\n");
    ok(check_record(hrec, 1, "Name"), "wrong hrecord type\n");
    ok(check_record(hrec, 2, "Data"), "wrong hrecord type\n");

    g_object_unref(hrec);

    create_storage("storage.bin");

    hrec = libmsi_record_new(2);
    libmsi_record_set_string(hrec, 1, "stgname");

    r = libmsi_record_load_stream(hrec, 2, "storage.bin");
    ok(r, "Failed to add stream data to the hrecord: %d\n", r);

    unlink("storage.bin");

    sql = "INSERT INTO `_Storages` (`Name`, `Data`) VALUES (?, ?)";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Failed to open database hquery\n");

    r = libmsi_query_execute(hquery, hrec, NULL);
    ok(r, "Failed to execute hquery: %d\n", r);

    g_object_unref(hrec);
    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT `Name`, `Data` FROM `_Storages`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Failed to open database hquery\n");

    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Failed to execute hquery: %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "stgname");

    in = libmsi_record_get_stream(hrec, 2);
    ok(!in, "Expected ERROR_INVALID_DATA\n");

    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    libmsi_database_commit(hdb, NULL);
    g_object_unref(hdb);

    MultiByteToWideChar(CP_ACP, 0, msifile, -1, name, MAX_PATH);
    hr = StgOpenStorage(name, NULL, STGM_DIRECT | STGM_READ |
                        STGM_SHARE_DENY_WRITE, NULL, 0, &stg);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(stg != NULL, "Expected non-NULL storage\n");

    MultiByteToWideChar(CP_ACP, 0, "stgname", -1, name, MAX_PATH);
    hr = IStorage_OpenStorage(stg, name, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE,
                              NULL, 0, &inner);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(inner != NULL, "Expected non-NULL storage\n");

    MultiByteToWideChar(CP_ACP, 0, "storage.bin", -1, name, MAX_PATH);
    hr = IStorage_OpenStream(inner, name, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(stm != NULL, "Expected non-NULL stream\n");

    hr = IStream_Read(stm, buf, MAX_PATH, &size);
    ok(hr == S_OK, "Expected S_OK, got %d\n", hr);
    ok(size == 8, "Expected 8, got %d\n", size);
    ok(g_str_equal(buf, "stgdata"), "Expected \"stgdata\", got \"%s\"\n", buf);

    IStream_Release(stm);
    IStorage_Release(inner);

    IStorage_Release(stg);
    unlink(msifile);
#endif
}

static void test_droptable(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *hquery;
    LibmsiRecord *hrec;
    char buf[200];
    const char *sql;
    unsigned size;
    unsigned r = 0;
    GError *error = NULL;

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `One`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_NO_MORE_ITEMS, got %d\n", r);
    g_assert(hrec == NULL);

    sql = "SELECT * FROM `_Tables` WHERE `Name` = 'One'";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "One");

    g_object_unref(hrec);
    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT * FROM `_Columns` WHERE `Table` = 'One'";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "One");

    r = libmsi_record_get_int(hrec, 2);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(hrec, 3, "A");
    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "DROP `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE `One`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, &error);
    ok(error, "Expected error\n");
    g_clear_error(&error);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT * FROM `IDontExist`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "SELECT * FROM `One`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE One";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `One`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "SELECT * FROM `_Tables` WHERE `Name` = 'One'";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_NO_MORE_ITEMS, got %d\n", r);
    g_assert(hrec == NULL);

    sql = "SELECT * FROM `_Columns` WHERE `Table` = 'One'";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_NO_MORE_ITEMS, got %d\n", r);
    g_assert(hrec == NULL);

    sql = "CREATE TABLE `One` ( `B` INT, `C` INT PRIMARY KEY `B` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `One`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_NO_MORE_ITEMS, got %d\n", r);
    g_assert(hrec == NULL);

    sql = "SELECT * FROM `_Tables` WHERE `Name` = 'One'";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "One");

    g_object_unref(hrec);
    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "SELECT * FROM `_Columns` WHERE `Table` = 'One'";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "One");

    r = libmsi_record_get_int(hrec, 2);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(hrec, 3, "B");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    check_record_string(hrec, 1, "One");

    r = libmsi_record_get_int(hrec, 2);
    ok(r == 2, "Expected 2, got %d\n", r);

    check_record_string(hrec, 3, "C");
    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "DROP TABLE One";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `One`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "SELECT * FROM `_Tables` WHERE `Name` = 'One'";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_NO_MORE_ITEMS, got %d\n", r);
    g_assert(hrec == NULL);

    sql = "SELECT * FROM `_Columns` WHERE `Table` = 'One'";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_NO_MORE_ITEMS, got %d\n", r);
    g_assert(hrec == NULL);

    g_object_unref(hdb);
    unlink(msifile);
}

static void test_dbmerge(void)
{
    GError *error = NULL;
    GInputStream *in;
    LibmsiDatabase *hdb;
    LibmsiDatabase *href;
    LibmsiQuery *hquery;
    LibmsiRecord *hrec;
    char buf[100];
    const char *sql;
    unsigned size;
    unsigned r = 0;

    hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    href = libmsi_database_new("refdb.msi", LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(href, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* szTableName is NULL */
    r = libmsi_database_merge(hdb, href, NULL, &error);
    ok(r, "libmsi_database_merge() failed");

    /* both DBs empty, szTableName is valid */
    r = libmsi_database_merge(hdb, href, "MergeErrors", &error);
    ok(r, "libmsi_database_merge() failed");

    sql = "CREATE TABLE `One` ( `A` INT PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` CHAR(72) PRIMARY KEY `A` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* column types don't match */
    r = libmsi_database_merge(hdb, href, "MergeErrors", &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(&error);

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( "
        "`A` CHAR(72), "
        "`B` CHAR(56), "
        "`C` CHAR(64) LOCALIZABLE, "
        "`D` LONGCHAR, "
        "`E` CHAR(72) NOT NULL, "
        "`F` CHAR(56) NOT NULL, "
        "`G` CHAR(64) NOT NULL LOCALIZABLE, "
        "`H` LONGCHAR NOT NULL "
        "PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( "
        "`A` CHAR(64), "
        "`B` CHAR(64), "
        "`C` CHAR(64), "
        "`D` CHAR(64), "
        "`E` CHAR(64) NOT NULL, "
        "`F` CHAR(64) NOT NULL, "
        "`G` CHAR(64) NOT NULL, "
        "`H` CHAR(64) NOT NULL "
        "PRIMARY KEY `A` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* column sting types don't match exactly */
    r = libmsi_database_merge(hdb, href, "MergeErrors", &error);
    ok(r, "libmsi_database_merge() failed");

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` INT PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `C` INT PRIMARY KEY `A` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* column names don't match */
    r = libmsi_database_merge(hdb, href, "MergeErrors", &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(&error);

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` INT PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` INT PRIMARY KEY `B` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* primary keys don't match */
    r = libmsi_database_merge(hdb, href, "MergeErrors", &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(&error);

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` INT PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` INT PRIMARY KEY `A`, `B` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* number of primary keys doesn't match */
    r = libmsi_database_merge(hdb, href, "MergeErrors", &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH);
    g_clear_error(&error);

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` INT, `C` INT PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` INT PRIMARY KEY `A` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `One` ( `A`, `B` ) VALUES ( 1, 2 )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* number of columns doesn't match */
    r = libmsi_database_merge(hdb, href, "MergeErrors", NULL);
    ok(r, "libmsi_database_merge() failed");

    sql = "SELECT * FROM `One`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 1, "Expected 1, got %d\n", r);

    r = libmsi_record_get_int(hrec, 2);
    ok(r == 2, "Expected 2, got %d\n", r);

    r = libmsi_record_get_int(hrec, 3);
    ok(r == LIBMSI_NULL_INT, "Expected LIBMSI_NULL_INT, got %d\n", r);

    g_object_unref(hrec);

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` INT PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` INT, `C` INT PRIMARY KEY `A` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `One` ( `A`, `B`, `C` ) VALUES ( 1, 2, 3 )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* number of columns doesn't match */
    r = libmsi_database_merge(hdb, href, "MergeErrors", NULL);
    ok(r, "libmsi_database_merge() failed");

    sql = "SELECT * FROM `One`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 1, "Expected 1, got %d\n", r);

    r = libmsi_record_get_int(hrec, 2);
    ok(r == 2, "Expected 2, got %d\n", r);

    r = libmsi_record_get_int(hrec, 3);
    ok(r == LIBMSI_NULL_INT, "Expected LIBMSI_NULL_INT, got %d\n", r);

    g_object_unref(hrec);

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` INT PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `One` ( `A`, `B` ) VALUES ( 1, 1 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `One` ( `A`, `B` ) VALUES ( 2, 2 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` INT PRIMARY KEY `A` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `One` ( `A`, `B` ) VALUES ( 1, 2 )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `One` ( `A`, `B` ) VALUES ( 2, 3 )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* primary keys match, rows do not */
    r = libmsi_database_merge(hdb, href, "MergeErrors", &error);
    g_assert_error(error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_FUNCTION_FAILED);
    g_clear_error(&error);

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    check_record_string(hrec, 1, "One");

    r = libmsi_record_get_int(hrec, 2);
    ok(r == 2, "Expected 2, got %d\n", r);

    g_object_unref(hrec);

    hquery = libmsi_query_new(hdb, "SELECT * FROM `MergeErrors`", NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");

    hrec = libmsi_query_get_column_info(hquery, LIBMSI_COL_INFO_NAMES, NULL);
    ok(hrec, "Expected result\n");

    check_record_string(hrec, 1, "Table");
    check_record_string(hrec, 2, "NumRowMergeConflicts");
    g_object_unref(hrec);

    hrec = libmsi_query_get_column_info(hquery, LIBMSI_COL_INFO_TYPES, NULL);
    ok(hrec, "Expected result\n");

    check_record_string(hrec, 1, "s255");
    check_record_string(hrec, 2, "i2");

    g_object_unref(hrec);
    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    sql = "DROP TABLE `MergeErrors`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` CHAR(72) PRIMARY KEY `A` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `One` ( `A`, `B` ) VALUES ( 1, 'hi' )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* table from merged database is not in target database */
    r = libmsi_database_merge(hdb, href, "MergeErrors", NULL);
    ok(r, "libmsi_database_merge() failed\n");

    sql = "SELECT * FROM `One`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(hrec, 2, "hi");
    g_object_unref(hrec);

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( "
            "`A` CHAR(72), `B` INT PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( "
            "`A` CHAR(72), `B` INT PRIMARY KEY `A` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `One` ( `A`, `B` ) VALUES ( 'hi', 1 )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* primary key is string */
    r = libmsi_database_merge(hdb, href, "MergeErrors", NULL);
    ok(r, "libmsi_database_merge() failed\n");

    sql = "SELECT * FROM `One`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    check_record_string(hrec, 1, "hi");

    r = libmsi_record_get_int(hrec, 2);
    ok(r == 1, "Expected 1, got %d\n", r);

    g_object_unref(hrec);

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    create_file_data("codepage.idt", "\r\n\r\n850\t_ForceCodepage\r\n", 0);

    r = libmsi_database_import(hdb, "codepage.idt", NULL);
    ok(r, "libmsi_database_import() failed\n");

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( "
            "`A` INT, `B` CHAR(72) LOCALIZABLE PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( "
            "`A` INT, `B` CHAR(72) LOCALIZABLE PRIMARY KEY `A` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `One` ( `A`, `B` ) VALUES ( 1, 'hi' )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* code page does not match */
    r = libmsi_database_merge(hdb, href, "MergeErrors", NULL);
    ok(r, "libmsi_database_merge() failed\n");

    sql = "SELECT * FROM `One`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(hrec, 2, "hi");
    g_object_unref(hrec);

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` OBJECT PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` OBJECT PRIMARY KEY `A` )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    create_file("binary.dat");
    hrec = libmsi_record_new(1);
    libmsi_record_load_stream(hrec, 1, "binary.dat");

    sql = "INSERT INTO `One` ( `A`, `B` ) VALUES ( 1, ? )";
    r = run_query(href, hrec, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    g_object_unref(hrec);

    /* binary data to merge */
    r = libmsi_database_merge(hdb, href, "MergeErrors", NULL);
    ok(r, "libmsi_database_merge() failed\n");

    sql = "SELECT * FROM `One`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 1, "Expected 1, got %d\n", r);

    size = sizeof(buf);
    memset(buf, 0, sizeof(buf));
    in = libmsi_record_get_stream(hrec, 2);
    ok(in, "Failed to get stream\n");
    size = g_input_stream_read(in, buf, sizeof(buf), NULL, NULL);
    ok(g_str_equal(buf, "binary.dat\n"),
       "Expected \"binary.dat\\n\", got \"%s\"\n", buf);
    g_object_unref(in);
    g_object_unref(hrec);

    /* nothing in MergeErrors */
    sql = "SELECT * FROM `MergeErrors`";
    r = do_query(hdb, sql, &hrec);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "DROP TABLE `One`";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `One` ( `A` INT, `B` CHAR(72) PRIMARY KEY `A` )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `One` ( `A`, `B` ) VALUES ( 1, 'foo' )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `One` ( `A`, `B` ) VALUES ( 2, 'bar' )";
    r = run_query(href, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_database_merge(hdb, href, "MergeErrors", NULL);
    ok(r, "libmsi_database_merge() failed\n");

    sql = "SELECT * FROM `One`";
    hquery = libmsi_query_new(hdb, sql, NULL);
    ok(hquery, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(hquery, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(hrec, 2, "foo");
    g_object_unref(hrec);

    hrec = libmsi_query_fetch(hquery, NULL);
    ok(hrec, "query fetch failed\n");

    r = libmsi_record_get_int(hrec, 1);
    ok(r == 2, "Expected 2, got %d\n", r);

    check_record_string(hrec, 2, "bar");
    g_object_unref(hrec);

    query_check_no_more(hquery);

    libmsi_query_close(hquery, NULL);
    g_object_unref(hquery);

    g_object_unref(hdb);
    g_object_unref(href);
    unlink(msifile);
    unlink("refdb.msi");
    unlink("codepage.idt");
    unlink("binary.dat");
}

static void test_select_with_tablenames(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *query;
    LibmsiRecord *rec;
    const char *sql;
    unsigned r = 0;
    int i;

    int vals[4][2] = {
        {1,12},
        {4,12},
        {1,15},
        {4,15}};

    hdb = create_db();
    ok(hdb, "failed to create db\n");

    /* Build a pair of tables with the same column names, but unique data */
    sql = "CREATE TABLE `T1` ( `A` SHORT, `B` SHORT PRIMARY KEY `A`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `T1` ( `A`, `B` ) VALUES ( 1, 2 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `T1` ( `A`, `B` ) VALUES ( 4, 5 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "CREATE TABLE `T2` ( `A` SHORT, `B` SHORT PRIMARY KEY `A`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `T2` ( `A`, `B` ) VALUES ( 11, 12 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `T2` ( `A`, `B` ) VALUES ( 14, 15 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);


    /* Test that selection based on prefixing the column with the table
     * actually selects the right data */

    query = NULL;
    sql = "SELECT T1.A, T2.B FROM T1,T2";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    for (i = 0; i < 4; i++)
    {
        rec = libmsi_query_fetch(query, NULL);
        ok(rec, "query fetch failed\n");

        r = libmsi_record_get_int(rec, 1);
        ok(r == vals[i][0], "Expected %d, got %d\n", vals[i][0], r);

        r = libmsi_record_get_int(rec, 2);
        ok(r == vals[i][1], "Expected %d, got %d\n", vals[i][1], r);

        g_object_unref(rec);
    }

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref(query);
    g_object_unref(hdb);
    unlink(msifile);
}

static const unsigned ordervals[6][3] =
{
    { LIBMSI_NULL_INT, 12, 13 },
    { 1, 2, 3 },
    { 6, 4, 5 },
    { 8, 9, 7 },
    { 10, 11, LIBMSI_NULL_INT },
    { 14, LIBMSI_NULL_INT, 15 }
};

static void test_insertorder(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *query;
    LibmsiRecord *rec;
    const char *sql;
    unsigned r = 0;
    int i;

    hdb = create_db();
    ok(hdb, "failed to create db\n");

    sql = "CREATE TABLE `T` ( `A` SHORT, `B` SHORT, `C` SHORT PRIMARY KEY `A`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `T` ( `A`, `B`, `C` ) VALUES ( 1, 2, 3 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `T` ( `B`, `C`, `A` ) VALUES ( 4, 5, 6 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `T` ( `C`, `A`, `B` ) VALUES ( 7, 8, 9 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `T` ( `A`, `B` ) VALUES ( 10, 11 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `T` ( `B`, `C` ) VALUES ( 12, 13 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    /* fails because the primary key already
     * has an LIBMSI_NULL_INT value set above
     */
    sql = "INSERT INTO `T` ( `C` ) VALUES ( 14 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_FUNCTION_FAILED,
       "Expected LIBMSI_RESULT_FUNCTION_FAILED, got %d\n", r);

    /* replicate the error where primary key is set twice */
    sql = "INSERT INTO `T` ( `A`, `C` ) VALUES ( 1, 14 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_FUNCTION_FAILED,
       "Expected LIBMSI_RESULT_FUNCTION_FAILED, got %d\n", r);

    sql = "INSERT INTO `T` ( `A`, `C` ) VALUES ( 14, 15 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `T` VALUES ( 16 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `T` VALUES ( 17, 18 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    sql = "INSERT INTO `T` VALUES ( 19, 20, 21 )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_BAD_QUERY_SYNTAX,
       "Expected LIBMSI_RESULT_BAD_QUERY_SYNTAX, got %d\n", r);

    query = NULL;
    sql = "SELECT * FROM `T`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    for (i = 0; i < 6; i++)
    {
        rec = libmsi_query_fetch(query, NULL);
        ok(rec, "Expected result\n");

        r = libmsi_record_get_int(rec, 1);
        ok(r == ordervals[i][0], "Expected %d, got %d\n", ordervals[i][0], r);

        r = libmsi_record_get_int(rec, 2);
        ok(r == ordervals[i][1], "Expected %d, got %d\n", ordervals[i][1], r);

        r = libmsi_record_get_int(rec, 3);
        ok(r == ordervals[i][2], "Expected %d, got %d\n", ordervals[i][2], r);

        g_object_unref(rec);
    }

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref(query);

    sql = "DELETE FROM `T` WHERE `A` IS NULL";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "INSERT INTO `T` ( `B`, `C` ) VALUES ( 12, 13 ) TEMPORARY";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    query = NULL;
    sql = "SELECT * FROM `T`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    for (i = 0; i < 6; i++)
    {
        rec = libmsi_query_fetch(query, NULL);
        ok(rec, "Expected result\n");

        r = libmsi_record_get_int(rec, 1);
        ok(r == ordervals[i][0], "Expected %d, got %d\n", ordervals[i][0], r);

        r = libmsi_record_get_int(rec, 2);
        ok(r == ordervals[i][1], "Expected %d, got %d\n", ordervals[i][1], r);

        r = libmsi_record_get_int(rec, 3);
        ok(r == ordervals[i][2], "Expected %d, got %d\n", ordervals[i][2], r);

        g_object_unref(rec);
    }

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref(query);
    g_object_unref(hdb);
    unlink(msifile);
}

static void test_columnorder(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *query;
    LibmsiRecord *rec;
    char buf[100];
    const char *sql;
    unsigned sz;
    unsigned r = 0;

    hdb = create_db();
    ok(hdb, "failed to create db\n");

    /* Each column is a slot:
     * ---------------------
     * | B | C | A | E | D |
     * ---------------------
     *
     * When a column is selected as a primary key,
     * the column occupying the nth primary key slot is swapped
     * with the current position of the primary key in question:
     *
     * set primary key `D`
     * ---------------------    ---------------------
     * | B | C | A | E | D | -> | D | C | A | E | B |
     * ---------------------    ---------------------
     *
     * set primary key `E`
     * ---------------------    ---------------------
     * | D | C | A | E | B | -> | D | E | A | C | B |
     * ---------------------    ---------------------
     */

    sql = "CREATE TABLE `T` ( `B` SHORT NOT NULL, `C` SHORT NOT NULL, "
            "`A` CHAR(255), `E` INT, `D` CHAR(255) NOT NULL "
            "PRIMARY KEY `D`, `E`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    query = NULL;
    sql = "SELECT * FROM `T`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");

    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_TYPES, NULL);
    ok(rec, "Expected result\n");

    sz = sizeof(buf);
    strcpy(buf, "kiwi");
    check_record_string(rec, 1, "s255");
    check_record_string(rec, 2, "I2");
    check_record_string(rec, 3, "S255");
    check_record_string(rec, 4, "i2");
    check_record_string(rec, 5, "i2");
    g_object_unref(rec);

    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_NAMES, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "D");
    check_record_string(rec, 2, "E");
    check_record_string(rec, 3, "A");
    check_record_string(rec, 4, "C");
    check_record_string(rec, 5, "B");
    g_object_unref(rec);

    libmsi_query_close(query, NULL);
    g_object_unref(query);

    sql = "INSERT INTO `T` ( `B`, `C`, `A`, `E`, `D` ) "
            "VALUES ( 1, 2, 'a', 3, 'bc' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `T`";
    r = do_query(hdb, sql, &rec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    check_record_string(rec, 1, "bc");

    r = libmsi_record_get_int(rec, 2);
    ok(r == 3, "Expected 3, got %d\n", r);

    check_record_string(rec, 3, "a");

    r = libmsi_record_get_int(rec, 4);
    ok(r == 2, "Expected 2, got %d\n", r);

    r = libmsi_record_get_int(rec, 5);
    ok(r == 1, "Expected 1, got %d\n", r);

    g_object_unref(rec);

    query = NULL;
    sql = "SELECT * FROM `_Columns` WHERE `Table` = 'T'";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "T");

    r = libmsi_record_get_int(rec, 2);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(rec, 3, "D");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "T");

    r = libmsi_record_get_int(rec, 2);
    ok(r == 2, "Expected 2, got %d\n", r);

    check_record_string(rec, 3, "E");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "T");

    r = libmsi_record_get_int(rec, 2);
    ok(r == 3, "Expected 3, got %d\n", r);

    check_record_string(rec, 3, "A");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "T");

    r = libmsi_record_get_int(rec, 2);
    ok(r == 4, "Expected 4, got %d\n", r);

    check_record_string(rec, 3, "C");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "T");

    r = libmsi_record_get_int(rec, 2);
    ok(r == 5, "Expected 5, got %d\n", r);

    check_record_string(rec, 3, "B");
    g_object_unref(rec);

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref(query);

    sql = "CREATE TABLE `Z` ( `B` SHORT NOT NULL, `C` SHORT NOT NULL, "
            "`A` CHAR(255), `E` INT, `D` CHAR(255) NOT NULL "
            "PRIMARY KEY `C`, `A`, `D`)";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    query = NULL;
    sql = "SELECT * FROM `Z`";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");

    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_TYPES, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "i2");
    check_record_string(rec, 2, "S255");
    check_record_string(rec, 3, "s255");
    check_record_string(rec, 4, "I2");
    check_record_string(rec, 5, "i2");
    g_object_unref(rec);

    rec = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_NAMES, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "C");
    check_record_string(rec, 2, "A");
    check_record_string(rec, 3, "D");
    check_record_string(rec, 4, "E");
    check_record_string(rec, 5, "B");

    g_object_unref(rec);
    libmsi_query_close(query, NULL);
    g_object_unref(query);

    sql = "INSERT INTO `Z` ( `B`, `C`, `A`, `E`, `D` ) "
            "VALUES ( 1, 2, 'a', 3, 'bc' )";
    r = run_query(hdb, 0, sql);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sql = "SELECT * FROM `Z`";
    r = do_query(hdb, sql, &rec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_record_get_int(rec, 1);
    ok(r == 2, "Expected 2, got %d\n", r);

    check_record_string(rec, 2, "a");
    check_record_string(rec, 3, "bc");

    r = libmsi_record_get_int(rec, 4);
    ok(r == 3, "Expected 3, got %d\n", r);

    r = libmsi_record_get_int(rec, 5);
    ok(r == 1, "Expected 1, got %d\n", r);

    g_object_unref(rec);

    query = NULL;
    sql = "SELECT * FROM `_Columns` WHERE `Table` = 'T'";
    query = libmsi_query_new(hdb, sql, NULL);
    ok(query, "Expected LIBMSI_RESULT_SUCCESS\n");
    r = libmsi_query_execute(query, 0, NULL);
    ok(r, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "T");

    r = libmsi_record_get_int(rec, 2);
    ok(r == 1, "Expected 1, got %d\n", r);

    check_record_string(rec, 3, "D");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "T");

    r = libmsi_record_get_int(rec, 2);
    ok(r == 2, "Expected 2, got %d\n", r);

    check_record_string(rec, 3, "E");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "T");

    r = libmsi_record_get_int(rec, 2);
    ok(r == 3, "Expected 3, got %d\n", r);

    check_record_string(rec, 3, "A");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "T");

    r = libmsi_record_get_int(rec, 2);
    ok(r == 4, "Expected 4, got %d\n", r);

    check_record_string(rec, 3, "C");
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");

    check_record_string(rec, 1, "T");

    r = libmsi_record_get_int(rec, 2);
    ok(r == 5, "Expected 5, got %d\n", r);

    check_record_string(rec, 3, "B");
    g_object_unref(rec);

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref(query);

    g_object_unref(hdb);
    unlink(msifile);
}

static void test_createtable(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *htab = 0;
    LibmsiRecord *hrec = 0;
    const char *sql;
    unsigned res = 0;
    unsigned size;
    char buffer[0x20];
    gchar *str;

    hdb = create_db();
    ok(hdb, "failed to create db\n");

    sql = "CREATE TABLE `blah` (`foo` CHAR(72) NOT NULL PRIMARY KEY `foo`)";
    htab = libmsi_query_new( hdb, sql, NULL);
    ok(htab, "Expected LIBMSI_RESULT_SUCCESS\n");
    if(res == LIBMSI_RESULT_SUCCESS )
    {
        res = libmsi_query_execute( htab, hrec , NULL);
        ok(res, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", res);

        hrec = libmsi_query_get_column_info( htab, LIBMSI_COL_INFO_NAMES, NULL );
        todo_wine ok(hrec, "Expected result\n");

        str = libmsi_record_get_string(hrec, 1);
        todo_wine ok(str, "Expected string\n");
        g_free(str);
        g_object_unref( hrec );

        res = libmsi_query_close(htab , NULL);
        ok(res, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", res);

        g_object_unref( htab );
    }

    sql = "CREATE TABLE `a` (`b` INT PRIMARY KEY `b`)";
    htab = libmsi_query_new( hdb, sql, NULL);
    ok(htab, "Expected LIBMSI_RESULT_SUCCESS\n");
    if(res == LIBMSI_RESULT_SUCCESS )
    {
        res = libmsi_query_execute( htab, 0 , NULL);
        ok(res, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", res);

        res = libmsi_query_close(htab, NULL);
        ok(res, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", res);

        g_object_unref( htab );

        sql = "SELECT * FROM `a`";
        htab = libmsi_query_new( hdb, sql, NULL);
        ok(htab, "Expected LIBMSI_RESULT_SUCCESS\n");

        hrec = libmsi_query_get_column_info( htab, LIBMSI_COL_INFO_NAMES, NULL );
        ok(hrec, "Expected result\n");

        check_record_string(hrec, 1, "b");
        g_object_unref( hrec );

        res = libmsi_query_close(htab , NULL);
        ok(res, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", res);

        g_object_unref( htab );

        res = libmsi_database_commit(hdb, NULL);
        ok(res, "Expected LIBMSI_RESULT_SUCCESS\n");

        g_object_unref(hdb);

        hdb = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_TRANSACT, NULL, NULL);
        ok(hdb, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", res);

        sql = "SELECT * FROM `a`";
        htab = libmsi_query_new(hdb, sql, NULL);
        ok(htab, "Expected LIBMSI_RESULT_SUCCESS\n");

        hrec = libmsi_query_get_column_info( htab, LIBMSI_COL_INFO_NAMES, NULL );
        ok(hrec, "Expected result\n");

        check_record_string(hrec, 1, "b");
        g_object_unref( hrec );

        res = libmsi_query_close(htab , NULL);
        ok(res, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", res);

        g_object_unref( htab );
    }

    res = libmsi_database_commit(hdb, NULL);
    ok(res, "Expected LIBMSI_RESULT_SUCCESS\n");

    g_object_unref(hdb);

    unlink(msifile);
}

static void test_embedded_nulls(void)
{
    static const char control_table[] =
        "Dialog\tText\n"
        "s72\tL0\n"
        "Control\tDialog\n"
        "LicenseAgreementDlg\ttext\x11\x19text\0text";
    unsigned r = 0, sz;
    LibmsiDatabase *hdb;
    LibmsiRecord *hrec;
    char buffer[32];
    gchar *str;

    hdb = libmsi_database_new( msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "failed to open database %u\n", r );

    write_file( "temp_file", control_table, sizeof(control_table) );
    r = libmsi_database_import( hdb, "temp_file", NULL);
    ok(r, "libmsi_database_import() failed\n");
    unlink( "temp_file" );

    r = do_query( hdb, "SELECT `Text` FROM `Control` WHERE `Dialog` = 'LicenseAgreementDlg'", &hrec );
    ok( r == LIBMSI_RESULT_SUCCESS, "query failed %u\n", r );

    str = libmsi_record_get_string( hrec, 1);
    ok(str, "failed to get string\n" );
    ok( !memcmp( "text\r\ntext\ntext", str, sizeof("text\r\ntext\ntext") - 1 ), "wrong buffer contents \"%s\"\n", str );
    g_free(str);

    g_object_unref( hrec );
    g_object_unref( hdb );
    unlink( msifile );
}

static void test_select_column_names(void)
{
    LibmsiDatabase *hdb = 0;
    LibmsiRecord *rec;
    LibmsiRecord *rec2;
    LibmsiQuery *query;
    char buffer[32];
    unsigned r = 0, size;

    unlink(msifile);

    hdb = libmsi_database_new( msifile, LIBMSI_DB_FLAGS_CREATE, NULL, NULL);
    ok(hdb, "failed to open database: %u\n", r );

    r = try_query( hdb, "CREATE TABLE `t` (`a` CHAR NOT NULL, `b` CHAR PRIMARY KEY `a`)");
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "SELECT `t`.`b` FROM `t` WHERE `t`.`b` = `x`" );
    ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %u\n", r );

    r = try_query( hdb, "SELECT '', `t`.`b` FROM `t` WHERE `t`.`b` = 'x'" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "SELECT *, `t`.`b` FROM `t` WHERE `t`.`b` = 'x'" );
    todo_wine ok( r == LIBMSI_RESULT_SUCCESS, "query failed: %u\n", r );

    r = try_query( hdb, "SELECT 'b', `t`.`b` FROM `t` WHERE `t`.`b` = 'x'" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "SELECT `t`.`b`, '' FROM `t` WHERE `t`.`b` = 'x'" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "SELECT `t`.`b`, '' FROM `t` WHERE `t`.`b` = 'x' ORDER BY `b`" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "SELECT `t`.`b`, '' FROM `t` WHERE `t`.`b` = 'x' ORDER BY 'b'" );
    ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %u\n", r );

    r = try_query( hdb, "SELECT 't'.'b' FROM `t` WHERE `t`.`b` = 'x'" );
    ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %u\n", r );

    r = try_query( hdb, "SELECT 'b' FROM `t` WHERE `t`.`b` = 'x'" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "INSERT INTO `t` ( `a`, `b` ) VALUES( '1', '2' )" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "INSERT INTO `t` ( `a`, `b` ) VALUES( '3', '4' )" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    query = libmsi_query_new( hdb, "SELECT '' FROM `t`", NULL);
    ok(query, "failed to open database query\n");

    r = libmsi_query_execute( query, 0 , NULL);
    ok( r, "failed to execute query: %u\n", r );

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");
    r = libmsi_record_get_field_count( rec );
    ok( r == 1, "got %u\n",  r );

    rec2 = libmsi_query_get_column_info( query, LIBMSI_COL_INFO_NAMES, NULL );
    ok(rec2, "unexpected result\n");
    r = libmsi_record_get_field_count( rec2 );
    ok( r == 1, "got %u\n",  r );

    check_record_string(rec2, 1, "");
    g_object_unref( rec2 );

    rec2 = libmsi_query_get_column_info( query, LIBMSI_COL_INFO_TYPES, NULL );
    ok(rec2, "unexpected result\n");
    r = libmsi_record_get_field_count( rec2 );
    ok( r == 1, "got %u\n",  r );

    check_record_string( rec2, 1, "f0");
    g_object_unref( rec2 );

    check_record_string( rec, 1, "");
    g_object_unref( rec );

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");
    check_record_string(rec, 1, "");
    g_object_unref( rec );

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref( query );

    query = libmsi_query_new( hdb, "SELECT `a`, '' FROM `t`", NULL);
    ok(query, "failed to open database query\n");

    r = libmsi_query_execute( query, 0 , NULL);
    ok( r, "failed to execute query: %u\n", r );

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");
    r = libmsi_record_get_field_count( rec );
    ok( r == 2, "got %u\n",  r );
    check_record_string(rec, 1, "1");
    g_object_unref( rec );

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");
    check_record_string( rec, 2, "");
    g_object_unref( rec );

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref( query );

    query = libmsi_query_new( hdb, "SELECT '', `a` FROM `t`", NULL);
    ok(query, "failed to open database query\n");

    r = libmsi_query_execute( query, 0 , NULL);
    ok( r, "failed to execute query: %u\n", r );

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");
    r = libmsi_record_get_field_count( rec );
    ok( r == 2, "got %u\n",  r );
    check_record_string( rec, 1, "");
    check_record_string( rec, 2, "1");
    g_object_unref( rec );

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");
    check_record_string( rec, 1, "");
    check_record_string(rec, 2, "3");
    g_object_unref( rec );

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref( query );

    query = libmsi_query_new( hdb, "SELECT `a`, '', `b` FROM `t`", NULL );
    ok(query, "failed to open database query\n");

    r = libmsi_query_execute( query, 0 , NULL);
    ok( r, "failed to execute query: %u\n", r );

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");
    r = libmsi_record_get_field_count( rec );
    ok( r == 3, "got %u\n",  r );
    check_record_string(rec, 1, "1");
    check_record_string(rec, 2, "");
    check_record_string(rec, 3, "2");
    g_object_unref( rec );

    rec = libmsi_query_fetch(query, NULL);
    ok(rec, "Expected result\n");
    check_record_string(rec, 1, "3");
    check_record_string(rec, 2, "");
    check_record_string(rec, 3, "4");
    g_object_unref( rec );

    query_check_no_more(query);

    libmsi_query_close(query, NULL);
    g_object_unref( query );

    r = try_query( hdb, "SELECT '' FROM `t` WHERE `t`.`b` = 'x'" );
    ok( r == LIBMSI_RESULT_SUCCESS , "query failed: %u\n", r );

    r = try_query( hdb, "SELECT `` FROM `t` WHERE `t`.`b` = 'x'" );
    todo_wine ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %u\n", r );

    r = try_query( hdb, "SELECT `b` FROM 't' WHERE `t`.`b` = 'x'" );
    ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %u\n", r );

    r = try_query( hdb, "SELECT `b` FROM `t` WHERE 'b' = 'x'" );
    ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %u\n", r );

    r = try_query( hdb, "SELECT `t`.`b`, `` FROM `t` WHERE `t`.`b` = 'x'" );
    todo_wine ok( r == LIBMSI_RESULT_BAD_QUERY_SYNTAX, "query failed: %u\n", r );

    g_object_unref( hdb );
}

int main()
{
#if !GLIB_CHECK_VERSION(2,35,1)
    g_type_init ();
#endif

    test_msidatabase();
    test_msiinsert();
    test_msibadqueries();
    test_querygetcolumninfo();
    test_getcolinfo();
    test_msiexport();
    test_longstrings();
    test_streamtable();
    test_binary();
    test_where_not_in_selected();
    test_where();
    test_msiimport();
    test_binary_import();
    test_markers();
    test_handle_limit();
#if 0
    test_try_transform();
#endif
    test_join();
    test_temporary_table();
    test_alter();
    test_integers();
    test_update();
    test_special_tables();
    test_tables_order();
    test_rows_order();
    test_select_markers();
#if 0
    test_stringtable();
#endif
    test_defaultdatabase();
    test_order();
#if 0
    test_deleterow();
#endif
    test_quotes();
    test_carriagereturn();
    test_noquotes();
    test_forcecodepage();
#if 0
    test_storages_table();
#endif
    test_droptable();
#if 0
    test_dbmerge();
#endif
    test_select_with_tablenames();
    test_insertorder();
    test_columnorder();
    test_suminfo_import();
#if 0
    test_createtable();
#endif
    test_collation();
    test_embedded_nulls();
    test_select_column_names();
}
