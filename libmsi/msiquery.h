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

#ifndef _MSIQUERY_H
#define _MSIQUERY_H

typedef unsigned __LONG32 MSIHANDLE;

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

#define MSIDBOPEN_READONLY (LPCWSTR)0
#define MSIDBOPEN_TRANSACT (LPCWSTR)1
#define MSIDBOPEN_DIRECT   (LPCWSTR)2
#define MSIDBOPEN_CREATE   (LPCWSTR)3
#define MSIDBOPEN_CREATEDIRECT (LPCWSTR)4

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

#define PID_DICTIONARY (0)
#define PID_CODEPAGE (0x1)
#define PID_TITLE 2
#define PID_SUBJECT 3
#define PID_AUTHOR 4
#define PID_KEYWORDS 5
#define PID_COMMENTS 6
#define PID_TEMPLATE 7
#define PID_LASTAUTHOR 8
#define PID_REVNUMBER 9
#define PID_EDITTIME 10
#define PID_LASTPRINTED 11
#define PID_CREATE_DTM 12
#define PID_LASTSAVE_DTM 13
#define PID_PAGECOUNT 14
#define PID_WORDCOUNT 15
#define PID_CHARCOUNT 16
#define PID_THUMBNAIL 17
#define PID_APPNAME 18
#define PID_SECURITY 19

#define PID_MSIVERSION PID_PAGECOUNT
#define PID_MSISOURCE PID_WORDCOUNT
#define PID_MSIRESTRICT PID_CHARCOUNT

/* view manipulation */
UINT WINAPI MsiViewFetch(MSIHANDLE,MSIHANDLE*);
UINT WINAPI MsiViewExecute(MSIHANDLE,MSIHANDLE);
UINT WINAPI MsiViewClose(MSIHANDLE);
UINT WINAPI MsiDatabaseOpenViewA(MSIHANDLE,LPCSTR,MSIHANDLE*);
UINT WINAPI MsiDatabaseOpenViewW(MSIHANDLE,LPCWSTR,MSIHANDLE*);
#define     MsiDatabaseOpenView WINELIB_NAME_AW(MsiDatabaseOpenView)
MSIDBERROR WINAPI MsiViewGetErrorA(MSIHANDLE,LPSTR,LPDWORD);
MSIDBERROR WINAPI MsiViewGetErrorW(MSIHANDLE,LPWSTR,LPDWORD);
#define     MsiViewGetError WINELIB_NAME_AW(MsiViewGetError)

MSIDBSTATE WINAPI MsiGetDatabaseState(MSIHANDLE);

/* record manipulation */
MSIHANDLE WINAPI MsiCreateRecord(UINT);
UINT WINAPI MsiRecordClearData(MSIHANDLE);
UINT WINAPI MsiRecordSetInteger(MSIHANDLE,UINT,int);
UINT WINAPI MsiRecordSetStringA(MSIHANDLE,UINT,LPCSTR);
UINT WINAPI MsiRecordSetStringW(MSIHANDLE,UINT,LPCWSTR);
#define     MsiRecordSetString WINELIB_NAME_AW(MsiRecordSetString)
UINT WINAPI MsiRecordGetStringA(MSIHANDLE,UINT,LPSTR,LPDWORD);
UINT WINAPI MsiRecordGetStringW(MSIHANDLE,UINT,LPWSTR,LPDWORD);
#define     MsiRecordGetString WINELIB_NAME_AW(MsiRecordGetString)
UINT WINAPI MsiRecordGetFieldCount(MSIHANDLE);
int WINAPI MsiRecordGetInteger(MSIHANDLE,UINT);
UINT WINAPI MsiRecordDataSize(MSIHANDLE,UINT);
BOOL WINAPI MsiRecordIsNull(MSIHANDLE,UINT);
UINT WINAPI MsiFormatRecordA(MSIHANDLE,MSIHANDLE,LPSTR,LPDWORD);
UINT WINAPI MsiFormatRecordW(MSIHANDLE,MSIHANDLE,LPWSTR,LPDWORD);
#define     MsiFormatRecord WINELIB_NAME_AW(MsiFormatRecord)
UINT WINAPI MsiRecordSetStreamA(MSIHANDLE,UINT,LPCSTR);
UINT WINAPI MsiRecordSetStreamW(MSIHANDLE,UINT,LPCWSTR);
#define     MsiRecordSetStream WINELIB_NAME_AW(MsiRecordSetStream)
UINT WINAPI MsiRecordReadStream(MSIHANDLE,UINT,char*,LPDWORD);

UINT WINAPI MsiDatabaseGetPrimaryKeysA(MSIHANDLE,LPCSTR,MSIHANDLE*);
UINT WINAPI MsiDatabaseGetPrimaryKeysW(MSIHANDLE,LPCWSTR,MSIHANDLE*);
#define     MsiDatabaseGetPrimaryKeys WINELIB_NAME_AW(MsiDatabaseGetPrimaryKeys)

/* database transforms */
UINT WINAPI MsiDatabaseApplyTransformA(MSIHANDLE,LPCSTR,int);
UINT WINAPI MsiDatabaseApplyTransformW(MSIHANDLE,LPCWSTR,int);
#define     MsiDatabaseApplyTransform WINELIB_NAME_AW(MsiDatabaseApplyTransform)

UINT WINAPI MsiViewGetColumnInfo(MSIHANDLE, MSICOLINFO, MSIHANDLE*);

UINT WINAPI MsiCreateTransformSummaryInfoA(MSIHANDLE, MSIHANDLE, LPCSTR, int, int);
UINT WINAPI MsiCreateTransformSummaryInfoW(MSIHANDLE, MSIHANDLE, LPCWSTR, int, int);
#define     MsiCreateTransformSummaryInfo WINELIB_NAME_AW(MsiCreateTransformSummaryInfo)

UINT WINAPI MsiGetSummaryInformationA(MSIHANDLE, LPCSTR, UINT, MSIHANDLE *);
UINT WINAPI MsiGetSummaryInformationW(MSIHANDLE, LPCWSTR, UINT, MSIHANDLE *);
#define     MsiGetSummaryInformation WINELIB_NAME_AW(MsiGetSummaryInformation)

UINT WINAPI MsiSummaryInfoGetPropertyA(MSIHANDLE,UINT,PUINT,LPINT,FILETIME*,LPSTR,LPDWORD);
UINT WINAPI MsiSummaryInfoGetPropertyW(MSIHANDLE,UINT,PUINT,LPINT,FILETIME*,LPWSTR,LPDWORD);
#define     MsiSummaryInfoGetProperty WINELIB_NAME_AW(MsiSummaryInfoGetProperty)

UINT WINAPI MsiSummaryInfoSetPropertyA(MSIHANDLE, UINT, UINT, INT, FILETIME*, LPCSTR);
UINT WINAPI MsiSummaryInfoSetPropertyW(MSIHANDLE, UINT, UINT, INT, FILETIME*, LPCWSTR);
#define     MsiSummaryInfoSetProperty WINELIB_NAME_AW(MsiSummaryInfoSetProperty)

UINT WINAPI MsiDatabaseExportA(MSIHANDLE, LPCSTR, LPCSTR, LPCSTR);
UINT WINAPI MsiDatabaseExportW(MSIHANDLE, LPCWSTR, LPCWSTR, LPCWSTR);
#define     MsiDatabaseExport WINELIB_NAME_AW(MsiDatabaseExport)

UINT WINAPI MsiDatabaseImportA(MSIHANDLE, LPCSTR, LPCSTR);
UINT WINAPI MsiDatabaseImportW(MSIHANDLE, LPCWSTR, LPCWSTR);
#define     MsiDatabaseImport WINELIB_NAME_AW(MsiDatabaseImport)

UINT WINAPI MsiOpenDatabaseW(LPCWSTR, LPCWSTR, MSIHANDLE*);
UINT WINAPI MsiOpenDatabaseA(LPCSTR, LPCSTR, MSIHANDLE*);
#define     MsiOpenDatabase WINELIB_NAME_AW(MsiOpenDatabase)

MSICONDITION WINAPI MsiDatabaseIsTablePersistentA(MSIHANDLE, LPCSTR);
MSICONDITION WINAPI MsiDatabaseIsTablePersistentW(MSIHANDLE, LPCWSTR);
#define     MsiDatabaseIsTablePersistent WINELIB_NAME_AW(MsiDatabaseIsTablePersistent)

UINT WINAPI MsiSummaryInfoPersist(MSIHANDLE);
UINT WINAPI MsiSummaryInfoGetPropertyCount(MSIHANDLE,PUINT);

UINT WINAPI MsiViewModify(MSIHANDLE, MSIMODIFY, MSIHANDLE);

UINT WINAPI MsiDatabaseMergeA(MSIHANDLE, MSIHANDLE, LPCSTR);
UINT WINAPI MsiDatabaseMergeW(MSIHANDLE, MSIHANDLE, LPCWSTR);
#define     MsiDatabaseMerge WINELIB_NAME_AW(MsiDatabaseMerge)

/* Non Unicode */
UINT WINAPI MsiDatabaseCommit(MSIHANDLE);
UINT WINAPI MsiCloseHandle(MSIHANDLE);
UINT WINAPI MsiCloseAllHandles(void);

MSIHANDLE WINAPI MsiGetLastErrorRecord(void);

#ifdef __cplusplus
}
#endif

#endif /* _MSIQUERY_H */
