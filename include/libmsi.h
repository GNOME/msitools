/*
 * Copyright (C) 2002,2003 Mike McCormack
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

#ifndef _LIBMSI_H
#define _LIBMSI_H

#include <stdint.h>
#include <stdbool.h>

typedef struct LibmsiQuery LibmsiQuery;
typedef struct LibmsiDatabase LibmsiDatabase;
typedef struct LibmsiRecord LibmsiRecord;
typedef struct LibmsiSummaryInfo LibmsiSummaryInfo;

typedef enum LibmsiCondition
{
    LIBMSI_CONDITION_FALSE = 0,
    LIBMSI_CONDITION_TRUE  = 1,
    LIBMSI_CONDITION_NONE  = 2,
    LIBMSI_CONDITION_ERROR = 3,
} LibmsiCondition;

typedef enum LibmsiResult
{
    LIBMSI_RESULT_SUCCESS = 0,
    LIBMSI_RESULT_ACCESS_DENIED = 5,
    LIBMSI_RESULT_INVALID_HANDLE = 6,
    LIBMSI_RESULT_NOT_ENOUGH_MEMORY = 8,
    LIBMSI_RESULT_INVALID_DATA = 13,
    LIBMSI_RESULT_OUTOFMEMORY = 14,
    LIBMSI_RESULT_INVALID_PARAMETER = 87,
    LIBMSI_RESULT_OPEN_FAILED = 110,
    LIBMSI_RESULT_CALL_NOT_IMPLEMENTED = 120,
    LIBMSI_RESULT_MORE_DATA = 234,
    LIBMSI_RESULT_NO_MORE_ITEMS = 259,
    LIBMSI_RESULT_NOT_FOUND = 1168,
    LIBMSI_RESULT_CONTINUE = 1246,
    LIBMSI_RESULT_UNKNOWN_PROPERTY = 1608,
    LIBMSI_RESULT_BAD_QUERY_SYNTAX = 1615,
    LIBMSI_RESULT_INVALID_FIELD = 1616,
    LIBMSI_RESULT_FUNCTION_FAILED = 1627,
    LIBMSI_RESULT_INVALID_TABLE = 1628,
    LIBMSI_RESULT_DATATYPE_MISMATCH = 1629,
    LIBMSI_RESULT_INVALID_DATATYPE = 1804
} LibmsiResult;

typedef enum LibmsiPropertyType
{
   LIBMSI_PROPERTY_TYPE_EMPTY = 0,
   LIBMSI_PROPERTY_TYPE_INT = 1,
   LIBMSI_PROPERTY_TYPE_STRING = 2,
   LIBMSI_PROPERTY_TYPE_FILETIME = 3,
} LibmsiPropertyType;

#define MSI_NULL_INTEGER 0x80000000

typedef enum LibmsiColInfo
{
    LIBMSI_COL_INFO_NAMES = 0,
    LIBMSI_COL_INFO_TYPES = 1
} LibmsiColInfo;

#define LIBMSI_DB_OPEN_READONLY (const char *)0
#define LIBMSI_DB_OPEN_TRANSACT (const char *)1
#define LIBMSI_DB_OPEN_CREATE   (const char *)2

#define LIBMSI_DB_OPEN_PATCHFILE 32 / sizeof(*LIBMSI_DB_OPEN_READONLY)

typedef enum LibmsiDBError
{
    LIBMSI_DB_ERROR_INVALIDARG = -3,
    LIBMSI_DB_ERROR_MOREDATA = -2,
    LIBMSI_DB_ERROR_FUNCTIONERROR = -1,
    LIBMSI_DB_ERROR_NOERROR = 0,
    LIBMSI_DB_ERROR_DUPLICATEKEY = 1,
    LIBMSI_DB_ERROR_REQUIRED = 2,
    LIBMSI_DB_ERROR_BADLINK = 3,
    LIBMSI_DB_ERROR_OVERFLOW = 4,
    LIBMSI_DB_ERROR_UNDERFLOW = 5,
    LIBMSI_DB_ERROR_NOTINSET = 6,
    LIBMSI_DB_ERROR_BADVERSION = 7,
    LIBMSI_DB_ERROR_BADCASE = 8,
    LIBMSI_DB_ERROR_BADGUID = 9,
    LIBMSI_DB_ERROR_BADWILDCARD = 10,
    LIBMSI_DB_ERROR_BADIDENTIFIER = 11,
    LIBMSI_DB_ERROR_BADLANGUAGE = 12,
    LIBMSI_DB_ERROR_BADFILENAME = 13,
    LIBMSI_DB_ERROR_BADPATH = 14,
    LIBMSI_DB_ERROR_BADCONDITION = 15,
    LIBMSI_DB_ERROR_BADFORMATTED = 16,
    LIBMSI_DB_ERROR_BADTEMPLATE = 17,
    LIBMSI_DB_ERROR_BADDEFAULTDIR = 18,
    LIBMSI_DB_ERROR_BADREGPATH = 19,
    LIBMSI_DB_ERROR_BADCUSTOMSOURCE = 20,
    LIBMSI_DB_ERROR_BADPROPERTY = 21,
    LIBMSI_DB_ERROR_MISSINGDATA = 22,
    LIBMSI_DB_ERROR_BADCATEGORY = 23,
    LIBMSI_DB_ERROR_BADKEYTABLE = 24,
    LIBMSI_DB_ERROR_BADMAXMINVALUES = 25,
    LIBMSI_DB_ERROR_BADCABINET = 26,
    LIBMSI_DB_ERROR_BADSHORTCUT= 27,
    LIBMSI_DB_ERROR_STRINGOVERFLOW = 28,
    LIBMSI_DB_ERROR_BADLOCALIZEATTRIB = 29
} LibmsiDBError; 

typedef enum LibmsiDBState
{
    LIBMSI_DB_STATE_ERROR = -1,
    LIBMSI_DB_STATE_READ = 0,
    LIBMSI_DB_STATE_WRITE = 1
} LibmsiDBState;


#ifdef __cplusplus
extern "C" {
#endif

#define MSI_PID_DICTIONARY (0)
#define MSI_PID_CODEPAGE (0x1)
#define MSI_PID_FIRST_USABLE 2
#define MSI_PID_TITLE 2
#define MSI_PID_SUBJECT 3
#define MSI_PID_AUTHOR 4
#define MSI_PID_KEYWORDS 5
#define MSI_PID_COMMENTS 6
#define MSI_PID_TEMPLATE 7
#define MSI_PID_LASTAUTHOR 8
#define MSI_PID_REVNUMBER 9
#define MSI_PID_EDITTIME 10
#define MSI_PID_LASTPRINTED 11
#define MSI_PID_CREATE_DTM 12
#define MSI_PID_LASTSAVE_DTM 13
#define MSI_PID_PAGECOUNT 14
#define MSI_PID_WORDCOUNT 15
#define MSI_PID_CHARCOUNT 16
#define MSI_PID_THUMBNAIL 17
#define MSI_PID_APPNAME 18
#define MSI_PID_SECURITY 19

#define MSI_PID_MSIVERSION MSI_PID_PAGECOUNT
#define MSI_PID_MSISOURCE MSI_PID_WORDCOUNT
#define MSI_PID_MSIRESTRICT MSI_PID_CHARCOUNT


/* view manipulation */
LibmsiResult libmsi_query_fetch(LibmsiQuery *,LibmsiRecord **);
LibmsiResult libmsi_query_execute(LibmsiQuery *,LibmsiRecord *);
LibmsiResult libmsi_query_close(LibmsiQuery *);
LibmsiResult libmsi_database_open_query(LibmsiDatabase *,const char *,LibmsiQuery **);
LibmsiDBError libmsi_query_get_error(LibmsiQuery *,char *,unsigned *);

LibmsiDBState libmsi_database_get_state(LibmsiDatabase *);

/* record manipulation */
LibmsiRecord * libmsi_record_create(unsigned);
LibmsiResult libmsi_record_clear_data(LibmsiRecord *);
LibmsiResult libmsi_record_set_int(LibmsiRecord *,unsigned,int);
LibmsiResult libmsi_record_set_string(LibmsiRecord *,unsigned,const char *);
LibmsiResult libmsi_record_get_string(const LibmsiRecord *,unsigned,char *,unsigned *);
unsigned libmsi_record_get_field_count(const LibmsiRecord *);
int libmsi_record_get_integer(const LibmsiRecord *,unsigned);
unsigned libmsi_record_get_field_size(const LibmsiRecord *,unsigned);
bool libmsi_record_is_null(const LibmsiRecord *,unsigned);

LibmsiResult libmsi_record_load_stream(LibmsiRecord *,unsigned,const char *);
LibmsiResult libmsi_record_save_stream(LibmsiRecord *,unsigned,char*,unsigned *);

LibmsiResult libmsi_database_get_primary_keys(LibmsiDatabase *,const char *,LibmsiRecord **);

/* database transforms */
LibmsiResult libmsi_database_apply_transform(LibmsiDatabase *,const char *,int);

LibmsiResult libmsi_query_get_column_info(LibmsiQuery *, LibmsiColInfo, LibmsiRecord **);

LibmsiResult libmsi_summary_info_get_property(LibmsiSummaryInfo *, LibmsiPropertyType,unsigned *,int *,uint64_t*,char *,unsigned *);

LibmsiResult libmsi_summary_info_set_property(LibmsiSummaryInfo *, LibmsiPropertyType, unsigned, int, uint64_t*, const char *);

LibmsiResult libmsi_database_export(LibmsiDatabase *, const char *, int fd);

LibmsiResult libmsi_database_import(LibmsiDatabase *, const char *, const char *);

LibmsiResult libmsi_database_open(const char *, const char *, LibmsiDatabase **);

LibmsiCondition libmsi_database_is_table_persistent(LibmsiDatabase *, const char *);

LibmsiResult libmsi_summary_info_persist(LibmsiSummaryInfo *);
LibmsiResult libmsi_summary_info_get_property_count(LibmsiSummaryInfo *,unsigned *);

LibmsiResult libmsi_database_merge(LibmsiDatabase *, LibmsiDatabase *, const char *);

/* Non Unicode */
LibmsiResult libmsi_database_get_summary_info(LibmsiDatabase *, unsigned, LibmsiSummaryInfo **);
LibmsiResult libmsi_database_commit(LibmsiDatabase *);
LibmsiResult libmsi_unref(void *);

#ifdef __cplusplus
}
#endif

#endif /* _LIBMSI_H */
