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

#define MSI_NULL_INTEGER 0x80000000

typedef enum LibmsiColInfo
{
    LIBMSI_COL_INFO_NAMES = 0,
    LIBMSI_COL_INFO_TYPES = 1
} LibmsiColInfo;

typedef enum LibmsiModify
{
    LIBMSI_MODIFY_SEEK = -1,
    LIBMSI_MODIFY_REFRESH = 0,
    LIBMSI_MODIFY_INSERT = 1,
    LIBMSI_MODIFY_UPDATE = 2,
    LIBMSI_MODIFY_ASSIGN = 3,
    LIBMSI_MODIFY_REPLACE = 4,
    LIBMSI_MODIFY_MERGE = 5,
    LIBMSI_MODIFY_DELETE = 6,
    LIBMSI_MODIFY_INSERT_TEMPORARY = 7,
    LIBMSI_MODIFY_VALIDATE = 8,
    LIBMSI_MODIFY_VALIDATE_NEW = 9,
    LIBMSI_MODIFY_VALIDATE_FIELD = 10,
    LIBMSI_MODIFY_VALIDATE_DELETE = 11
} LibmsiModify;

#define LIBMSI_DB_OPEN_READONLY (const char *)0
#define LIBMSI_DB_OPEN_TRANSACT (const char *)1
#define LIBMSI_DB_OPEN_DIRECT   (const char *)2
#define LIBMSI_DB_OPEN_CREATE   (const char *)3
#define LIBMSI_DB_OPEN_CREATEDIRECT (const char *)4

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
unsigned libmsi_query_fetch(LibmsiQuery *,LibmsiRecord **);
unsigned libmsi_query_execute(LibmsiQuery *,LibmsiRecord *);
unsigned libmsi_query_close(LibmsiQuery *);
unsigned libmsi_database_open_query(LibmsiDatabase *,const char *,LibmsiQuery **);
LibmsiDBError libmsi_query_get_error(LibmsiQuery *,char *,unsigned *);

LibmsiDBState libmsi_database_get_state(LibmsiDatabase *);

/* record manipulation */
LibmsiRecord * libmsi_record_create(unsigned);
unsigned libmsi_record_clear_data(LibmsiRecord *);
unsigned libmsi_record_set_int(LibmsiRecord *,unsigned,int);
unsigned libmsi_record_set_string(LibmsiRecord *,unsigned,const char *);
unsigned libmsi_record_get_string(const LibmsiRecord *,unsigned,char *,unsigned *);
unsigned libmsi_record_get_field_count(const LibmsiRecord *);
int libmsi_record_get_integer(const LibmsiRecord *,unsigned);
unsigned libmsi_record_get_field_size(const LibmsiRecord *,unsigned);
bool libmsi_record_is_null(const LibmsiRecord *,unsigned);

unsigned libmsi_record_load_stream(LibmsiRecord *,unsigned,const char *);
unsigned libmsi_record_save_stream(LibmsiRecord *,unsigned,char*,unsigned *);

unsigned libmsi_database_get_primary_keys(LibmsiDatabase *,const char *,LibmsiRecord **);

/* database transforms */
unsigned libmsi_database_apply_transform(LibmsiDatabase *,const char *,int);

unsigned libmsi_query_get_column_info(LibmsiQuery *, LibmsiColInfo, LibmsiRecord **);

unsigned libmsi_summary_info_get_property(LibmsiSummaryInfo *,unsigned,unsigned *,int *,uint64_t*,char *,unsigned *);

unsigned libmsi_summary_info_set_property(LibmsiSummaryInfo *, unsigned, unsigned, int, uint64_t*, const char *);

unsigned libmsi_database_export(LibmsiDatabase *, const char *, int fd);

unsigned libmsi_database_import(LibmsiDatabase *, const char *, const char *);

unsigned libmsi_database_open(const char *, const char *, LibmsiDatabase **);

LibmsiCondition libmsi_database_is_table_persistent(LibmsiDatabase *, const char *);

unsigned libmsi_summary_info_persist(LibmsiSummaryInfo *);
unsigned libmsi_summary_info_get_property_count(LibmsiSummaryInfo *,unsigned *);

unsigned libmsi_query_modify(LibmsiQuery *, LibmsiModify, LibmsiRecord *);

unsigned libmsi_database_merge(LibmsiDatabase *, LibmsiDatabase *, const char *);

/* Non Unicode */
unsigned libmsi_database_get_summary_info(LibmsiDatabase *, unsigned, LibmsiSummaryInfo **);
unsigned libmsi_database_commit(LibmsiDatabase *);
unsigned libmsi_unref(void *);

#ifdef __cplusplus
}
#endif

#endif /* _LIBMSI_H */
