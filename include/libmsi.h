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

struct tagMSIOBJECT;
typedef struct tagMSIOBJECT MSIOBJECT;

typedef enum tagMSICONDITION
{
    MSICONDITION_FALSE = 0,
    MSICONDITION_TRUE  = 1,
    MSICONDITION_NONE  = 2,
    MSICONDITION_ERROR = 3,
} MSICONDITION;

#define MSI_NULL_INTEGER 0x80000000

typedef enum tagMSICOLINFO
{
    MSICOLINFO_NAMES = 0,
    MSICOLINFO_TYPES = 1
} MSICOLINFO;

typedef enum tagMSIMODIFY
{
    MSIMODIFY_SEEK = -1,
    MSIMODIFY_REFRESH = 0,
    MSIMODIFY_INSERT = 1,
    MSIMODIFY_UPDATE = 2,
    MSIMODIFY_ASSIGN = 3,
    MSIMODIFY_REPLACE = 4,
    MSIMODIFY_MERGE = 5,
    MSIMODIFY_DELETE = 6,
    MSIMODIFY_INSERT_TEMPORARY = 7,
    MSIMODIFY_VALIDATE = 8,
    MSIMODIFY_VALIDATE_NEW = 9,
    MSIMODIFY_VALIDATE_FIELD = 10,
    MSIMODIFY_VALIDATE_DELETE = 11
} MSIMODIFY;

#define MSIDBOPEN_READONLY (LPCTSTR)0
#define MSIDBOPEN_TRANSACT (LPCTSTR)1
#define MSIDBOPEN_DIRECT   (LPCTSTR)2
#define MSIDBOPEN_CREATE   (LPCTSTR)3
#define MSIDBOPEN_CREATEDIRECT (LPCTSTR)4

#define MSIDBOPEN_PATCHFILE 32 / sizeof(*MSIDBOPEN_READONLY)

typedef enum tagMSIDBERROR
{
    MSIDBERROR_INVALIDARG = -3,
    MSIDBERROR_MOREDATA = -2,
    MSIDBERROR_FUNCTIONERROR = -1,
    MSIDBERROR_NOERROR = 0,
    MSIDBERROR_DUPLICATEKEY = 1,
    MSIDBERROR_REQUIRED = 2,
    MSIDBERROR_BADLINK = 3,
    MSIDBERROR_OVERFLOW = 4,
    MSIDBERROR_UNDERFLOW = 5,
    MSIDBERROR_NOTINSET = 6,
    MSIDBERROR_BADVERSION = 7,
    MSIDBERROR_BADCASE = 8,
    MSIDBERROR_BADGUID = 9,
    MSIDBERROR_BADWILDCARD = 10,
    MSIDBERROR_BADIDENTIFIER = 11,
    MSIDBERROR_BADLANGUAGE = 12,
    MSIDBERROR_BADFILENAME = 13,
    MSIDBERROR_BADPATH = 14,
    MSIDBERROR_BADCONDITION = 15,
    MSIDBERROR_BADFORMATTED = 16,
    MSIDBERROR_BADTEMPLATE = 17,
    MSIDBERROR_BADDEFAULTDIR = 18,
    MSIDBERROR_BADREGPATH = 19,
    MSIDBERROR_BADCUSTOMSOURCE = 20,
    MSIDBERROR_BADPROPERTY = 21,
    MSIDBERROR_MISSINGDATA = 22,
    MSIDBERROR_BADCATEGORY = 23,
    MSIDBERROR_BADKEYTABLE = 24,
    MSIDBERROR_BADMAXMINVALUES = 25,
    MSIDBERROR_BADCABINET = 26,
    MSIDBERROR_BADSHORTCUT= 27,
    MSIDBERROR_STRINGOVERFLOW = 28,
    MSIDBERROR_BADLOCALIZEATTRIB = 29
} MSIDBERROR; 

typedef enum tagMSIDBSTATE
{
    MSIDBSTATE_ERROR = -1,
    MSIDBSTATE_READ = 0,
    MSIDBSTATE_WRITE = 1
} MSIDBSTATE;


#ifdef __cplusplus
extern "C" {
#endif

#ifndef WINELIB_NAME_AW
#ifdef UNICODE
#define WINELIB_NAME_AW(x) x##W
#else
#define WINELIB_NAME_AW(x) x##A
#endif
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
unsigned MsiViewFetch(MSIOBJECT *,MSIOBJECT **);
unsigned MsiViewExecute(MSIOBJECT *,MSIOBJECT *);
unsigned MsiViewClose(MSIOBJECT *);
unsigned MsiDatabaseOpenViewA(MSIOBJECT *,const CHAR *,MSIOBJECT **);
unsigned MsiDatabaseOpenViewW(MSIOBJECT *,const WCHAR *,MSIOBJECT **);
#define     MsiDatabaseOpenView WINELIB_NAME_AW(MsiDatabaseOpenView)
MSIDBERROR MsiViewGetErrorA(MSIOBJECT *,CHAR *,unsigned *);
MSIDBERROR MsiViewGetErrorW(MSIOBJECT *,WCHAR *,unsigned *);
#define     MsiViewGetError WINELIB_NAME_AW(MsiViewGetError)

MSIDBSTATE MsiGetDatabaseState(MSIOBJECT *);

/* record manipulation */
MSIOBJECT * MsiCreateRecord(unsigned);
unsigned MsiRecordClearData(MSIOBJECT *);
unsigned MsiRecordSetInteger(MSIOBJECT *,unsigned,int);
unsigned MsiRecordSetStringA(MSIOBJECT *,unsigned,const CHAR *);
unsigned MsiRecordSetStringW(MSIOBJECT *,unsigned,const WCHAR *);
#define     MsiRecordSetString WINELIB_NAME_AW(MsiRecordSetString)
unsigned MsiRecordGetStringA(MSIOBJECT *,unsigned,CHAR *,unsigned *);
unsigned MsiRecordGetStringW(MSIOBJECT *,unsigned,WCHAR *,unsigned *);
#define     MsiRecordGetString WINELIB_NAME_AW(MsiRecordGetString)
unsigned MsiRecordGetFieldCount(MSIOBJECT *);
int MsiRecordGetInteger(MSIOBJECT *,unsigned);
unsigned MsiRecordDataSize(MSIOBJECT *,unsigned);
BOOL MsiRecordIsNull(MSIOBJECT *,unsigned);
unsigned MsiFormatRecordA(MSIOBJECT *,MSIOBJECT *,CHAR *,unsigned *);
unsigned MsiFormatRecordW(MSIOBJECT *,MSIOBJECT *,WCHAR *,unsigned *);
#define     MsiFormatRecord WINELIB_NAME_AW(MsiFormatRecord)
unsigned MsiRecordSetStreamA(MSIOBJECT *,unsigned,const CHAR *);
unsigned MsiRecordSetStreamW(MSIOBJECT *,unsigned,const WCHAR *);
#define     MsiRecordSetStream WINELIB_NAME_AW(MsiRecordSetStream)
unsigned MsiRecordReadStream(MSIOBJECT *,unsigned,char*,unsigned *);

unsigned MsiDatabaseGetPrimaryKeysA(MSIOBJECT *,const CHAR *,MSIOBJECT **);
unsigned MsiDatabaseGetPrimaryKeysW(MSIOBJECT *,const WCHAR *,MSIOBJECT **);
#define     MsiDatabaseGetPrimaryKeys WINELIB_NAME_AW(MsiDatabaseGetPrimaryKeys)

/* database transforms */
unsigned MsiDatabaseApplyTransformA(MSIOBJECT *,const CHAR *,int);
unsigned MsiDatabaseApplyTransformW(MSIOBJECT *,const WCHAR *,int);
#define     MsiDatabaseApplyTransform WINELIB_NAME_AW(MsiDatabaseApplyTransform)

unsigned MsiViewGetColumnInfo(MSIOBJECT *, MSICOLINFO, MSIOBJECT **);

unsigned MsiCreateTransformSummaryInfoA(MSIOBJECT *, MSIOBJECT *, const CHAR *, int, int);
unsigned MsiCreateTransformSummaryInfoW(MSIOBJECT *, MSIOBJECT *, const WCHAR *, int, int);
#define     MsiCreateTransformSummaryInfo WINELIB_NAME_AW(MsiCreateTransformSummaryInfo)

unsigned MsiGetSummaryInformationA(MSIOBJECT *, const CHAR *, unsigned, MSIOBJECT **);
unsigned MsiGetSummaryInformationW(MSIOBJECT *, const WCHAR *, unsigned, MSIOBJECT **);
#define     MsiGetSummaryInformation WINELIB_NAME_AW(MsiGetSummaryInformation)

unsigned MsiSummaryInfoGetPropertyA(MSIOBJECT *,unsigned,unsigned *,int *,FILETIME*,CHAR *,unsigned *);
unsigned MsiSummaryInfoGetPropertyW(MSIOBJECT *,unsigned,unsigned *,int *,FILETIME*,WCHAR *,unsigned *);
#define     MsiSummaryInfoGetProperty WINELIB_NAME_AW(MsiSummaryInfoGetProperty)

unsigned MsiSummaryInfoSetPropertyA(MSIOBJECT *, unsigned, unsigned, int, FILETIME*, const CHAR *);
unsigned MsiSummaryInfoSetPropertyW(MSIOBJECT *, unsigned, unsigned, int, FILETIME*, const WCHAR *);
#define     MsiSummaryInfoSetProperty WINELIB_NAME_AW(MsiSummaryInfoSetProperty)

unsigned MsiDatabaseExportA(MSIOBJECT *, const CHAR *, const CHAR *, const CHAR *);
unsigned MsiDatabaseExportW(MSIOBJECT *, const WCHAR *, const WCHAR *, const WCHAR *);
#define     MsiDatabaseExport WINELIB_NAME_AW(MsiDatabaseExport)

unsigned MsiDatabaseImportA(MSIOBJECT *, const CHAR *, const CHAR *);
unsigned MsiDatabaseImportW(MSIOBJECT *, const WCHAR *, const WCHAR *);
#define     MsiDatabaseImport WINELIB_NAME_AW(MsiDatabaseImport)

unsigned MsiOpenDatabaseW(const WCHAR *, const WCHAR *, MSIOBJECT **);
unsigned MsiOpenDatabaseA(const CHAR *, const CHAR *, MSIOBJECT **);
#define     MsiOpenDatabase WINELIB_NAME_AW(MsiOpenDatabase)

MSICONDITION MsiDatabaseIsTablePersistentA(MSIOBJECT *, const CHAR *);
MSICONDITION MsiDatabaseIsTablePersistentW(MSIOBJECT *, const WCHAR *);
#define     MsiDatabaseIsTablePersistent WINELIB_NAME_AW(MsiDatabaseIsTablePersistent)

unsigned MsiSummaryInfoPersist(MSIOBJECT *);
unsigned MsiSummaryInfoGetPropertyCount(MSIOBJECT *,unsigned *);

unsigned MsiViewModify(MSIOBJECT *, MSIMODIFY, MSIOBJECT *);

unsigned MsiDatabaseMergeA(MSIOBJECT *, MSIOBJECT *, const CHAR *);
unsigned MsiDatabaseMergeW(MSIOBJECT *, MSIOBJECT *, const WCHAR *);
#define     MsiDatabaseMerge WINELIB_NAME_AW(MsiDatabaseMerge)

/* Non Unicode */
unsigned MsiDatabaseCommit(MSIOBJECT *);
unsigned MsiCloseHandle(MSIOBJECT *);

MSIOBJECT * MsiGetLastErrorRecord(void);

#ifdef __cplusplus
}
#endif

#endif /* _LIBMSI_H */
