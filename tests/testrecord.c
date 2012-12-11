/*
 * Copyright (C) 2005 Mike McCormack for CodeWeavers
 *
 * A test program for MSI records
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

#include <libmsi.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "test.h"

static const char *msifile = "winetest-record.msi";

static bool create_temp_file(char *name)
{
    unsigned r;
    unsigned char buffer[26], i;
    unsigned sz;
    int fd;
    
#ifdef _WIN32
    r = GetTempFileName(".", "msitest",0,name);
    if(!r)
        return r;
    fd = open(name, O_WRONLY);
#else
    strcpy(name, "msitext-XXXXXX.tmp");
    fd = mkstemp(name);
#endif

    if(fd == -1)
        return false;
    for(i=0; i<26; i++)
        buffer[i]=i+'a';
    write(fd, buffer, sizeof(buffer));
    close(fd);
    return true;
}

static void test_msirecord(void)
{
    unsigned r, sz;
    int i;
    LibmsiRecord *h;
    char buf[10];
    const char str[] = "hello";
    char filename[100];

    /* check behaviour with an invalid record */
    r = libmsi_record_get_field_count(0);
    ok(r==-1, "field count for invalid record not -1\n");
    r = libmsi_record_is_null(0, 0);
    ok(r==0, "invalid handle not considered to be non-null...\n");
    r = libmsi_record_get_integer(0,0);
    ok(r == MSI_NULL_INTEGER, "got integer from invalid record\n");
    r = libmsi_record_set_int(0,0,0);
    ok(r == LIBMSI_RESULT_INVALID_HANDLE, "libmsi_record_set_int returned wrong error\n");
    r = libmsi_record_set_int(0,-1,0);
    ok(r == LIBMSI_RESULT_INVALID_HANDLE, "libmsi_record_set_int returned wrong error\n");
    r = libmsi_record_clear_data(0);
    ok(r == LIBMSI_RESULT_INVALID_HANDLE, "libmsi_record_clear_data returned wrong error\n");
    r = libmsi_record_get_field_size(0,0);
    ok(r == 0, "libmsi_record_get_field_size returned wrong error\n");


    /* check behaviour of a record with 0 elements */
    h = libmsi_record_new(0);
    ok(h!=0, "couldn't create record with zero elements\n");
    r = libmsi_record_get_field_count(h);
    ok(r==0, "field count should be zero\n");
    r = libmsi_record_is_null(h,0);
    ok(r, "new record wasn't null\n");
    r = libmsi_record_is_null(h,1);
    ok(r, "out of range record wasn't null\n");
    r = libmsi_record_is_null(h,-1);
    ok(r, "out of range record wasn't null\n");
    r = libmsi_record_get_field_size(h,0);
    ok(r==0, "size of null record is 0\n");
    sz = sizeof buf;
    strcpy(buf,"x");
    r = libmsi_record_get_string(h, 0, buf, &sz);
    ok(r==LIBMSI_RESULT_SUCCESS, "failed to get null string\n");
    ok(sz==0, "null string too long\n");
    ok(buf[0]==0, "null string not set\n");

    /* same record, but add an integer to it */
    r = libmsi_record_set_int(h, 0, 0);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to set integer at 0 to 0\n");
    r = libmsi_record_is_null(h,0);
    ok(r==0, "new record is null after setting an integer\n");
    r = libmsi_record_get_field_size(h,0);
    ok(r==sizeof(unsigned), "size of integer record is 4\n");
    r = libmsi_record_set_int(h, 0, 1);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to set integer at 0 to 1\n");
    r = libmsi_record_set_int(h, 1, 1);
    ok(r == LIBMSI_RESULT_INVALID_PARAMETER, "set integer at 1\n");
    r = libmsi_record_set_int(h, -1, 0);
    ok(r == LIBMSI_RESULT_INVALID_PARAMETER, "set integer at -1\n");
    r = libmsi_record_is_null(h,0);
    ok(r==0, "new record is null after setting an integer\n");
    r = libmsi_record_get_integer(h, 0);
    ok(r == 1, "failed to get integer\n");

    /* same record, but add a null or empty string to it */
    r = libmsi_record_set_string(h, 0, NULL);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to set null string at 0\n");
    r = libmsi_record_is_null(h, 0);
    ok(r == true, "null string not null field\n");
    r = libmsi_record_get_field_size(h, 0);
    ok(r == 0, "size of string record is strlen\n");
    buf[0] = 0;
    sz = sizeof buf;
    r = libmsi_record_get_string(h, 0, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to get string at 0\n");
    ok(buf[0] == 0, "libmsi_record_get_string returned the wrong string\n");
    ok(sz == 0, "libmsi_record_get_string returned the wrong length\n");
    r = libmsi_record_set_string(h, 0, "");
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to set empty string at 0\n");
    r = libmsi_record_is_null(h, 0);
    ok(r == true, "null string not null field\n");
    r = libmsi_record_get_field_size(h, 0);
    ok(r == 0, "size of string record is strlen\n");
    buf[0] = 0;
    sz = sizeof buf;
    r = libmsi_record_get_string(h, 0, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to get string at 0\n");
    ok(buf[0] == 0, "libmsi_record_get_string returned the wrong string\n");
    ok(sz == 0, "libmsi_record_get_string returned the wrong length\n");

    /* same record, but add a string to it */
    r = libmsi_record_set_string(h,0,str);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    r = libmsi_record_get_integer(h, 0);
    ok(r == MSI_NULL_INTEGER, "should get invalid integer\n");
    r = libmsi_record_get_field_size(h,0);
    ok(r==sizeof str-1, "size of string record is strlen\n");
    buf[0]=0;
    sz = sizeof buf;
    r = libmsi_record_get_string(h,0,buf,&sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to get string at 0\n");
    ok(0==strcmp(buf,str), "libmsi_record_get_string returned the wrong string\n");
    ok(sz == sizeof str-1, "libmsi_record_get_string returned the wrong length\n");
    buf[0]=0;
    sz = sizeof str - 2;
    r = libmsi_record_get_string(h,0,buf,&sz);
    ok(r == LIBMSI_RESULT_MORE_DATA, "small buffer should yield LIBMSI_RESULT_MORE_DATA\n");
    ok(sz == sizeof str-1, "libmsi_record_get_string returned the wrong length\n");
    ok(0==strncmp(buf,str,sizeof str-3), "libmsi_record_get_string returned the wrong string\n");
    ok(buf[sizeof str - 3]==0, "string wasn't nul terminated\n");

    buf[0]=0;
    sz = sizeof str;
    r = libmsi_record_get_string(h,0,buf,&sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "wrong error\n");
    ok(sz == sizeof str-1, "libmsi_record_get_string returned the wrong length\n");
    ok(0==strcmp(buf,str), "libmsi_record_get_string returned the wrong string\n");


    memset(buf, 0, sizeof buf);
    sz = 5;
    r = libmsi_record_get_string(h,0,buf,&sz);
    ok(r == LIBMSI_RESULT_MORE_DATA, "wrong error\n");
    ok(sz == 5, "libmsi_record_get_string returned the wrong length\n");
    ok(0==memcmp(buf,str,4), "libmsi_record_get_string returned the wrong string\n");

    sz = 0;
    buf[0] = 'x';
    r = libmsi_record_get_string(h,0,buf,&sz);
    ok(r == LIBMSI_RESULT_MORE_DATA, "wrong error\n");
    ok(sz == 5, "libmsi_record_get_string returned the wrong length\n");
    ok('x'==buf[0], "libmsi_record_get_string returned the wrong string\n");

    /* same record, check we can wipe all the data */
    r = libmsi_record_clear_data(h);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to clear record\n");
    r = libmsi_record_clear_data(h);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to clear record again\n");
    r = libmsi_record_is_null(h,0);
    ok(r, "cleared record wasn't null\n");

    /* same record, try converting strings to integers */
    i = libmsi_record_set_string(h,0,"42");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == 42, "should get invalid integer\n");
    i = libmsi_record_set_string(h,0,"-42");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == -42, "should get invalid integer\n");
    i = libmsi_record_set_string(h,0," 42");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == MSI_NULL_INTEGER, "should get invalid integer\n");
    i = libmsi_record_set_string(h,0,"42 ");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == MSI_NULL_INTEGER, "should get invalid integer\n");
    i = libmsi_record_set_string(h,0,"42.0");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == MSI_NULL_INTEGER, "should get invalid integer\n");
    i = libmsi_record_set_string(h,0,"0x42");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == MSI_NULL_INTEGER, "should get invalid integer\n");
    i = libmsi_record_set_string(h,0,"1000000000000000");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == -1530494976, "should get truncated integer\n");
    i = libmsi_record_set_string(h,0,"2147483647");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == 2147483647, "should get maxint\n");
    i = libmsi_record_set_string(h,0,"-2147483647");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == -2147483647, "should get -maxint-1\n");
    i = libmsi_record_set_string(h,0,"4294967297");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == 1, "should get one\n");
    i = libmsi_record_set_string(h,0,"foo");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == MSI_NULL_INTEGER, "should get zero\n");
    i = libmsi_record_set_string(h,0,"");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == MSI_NULL_INTEGER, "should get zero\n");
    i = libmsi_record_set_string(h,0,"+1");
    ok(i == LIBMSI_RESULT_SUCCESS, "Failed to set string at 0\n");
    i = libmsi_record_get_integer(h, 0);
    ok(i == MSI_NULL_INTEGER, "should get zero\n");

    /* same record, try converting integers to strings */
    r = libmsi_record_set_int(h, 0, 32);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to set integer at 0 to 32\n");
    sz = 1;
    r = libmsi_record_get_string(h, 0, NULL, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to get string from integer\n");
    ok(sz == 2, "length wrong\n");
    buf[0]=0;
    sz = sizeof buf;
    r = libmsi_record_get_string(h, 0, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to get string from integer\n");
    ok(0==strcmp(buf,"32"), "failed to get string from integer\n");
    r = libmsi_record_set_int(h, 0, -32);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to set integer at 0 to 32\n");
    buf[0]=0;
    sz = 1;
    r = libmsi_record_get_string(h, 0, NULL, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to get string from integer\n");
    ok(sz == 3, "length wrong\n");
    sz = sizeof buf;
    r = libmsi_record_get_string(h, 0, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to get string from integer\n");
    ok(0==strcmp(buf,"-32"), "failed to get string from integer\n");
    buf[0]=0;

    /* same record, now try streams */
    r = libmsi_record_load_stream(h, 0, NULL);
    ok(r == LIBMSI_RESULT_INVALID_PARAMETER, "set NULL stream\n");
    sz = sizeof buf;
    r = libmsi_record_save_stream(h, 0, buf, &sz);
    ok(r == LIBMSI_RESULT_INVALID_DATATYPE, "read non-stream type\n");
    ok(sz == sizeof buf, "set sz\n");
    r = libmsi_record_get_field_size( h, -1);
    ok(r == 0,"libmsi_record_get_field_size returned wrong size\n");
    r = libmsi_record_get_field_size( h, 0);
    ok(r == 4,"libmsi_record_get_field_size returned wrong size\n");

    /* same record, now close it */
    r = libmsi_unref(h);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to close handle\n");

    /* now try streams in a new record - need to create a file to play with */
    r = create_temp_file(filename); 
    if(!r)
        return;

    /* streams can't be inserted in field 0 for some reason */
    h = libmsi_record_new(2);
    ok(h, "couldn't create a two field record\n");
    r = libmsi_record_load_stream(h, 0, filename);
    ok(r == LIBMSI_RESULT_INVALID_PARAMETER, "added stream to field 0\n");
    r = libmsi_record_load_stream(h, 1, filename);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to add stream to record\n");
    r = libmsi_record_save_stream(h, 1, buf, NULL);
    ok(r == LIBMSI_RESULT_INVALID_PARAMETER, "should return error\n");
    unlink(filename); /* Windows 98 doesn't like this at all, so don't check return. */
    r = libmsi_record_save_stream(h, 1, NULL, NULL);
    ok(r == LIBMSI_RESULT_INVALID_PARAMETER, "should return error\n");
    sz = sizeof buf;
    r = libmsi_record_save_stream(h, 1, NULL, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to read stream\n");
    ok(sz==26,"couldn't get size of stream\n");
    sz = 0;
    r = libmsi_record_save_stream(h, 1, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to read stream\n");
    ok(sz==0,"short read\n");
    sz = sizeof buf;
    r = libmsi_record_save_stream(h, 1, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to read stream\n");
    ok(sz==sizeof buf,"short read\n");
    ok(!strncmp(buf,"abcdefghij",10), "read the wrong thing\n");
    sz = sizeof buf;
    r = libmsi_record_save_stream(h, 1, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to read stream\n");
    ok(sz==sizeof buf,"short read\n");
    ok(!strncmp(buf,"klmnopqrst",10), "read the wrong thing\n");
    memset(buf,0,sizeof buf);
    sz = sizeof buf;
    r = libmsi_record_save_stream(h, 1, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to read stream\n");
    ok(sz==6,"short read\n");
    ok(!strcmp(buf,"uvwxyz"), "read the wrong thing\n");
    memset(buf,0,sizeof buf);
    sz = sizeof buf;
    r = libmsi_record_save_stream(h, 1, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to read stream\n");
    ok(sz==0,"size non-zero at end of stream\n");
    ok(buf[0]==0, "read something at end of the stream\n");
    r = libmsi_record_load_stream(h, 1, NULL);
    ok(r == LIBMSI_RESULT_SUCCESS, "failed to reset stream\n");
    sz = 0;
    r = libmsi_record_save_stream(h, 1, NULL, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "bytes left wrong after reset\n");
    ok(sz==26,"couldn't get size of stream\n");
    r = libmsi_record_get_field_size(h,1);
    ok(r == 26,"libmsi_record_get_field_size returned wrong size\n");

    /* now close the stream record */
    r = libmsi_unref(h);
    ok(r == LIBMSI_RESULT_SUCCESS, "Failed to close handle\n");
    unlink(filename); /* Delete it for sure, when everything else is closed. */
}

static void test_MsiRecordGetString(void)
{
    LibmsiRecord *rec;
    char buf[100];
    unsigned sz;
    unsigned r;

    rec = libmsi_record_new(2);
    ok(rec != 0, "Expected a valid handle\n");

    sz = sizeof(buf);
    r = libmsi_record_get_string(rec, 1, NULL, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n",r);
    ok(sz == 0, "Expected 0, got %d\n",sz);

    sz = sizeof(buf);
    strcpy(buf, "apple");
    r = libmsi_record_get_string(rec, 1, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    ok(!strcmp(buf, ""), "Expected \"\", got \"%s\"\n", buf);
    ok(sz == 0, "Expected 0, got %d\n", sz);

    sz = sizeof(buf);
    strcpy(buf, "apple");
    r = libmsi_record_get_string(rec, 10, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    ok(!strcmp(buf, ""), "Expected \"\", got \"%s\"\n", buf);
    ok(sz == 0, "Expected 0, got %d\n", sz);

    libmsi_unref(rec);

    rec = libmsi_record_new(1);
    ok(rec != 0, "Expected a valid handle\n");

    r = libmsi_record_set_int(rec, 1, 5);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sz = sizeof(buf);
    r = libmsi_record_get_string(rec, 1, NULL, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n",r);
    ok(sz == 1, "Expected 1, got %d\n",sz);

    sz = sizeof(buf);
    strcpy(buf, "apple");
    r = libmsi_record_get_string(rec, 1, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    ok(!strcmp(buf, "5"), "Expected \"5\", got \"%s\"\n", buf);
    ok(sz == 1, "Expectd 1, got %d\n", sz);

    r = libmsi_record_set_int(rec, 1, -5);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    sz = sizeof(buf);
    strcpy(buf, "apple");
    r = libmsi_record_get_string(rec, 1, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    ok(!strcmp(buf, "-5"), "Expected \"-5\", got \"%s\"\n", buf);
    ok(sz == 2, "Expectd 2, got %d\n", sz);

    libmsi_unref(rec);
}

static void test_MsiRecordGetInteger(void)
{
    LibmsiRecord *rec;
    int val;
    unsigned r;

    rec = libmsi_record_new(1);
    ok(rec != 0, "Expected a valid handle\n");

    r = libmsi_record_set_string(rec, 1, "5");
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    val = libmsi_record_get_integer(rec, 1);
    ok(val == 5, "Expected 5, got %d\n", val);

    r = libmsi_record_set_string(rec, 1, "-5");
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    val = libmsi_record_get_integer(rec, 1);
    ok(val == -5, "Expected -5, got %d\n", val);

    r = libmsi_record_set_string(rec, 1, "5apple");
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    val = libmsi_record_get_integer(rec, 1);
    ok(val == MSI_NULL_INTEGER, "Expected MSI_NULL_INTEGER, got %d\n", val);

    libmsi_unref(rec);
}

static void test_fieldzero(void)
{
    LibmsiDatabase *hdb;
    LibmsiQuery *hview;
    LibmsiRecord *rec;
    char buf[100];
    const char *query;
    unsigned sz;
    unsigned r;

    rec = libmsi_record_new(1);
    ok(rec != 0, "Expected a valid handle\n");

    r = libmsi_record_get_integer(rec, 0);
    ok(r == MSI_NULL_INTEGER, "Expected MSI_NULL_INTEGER, got %d\n", r);

    sz = sizeof(buf);
    strcpy(buf, "apple");
    r = libmsi_record_get_string(rec, 0, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    ok(!strcmp(buf, ""), "Expected \"\", got \"%s\"\n", buf);
    ok(sz == 0, "Expectd 0, got %d\n", sz);

    r = libmsi_record_is_null(rec, 0);
    ok(r == true, "Expected true, got %d\n", r);

    r = libmsi_record_get_integer(rec, 1);
    ok(r == MSI_NULL_INTEGER, "Expected MSI_NULL_INTEGER, got %d\n", r);

    r = libmsi_record_set_int(rec, 1, 42);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_record_get_integer(rec, 0);
    ok(r == MSI_NULL_INTEGER, "Expected MSI_NULL_INTEGER, got %d\n", r);

    sz = sizeof(buf);
    strcpy(buf, "apple");
    r = libmsi_record_get_string(rec, 0, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    ok(!strcmp(buf, ""), "Expected \"\", got \"%s\"\n", buf);
    ok(sz == 0, "Expectd 0, got %d\n", sz);

    r = libmsi_record_is_null(rec, 0);
    ok(r == true, "Expected true, got %d\n", r);

    r = libmsi_record_get_integer(rec, 1);
    ok(r == 42, "Expected 42, got %d\n", r);

    r = libmsi_record_set_string(rec, 1, "bologna");
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_record_get_integer(rec, 0);
    ok(r == MSI_NULL_INTEGER, "Expected MSI_NULL_INTEGER, got %d\n", r);

    sz = sizeof(buf);
    strcpy(buf, "apple");
    r = libmsi_record_get_string(rec, 0, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    ok(!strcmp(buf, ""), "Expected \"\", got \"%s\"\n", buf);
    ok(sz == 0, "Expectd 0, got %d\n", sz);

    r = libmsi_record_is_null(rec, 0);
    ok(r == true, "Expected true, got %d\n", r);

    sz = sizeof(buf);
    strcpy(buf, "apple");
    r = libmsi_record_get_string(rec, 1, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    ok(!strcmp(buf, "bologna"), "Expected \"bologna\", got \"%s\"\n", buf);
    ok(sz == 7, "Expectd 7, got %d\n", sz);

    libmsi_unref(rec);

    r = libmsi_database_open(msifile, LIBMSI_DB_OPEN_CREATE, &hdb);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_database_open failed\n");

    query = "CREATE TABLE `drone` ( "
           "`id` INT, `name` CHAR(32), `number` CHAR(32) "
           "PRIMARY KEY `id`)";
    r = libmsi_database_open_query(hdb, query, &hview);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    r = libmsi_query_execute(hview, 0);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    r = libmsi_query_close(hview);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_query_close failed\n");
    r = libmsi_unref(hview);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_unref failed\n");

    query = "INSERT INTO `drone` ( `id`, `name`, `number` )"
           "VALUES('1', 'Abe', '8675309')";
    r = libmsi_database_open_query(hdb, query, &hview);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_database_open_query failed\n");
    r = libmsi_query_execute(hview, 0);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_query_execute failed\n");
    r = libmsi_query_close(hview);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_query_close failed\n");
    r = libmsi_unref(hview);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_unref failed\n");

    rec = NULL;
    r = libmsi_database_get_primary_keys(hdb, "drone", &rec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_record_get_integer(rec, 0);
    ok(r == MSI_NULL_INTEGER, "Expected MSI_NULL_INTEGER, got %d\n", r);

    sz = sizeof(buf);
    strcpy(buf, "apple");
    r = libmsi_record_get_string(rec, 0, buf, &sz);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    ok(!strcmp(buf, "drone"), "Expected \"drone\", got \"%s\"\n", buf);
    ok(sz == 5, "Expectd 5, got %d\n", sz);

    r = libmsi_record_is_null(rec, 0);
    ok(r == false, "Expected false, got %d\n", r);

    libmsi_unref(rec);

    r = libmsi_database_get_primary_keys(hdb, "nosuchtable", &rec);
    ok(r == LIBMSI_RESULT_INVALID_TABLE, "Expected LIBMSI_RESULT_INVALID_TABLE, got %d\n", r);

    query = "SELECT * FROM `drone` WHERE `id` = 1";
    r = libmsi_database_open_query(hdb, query, &hview);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    r = libmsi_query_execute(hview, 0);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);
    r = libmsi_query_fetch(hview, &rec);
    ok(r == LIBMSI_RESULT_SUCCESS, "Expected LIBMSI_RESULT_SUCCESS, got %d\n", r);

    r = libmsi_record_get_integer(rec, 0);
    ok(r == MSI_NULL_INTEGER, "Expected NULL value, got %d\n", r);
    r = libmsi_record_is_null(rec, 0);
    ok(r == true, "Expected true, got %d\n", r);

    r = libmsi_unref(hview);
    ok(r == LIBMSI_RESULT_SUCCESS, "libmsi_unref failed\n");
    libmsi_unref(rec);
    libmsi_unref(hdb);
    unlink(msifile);
}

void main()
{
    test_msirecord();
    test_MsiRecordGetString();
    test_MsiRecordGetInteger();
    test_fieldzero();
}
