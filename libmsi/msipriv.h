/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002-2005 Mike McCormack for CodeWeavers
 * Copyright 2005 Aric Stewart for CodeWeavers
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

#ifndef __WINE_MSI_PRIVATE__
#define __WINE_MSI_PRIVATE__

#include <stdarg.h>

#include "unicode.h"
#include "windef.h"
#include "winbase.h"
#include "libmsi.h"
#include "objbase.h"
#include "objidl.h"
#include "winnls.h"
#include "list.h"

#pragma GCC visibility push(hidden)

#define MSI_DATASIZEMASK 0x00ff
#define MSITYPE_VALID    0x0100
#define MSITYPE_LOCALIZABLE 0x200
#define MSITYPE_STRING   0x0800
#define MSITYPE_NULLABLE 0x1000
#define MSITYPE_KEY      0x2000
#define MSITYPE_TEMPORARY 0x4000
#define MSITYPE_UNKNOWN   0x8000

#define MAX_STREAM_NAME_LEN     62
#define LONG_STR_BYTES  3

#define MSITYPE_IS_BINARY(type) (((type) & ~MSITYPE_NULLABLE) == (MSITYPE_STRING|MSITYPE_VALID))

struct tagMSITABLE;
typedef struct tagMSITABLE MSITABLE;

struct string_table;
typedef struct string_table string_table;

struct tagMSIOBJECTHDR;
typedef struct tagMSIOBJECTHDR MSIOBJECTHDR;

typedef VOID (*msihandledestructor)( MSIOBJECTHDR * );

struct tagMSIOBJECTHDR
{
    UINT magic;
    UINT type;
    LONG refcount;
    msihandledestructor destructor;
};

#define MSI_INITIAL_MEDIA_TRANSFORM_OFFSET 10000
#define MSI_INITIAL_MEDIA_TRANSFORM_DISKID 30000

typedef struct tagMSIDATABASE
{
    MSIOBJECTHDR hdr;
    IStorage *storage;
    string_table *strings;
    UINT bytes_per_strref;
    LPWSTR path;
    LPWSTR deletefile;
    LPCWSTR mode;
    UINT media_transform_offset;
    UINT media_transform_disk_id;
    struct list tables;
    struct list transforms;
    struct list streams;
} MSIDATABASE;

typedef struct tagMSIVIEW MSIVIEW;

typedef struct tagMSIQUERY
{
    MSIOBJECTHDR hdr;
    MSIVIEW *view;
    UINT row;
    MSIDATABASE *db;
    struct list mem;
} MSIQUERY;

/* maybe we can use a Variant instead of doing it ourselves? */
typedef struct tagMSIFIELD
{
    UINT type;
    union
    {
        INT iVal;
        INT_PTR pVal;
        LPWSTR szwVal;
        IStream *stream;
    } u;
} MSIFIELD;

typedef struct tagMSIRECORD
{
    MSIOBJECTHDR hdr;
    UINT count;       /* as passed to MsiCreateRecord */
    MSIFIELD fields[1]; /* nb. array size is count+1 */
} MSIRECORD;

typedef struct _column_info
{
    LPCWSTR table;
    LPCWSTR column;
    INT   type;
    BOOL   temporary;
    struct expr *val;
    struct _column_info *next;
} column_info;

typedef const struct tagMSICOLUMNHASHENTRY *MSIITERHANDLE;

typedef struct tagMSIVIEWOPS
{
    /*
     * fetch_int - reads one integer from {row,col} in the table
     *
     *  This function should be called after the execute method.
     *  Data returned by the function should not change until
     *   close or delete is called.
     *  To get a string value, query the database's string table with
     *   the integer value returned from this function.
     */
    UINT (*fetch_int)( struct tagMSIVIEW *view, UINT row, UINT col, UINT *val );

    /*
     * fetch_stream - gets a stream from {row,col} in the table
     *
     *  This function is similar to fetch_int, except fetches a
     *    stream instead of an integer.
     */
    UINT (*fetch_stream)( struct tagMSIVIEW *view, UINT row, UINT col, IStream **stm );

    /*
     * get_row - gets values from a row
     *
     */
    UINT (*get_row)( struct tagMSIVIEW *view, UINT row, MSIRECORD **rec );

    /*
     * set_row - sets values in a row as specified by mask
     *
     *  Similar semantics to fetch_int
     */
    UINT (*set_row)( struct tagMSIVIEW *view, UINT row, MSIRECORD *rec, UINT mask );

    /*
     * Inserts a new row into the database from the records contents
     */
    UINT (*insert_row)( struct tagMSIVIEW *view, MSIRECORD *record, UINT row, BOOL temporary );

    /*
     * Deletes a row from the database
     */
    UINT (*delete_row)( struct tagMSIVIEW *view, UINT row );

    /*
     * execute - loads the underlying data into memory so it can be read
     */
    UINT (*execute)( struct tagMSIVIEW *view, MSIRECORD *record );

    /*
     * close - clears the data read by execute from memory
     */
    UINT (*close)( struct tagMSIVIEW *view );

    /*
     * get_dimensions - returns the number of rows or columns in a table.
     *
     *  The number of rows can only be queried after the execute method
     *   is called. The number of columns can be queried at any time.
     */
    UINT (*get_dimensions)( struct tagMSIVIEW *view, UINT *rows, UINT *cols );

    /*
     * get_column_info - returns the name and type of a specific column
     *
     *  The column information can be queried at any time.
     */
    UINT (*get_column_info)( struct tagMSIVIEW *view, UINT n, LPCWSTR *name, UINT *type,
                             BOOL *temporary, LPCWSTR *table_name );

    /*
     * modify - not yet implemented properly
     */
    UINT (*modify)( struct tagMSIVIEW *view, MSIMODIFY eModifyMode, MSIRECORD *record, UINT row );

    /*
     * delete - destroys the structure completely
     */
    UINT (*delete)( struct tagMSIVIEW * );

    /*
     * find_matching_rows - iterates through rows that match a value
     *
     * If the column type is a string then a string ID should be passed in.
     *  If the value to be looked up is an integer then no transformation of
     *  the input value is required, except if the column is a string, in which
     *  case a string ID should be passed in.
     * The handle is an input/output parameter that keeps track of the current
     *  position in the iteration. It must be initialised to zero before the
     *  first call and continued to be passed in to subsequent calls.
     */
    UINT (*find_matching_rows)( struct tagMSIVIEW *view, UINT col, UINT val, UINT *row, MSIITERHANDLE *handle );

    /*
     * add_ref - increases the reference count of the table
     */
    UINT (*add_ref)( struct tagMSIVIEW *view );

    /*
     * release - decreases the reference count of the table
     */
    UINT (*release)( struct tagMSIVIEW *view );

    /*
     * add_column - adds a column to the table
     */
    UINT (*add_column)( struct tagMSIVIEW *view, LPCWSTR table, UINT number, LPCWSTR column, UINT type, BOOL hold );

    /*
     * remove_column - removes the column represented by table name and column number from the table
     */
    UINT (*remove_column)( struct tagMSIVIEW *view, LPCWSTR table, UINT number );

    /*
     * sort - orders the table by columns
     */
    UINT (*sort)( struct tagMSIVIEW *view, column_info *columns );

    /*
     * drop - drops the table from the database
     */
    UINT (*drop)( struct tagMSIVIEW *view );
} MSIVIEWOPS;

struct tagMSIVIEW
{
    MSIOBJECTHDR hdr;
    const MSIVIEWOPS *ops;
    MSIDBERROR error;
    const WCHAR *error_column;
};

#define MSI_MAX_PROPS 20

typedef struct tagMSISUMMARYINFO
{
    MSIOBJECTHDR hdr;
    IStorage *storage;
    DWORD update_count;
    PROPVARIANT property[MSI_MAX_PROPS];
} MSISUMMARYINFO;

#define MSIHANDLETYPE_ANY 0
#define MSIHANDLETYPE_DATABASE 1
#define MSIHANDLETYPE_SUMMARYINFO 2
#define MSIHANDLETYPE_VIEW 3
#define MSIHANDLETYPE_RECORD 4
#define MSIHANDLETYPE_PACKAGE 5
#define MSIHANDLETYPE_PREVIEW 6

#define MSIHANDLE_MAGIC 0x4d434923

/* handle unicode/ascii output in the Msi* API functions */
typedef struct {
    BOOL unicode;
    union {
       LPSTR a;
       LPWSTR w;
    } str;
} awstring;

typedef struct {
    BOOL unicode;
    union {
       LPCSTR a;
       LPCWSTR w;
    } str;
} awcstring;

UINT msi_strcpy_to_awstring( LPCWSTR str, awstring *awbuf, DWORD *sz );

/* handle functions */
extern void *msihandle2msiinfo(MSIHANDLE handle, UINT type);
extern MSIHANDLE alloc_msihandle( MSIOBJECTHDR * );
extern MSIHANDLE alloc_msi_remote_handle( IUnknown *unk );
extern void *alloc_msiobject(UINT type, UINT size, msihandledestructor destroy );
extern void msiobj_addref(MSIOBJECTHDR *);
extern int msiobj_release(MSIOBJECTHDR *);
extern void msiobj_lock(MSIOBJECTHDR *);
extern void msiobj_unlock(MSIOBJECTHDR *);
extern void msi_free_handle_table(void);

extern void free_cached_tables( MSIDATABASE *db );
extern UINT MSI_CommitTables( MSIDATABASE *db );


/* string table functions */
enum StringPersistence
{
    StringPersistent = 0,
    StringNonPersistent = 1
};

extern BOOL msi_addstringW( string_table *st, const WCHAR *data, int len, USHORT refcount, enum StringPersistence persistence );
extern UINT msi_string2idW( const string_table *st, LPCWSTR buffer, UINT *id );
extern VOID msi_destroy_stringtable( string_table *st );
extern const WCHAR *msi_string_lookup_id( const string_table *st, UINT id );
extern HRESULT msi_init_string_table( IStorage *stg );
extern string_table *msi_load_string_table( IStorage *stg, UINT *bytes_per_strref );
extern UINT msi_save_string_table( const string_table *st, IStorage *storage, UINT *bytes_per_strref );
extern UINT msi_get_string_table_codepage( const string_table *st );
extern UINT msi_set_string_table_codepage( string_table *st, UINT codepage );

extern BOOL TABLE_Exists( MSIDATABASE *db, LPCWSTR name );
extern MSICONDITION MSI_DatabaseIsTablePersistent( MSIDATABASE *db, LPCWSTR table );

extern UINT read_stream_data( IStorage *stg, LPCWSTR stname, BOOL table,
                              BYTE **pdata, UINT *psz );
extern UINT write_stream_data( IStorage *stg, LPCWSTR stname,
                               LPCVOID data, UINT sz, BOOL bTable );

/* transform functions */
extern UINT msi_table_apply_transform( MSIDATABASE *db, IStorage *stg );
extern UINT MSI_DatabaseApplyTransformW( MSIDATABASE *db,
                 LPCWSTR szTransformFile, int iErrorCond );
extern void append_storage_to_db( MSIDATABASE *db, IStorage *stg );

/* record internals */
extern void MSI_CloseRecord( MSIOBJECTHDR * );
extern UINT MSI_RecordSetIStream( MSIRECORD *, UINT, IStream *);
extern UINT MSI_RecordGetIStream( MSIRECORD *, UINT, IStream **);
extern const WCHAR *MSI_RecordGetString( const MSIRECORD *, UINT );
extern MSIRECORD *MSI_CreateRecord( UINT );
extern UINT MSI_RecordSetInteger( MSIRECORD *, UINT, int );
extern UINT MSI_RecordSetIntPtr( MSIRECORD *, UINT, INT_PTR );
extern UINT MSI_RecordSetStringW( MSIRECORD *, UINT, LPCWSTR );
extern BOOL MSI_RecordIsNull( MSIRECORD *, UINT );
extern UINT MSI_RecordGetStringW( MSIRECORD * , UINT, LPWSTR, LPDWORD);
extern UINT MSI_RecordGetStringA( MSIRECORD *, UINT, LPSTR, LPDWORD);
extern int MSI_RecordGetInteger( MSIRECORD *, UINT );
extern INT_PTR MSI_RecordGetIntPtr( MSIRECORD *, UINT );
extern UINT MSI_RecordReadStream( MSIRECORD *, UINT, char *, LPDWORD);
extern UINT MSI_RecordSetStream(MSIRECORD *, UINT, IStream *);
extern UINT MSI_RecordGetFieldCount( const MSIRECORD *rec );
extern UINT MSI_RecordStreamToFile( MSIRECORD *, UINT, LPCWSTR );
extern UINT MSI_RecordSetStreamFromFileW( MSIRECORD *, UINT, LPCWSTR );
extern UINT MSI_RecordCopyField( MSIRECORD *, UINT, MSIRECORD *, UINT );
extern MSIRECORD *MSI_CloneRecord( MSIRECORD * );
extern BOOL MSI_RecordsAreEqual( MSIRECORD *, MSIRECORD * );
extern BOOL MSI_RecordsAreFieldsEqual(MSIRECORD *a, MSIRECORD *b, UINT field);

/* stream internals */
extern void enum_stream_names( IStorage *stg );
extern LPWSTR encode_streamname(BOOL bTable, LPCWSTR in);
extern BOOL decode_streamname(LPCWSTR in, LPWSTR out);

/* database internals */
extern UINT msi_get_raw_stream( MSIDATABASE *, LPCWSTR, IStream ** );
extern UINT msi_clone_open_stream( MSIDATABASE *, IStorage *, const WCHAR *, IStream ** );
void msi_destroy_stream( MSIDATABASE *, const WCHAR * );
extern UINT MSI_OpenDatabaseW( LPCWSTR, LPCWSTR, MSIDATABASE ** );
extern UINT MSI_DatabaseOpenViewW(MSIDATABASE *, LPCWSTR, MSIQUERY ** );
extern UINT MSI_OpenQuery( MSIDATABASE *, MSIQUERY **, LPCWSTR, ... );
typedef UINT (*record_func)( MSIRECORD *, LPVOID );
extern UINT MSI_IterateRecords( MSIQUERY *, LPDWORD, record_func, LPVOID );
extern MSIRECORD *MSI_QueryGetRecord( MSIDATABASE *db, LPCWSTR query, ... );
extern UINT MSI_DatabaseGetPrimaryKeys( MSIDATABASE *, LPCWSTR, MSIRECORD ** );

/* view internals */
extern UINT MSI_ViewExecute( MSIQUERY*, MSIRECORD * );
extern UINT MSI_ViewFetch( MSIQUERY*, MSIRECORD ** );
extern UINT MSI_ViewClose( MSIQUERY* );
extern UINT MSI_ViewGetColumnInfo(MSIQUERY *, MSICOLINFO, MSIRECORD **);
extern UINT MSI_ViewModify( MSIQUERY *, MSIMODIFY, MSIRECORD * );
extern UINT VIEW_find_column( MSIVIEW *, LPCWSTR, LPCWSTR, UINT * );
extern UINT msi_view_get_row(MSIDATABASE *, MSIVIEW *, UINT, MSIRECORD **);

/* summary information */
extern MSISUMMARYINFO *MSI_GetSummaryInformationW( IStorage *stg, UINT uiUpdateCount );
extern LPWSTR msi_suminfo_dup_string( MSISUMMARYINFO *si, UINT uiProperty );
extern INT msi_suminfo_get_int32( MSISUMMARYINFO *si, UINT uiProperty );
extern LPWSTR msi_get_suminfo_product( IStorage *stg );
extern UINT msi_add_suminfo( MSIDATABASE *db, LPWSTR **records, int num_records, int num_columns );

/* Helpers */
extern WCHAR *msi_dup_record_field(MSIRECORD *row, INT index);
extern LPWSTR msi_dup_property( MSIDATABASE *db, LPCWSTR prop );
extern UINT msi_set_property( MSIDATABASE *, LPCWSTR, LPCWSTR );
extern UINT msi_get_property( MSIDATABASE *, LPCWSTR, LPWSTR, LPDWORD );
extern int msi_get_property_int( MSIDATABASE *package, LPCWSTR prop, int def );

/* common strings */
static const WCHAR szSourceDir[] = {'S','o','u','r','c','e','D','i','r',0};
static const WCHAR szSOURCEDIR[] = {'S','O','U','R','C','E','D','I','R',0};
static const WCHAR szRootDrive[] = {'R','O','O','T','D','R','I','V','E',0};
static const WCHAR szTargetDir[] = {'T','A','R','G','E','T','D','I','R',0};
static const WCHAR szLocalSid[] = {'S','-','1','-','5','-','1','8',0};
static const WCHAR szAllSid[] = {'S','-','1','-','1','-','0',0};
static const WCHAR szEmpty[] = {0};
static const WCHAR szAll[] = {'A','L','L',0};
static const WCHAR szOne[] = {'1',0};
static const WCHAR szZero[] = {'0',0};
static const WCHAR szSpace[] = {' ',0};
static const WCHAR szBackSlash[] = {'\\',0};
static const WCHAR szForwardSlash[] = {'/',0};
static const WCHAR szDot[] = {'.',0};
static const WCHAR szDotDot[] = {'.','.',0};
static const WCHAR szSemiColon[] = {';',0};
static const WCHAR szPreselected[] = {'P','r','e','s','e','l','e','c','t','e','d',0};
static const WCHAR szPatches[] = {'P','a','t','c','h','e','s',0};
static const WCHAR szState[] = {'S','t','a','t','e',0};
static const WCHAR szMsi[] = {'m','s','i',0};
static const WCHAR szPatch[] = {'P','A','T','C','H',0};
static const WCHAR szSourceList[] = {'S','o','u','r','c','e','L','i','s','t',0};
static const WCHAR szInstalled[] = {'I','n','s','t','a','l','l','e','d',0};
static const WCHAR szReinstall[] = {'R','E','I','N','S','T','A','L','L',0};
static const WCHAR szReinstallMode[] = {'R','E','I','N','S','T','A','L','L','M','O','D','E',0};
static const WCHAR szRemove[] = {'R','E','M','O','V','E',0};
static const WCHAR szUserSID[] = {'U','s','e','r','S','I','D',0};
static const WCHAR szProductCode[] = {'P','r','o','d','u','c','t','C','o','d','e',0};
static const WCHAR szRegisterClassInfo[] = {'R','e','g','i','s','t','e','r','C','l','a','s','s','I','n','f','o',0};
static const WCHAR szRegisterProgIdInfo[] = {'R','e','g','i','s','t','e','r','P','r','o','g','I','d','I','n','f','o',0};
static const WCHAR szRegisterExtensionInfo[] = {'R','e','g','i','s','t','e','r','E','x','t','e','n','s','i','o','n','I','n','f','o',0};
static const WCHAR szRegisterMIMEInfo[] = {'R','e','g','i','s','t','e','r','M','I','M','E','I','n','f','o',0};
static const WCHAR szDuplicateFiles[] = {'D','u','p','l','i','c','a','t','e','F','i','l','e','s',0};
static const WCHAR szRemoveDuplicateFiles[] = {'R','e','m','o','v','e','D','u','p','l','i','c','a','t','e','F','i','l','e','s',0};
static const WCHAR szInstallFiles[] = {'I','n','s','t','a','l','l','F','i','l','e','s',0};
static const WCHAR szPatchFiles[] = {'P','a','t','c','h','F','i','l','e','s',0};
static const WCHAR szRemoveFiles[] = {'R','e','m','o','v','e','F','i','l','e','s',0};
static const WCHAR szFindRelatedProducts[] = {'F','i','n','d','R','e','l','a','t','e','d','P','r','o','d','u','c','t','s',0};
static const WCHAR szAllUsers[] = {'A','L','L','U','S','E','R','S',0};
static const WCHAR szCustomActionData[] = {'C','u','s','t','o','m','A','c','t','i','o','n','D','a','t','a',0};
static const WCHAR szUILevel[] = {'U','I','L','e','v','e','l',0};
static const WCHAR szProductID[] = {'P','r','o','d','u','c','t','I','D',0};
static const WCHAR szPIDTemplate[] = {'P','I','D','T','e','m','p','l','a','t','e',0};
static const WCHAR szPIDKEY[] = {'P','I','D','K','E','Y',0};
static const WCHAR szTYPELIB[] = {'T','Y','P','E','L','I','B',0};
static const WCHAR szSumInfo[] = {5 ,'S','u','m','m','a','r','y','I','n','f','o','r','m','a','t','i','o','n',0};
static const WCHAR szHCR[] = {'H','K','E','Y','_','C','L','A','S','S','E','S','_','R','O','O','T','\\',0};
static const WCHAR szHCU[] = {'H','K','E','Y','_','C','U','R','R','E','N','T','_','U','S','E','R','\\',0};
static const WCHAR szHLM[] = {'H','K','E','Y','_','L','O','C','A','L','_','M','A','C','H','I','N','E','\\',0};
static const WCHAR szHU[] = {'H','K','E','Y','_','U','S','E','R','S','\\',0};
static const WCHAR szWindowsFolder[] = {'W','i','n','d','o','w','s','F','o','l','d','e','r',0};
static const WCHAR szAppSearch[] = {'A','p','p','S','e','a','r','c','h',0};
static const WCHAR szMoveFiles[] = {'M','o','v','e','F','i','l','e','s',0};
static const WCHAR szCCPSearch[] = {'C','C','P','S','e','a','r','c','h',0};
static const WCHAR szUnregisterClassInfo[] = {'U','n','r','e','g','i','s','t','e','r','C','l','a','s','s','I','n','f','o',0};
static const WCHAR szUnregisterExtensionInfo[] = {'U','n','r','e','g','i','s','t','e','r','E','x','t','e','n','s','i','o','n','I','n','f','o',0};
static const WCHAR szUnregisterMIMEInfo[] = {'U','n','r','e','g','i','s','t','e','r','M','I','M','E','I','n','f','o',0};
static const WCHAR szUnregisterProgIdInfo[] = {'U','n','r','e','g','i','s','t','e','r','P','r','o','g','I','d','I','n','f','o',0};
static const WCHAR szRegisterFonts[] = {'R','e','g','i','s','t','e','r','F','o','n','t','s',0};
static const WCHAR szUnregisterFonts[] = {'U','n','r','e','g','i','s','t','e','r','F','o','n','t','s',0};
static const WCHAR szCLSID[] = {'C','L','S','I','D',0};
static const WCHAR szProgID[] = {'P','r','o','g','I','D',0};
static const WCHAR szVIProgID[] = {'V','e','r','s','i','o','n','I','n','d','e','p','e','n','d','e','n','t','P','r','o','g','I','D',0};
static const WCHAR szAppID[] = {'A','p','p','I','D',0};
static const WCHAR szDefaultIcon[] = {'D','e','f','a','u','l','t','I','c','o','n',0};
static const WCHAR szInprocHandler[] = {'I','n','p','r','o','c','H','a','n','d','l','e','r',0};
static const WCHAR szInprocHandler32[] = {'I','n','p','r','o','c','H','a','n','d','l','e','r','3','2',0};
static const WCHAR szMIMEDatabase[] = {'M','I','M','E','\\','D','a','t','a','b','a','s','e','\\','C','o','n','t','e','n','t',' ','T','y','p','e','\\',0};
static const WCHAR szLocalPackage[] = {'L','o','c','a','l','P','a','c','k','a','g','e',0};
static const WCHAR szOriginalDatabase[] = {'O','r','i','g','i','n','a','l','D','a','t','a','b','a','s','e',0};
static const WCHAR szUpgradeCode[] = {'U','p','g','r','a','d','e','C','o','d','e',0};
static const WCHAR szAdminUser[] = {'A','d','m','i','n','U','s','e','r',0};
static const WCHAR szIntel[] = {'I','n','t','e','l',0};
static const WCHAR szIntel64[] = {'I','n','t','e','l','6','4',0};
static const WCHAR szX64[] = {'x','6','4',0};
static const WCHAR szAMD64[] = {'A','M','D','6','4',0};
static const WCHAR szARM[] = {'A','r','m',0};
static const WCHAR szWow6432NodeCLSID[] = {'W','o','w','6','4','3','2','N','o','d','e','\\','C','L','S','I','D',0};
static const WCHAR szWow6432Node[] = {'W','o','w','6','4','3','2','N','o','d','e',0};
static const WCHAR szStreams[] = {'_','S','t','r','e','a','m','s',0};
static const WCHAR szStorages[] = {'_','S','t','o','r','a','g','e','s',0};
static const WCHAR szMsiPublishAssemblies[] = {'M','s','i','P','u','b','l','i','s','h','A','s','s','e','m','b','l','i','e','s',0};
static const WCHAR szCostingComplete[] = {'C','o','s','t','i','n','g','C','o','m','p','l','e','t','e',0};
static const WCHAR szTempFolder[] = {'T','e','m','p','F','o','l','d','e','r',0};
static const WCHAR szDatabase[] = {'D','A','T','A','B','A','S','E',0};
static const WCHAR szCRoot[] = {'C',':','\\',0};
static const WCHAR szProductLanguage[] = {'P','r','o','d','u','c','t','L','a','n','g','u','a','g','e',0};
static const WCHAR szProductVersion[] = {'P','r','o','d','u','c','t','V','e','r','s','i','o','n',0};
static const WCHAR szWindowsInstaller[] = {'W','i','n','d','o','w','s','I','n','s','t','a','l','l','e','r',0};
static const WCHAR szStringData[] = {'_','S','t','r','i','n','g','D','a','t','a',0};
static const WCHAR szStringPool[] = {'_','S','t','r','i','n','g','P','o','o','l',0};
static const WCHAR szInstallLevel[] = {'I','N','S','T','A','L','L','L','E','V','E','L',0};
static const WCHAR szCostInitialize[] = {'C','o','s','t','I','n','i','t','i','a','l','i','z','e',0};
static const WCHAR szAppDataFolder[] = {'A','p','p','D','a','t','a','F','o','l','d','e','r',0};
static const WCHAR szRollbackDisabled[] = {'R','o','l','l','b','a','c','k','D','i','s','a','b','l','e','d',0};
static const WCHAR szName[] = {'N','a','m','e',0};
static const WCHAR szData[] = {'D','a','t','a',0};
static const WCHAR szLangResource[] = {'\\','V','a','r','F','i','l','e','I','n','f','o','\\','T','r','a','n','s','l','a','t','i','o','n',0};
static const WCHAR szInstallLocation[] = {'I','n','s','t','a','l','l','L','o','c','a','t','i','o','n',0};

/* memory allocation macro functions */

#if defined(__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 3)))
#define __WINE_ALLOC_SIZE(x) __attribute__((__alloc_size__(x)))
#else
#define __WINE_ALLOC_SIZE(x)
#endif

static void *msi_alloc( size_t len ) __WINE_ALLOC_SIZE(1);
static inline void *msi_alloc( size_t len )
{
    return HeapAlloc( GetProcessHeap(), 0, len );
}

static void *msi_alloc_zero( size_t len ) __WINE_ALLOC_SIZE(1);
static inline void *msi_alloc_zero( size_t len )
{
    return HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, len );
}

static void *msi_realloc( void *mem, size_t len ) __WINE_ALLOC_SIZE(2);
static inline void *msi_realloc( void *mem, size_t len )
{
    return HeapReAlloc( GetProcessHeap(), 0, mem, len );
}

static void *msi_realloc_zero( void *mem, size_t len ) __WINE_ALLOC_SIZE(2);
static inline void *msi_realloc_zero( void *mem, size_t len )
{
    return HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, mem, len );
}

static inline BOOL msi_free( void *mem )
{
    return HeapFree( GetProcessHeap(), 0, mem );
}

static inline char *strdupWtoA( LPCWSTR str )
{
    LPSTR ret = NULL;
    DWORD len;

    if (!str) return ret;
    len = WideCharToMultiByte( CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
    ret = msi_alloc( len );
    if (ret)
        WideCharToMultiByte( CP_ACP, 0, str, -1, ret, len, NULL, NULL );
    return ret;
}

static inline LPWSTR strdupAtoW( LPCSTR str )
{
    LPWSTR ret = NULL;
    DWORD len;

    if (!str) return ret;
    len = MultiByteToWideChar( CP_ACP, 0, str, -1, NULL, 0 );
    ret = msi_alloc( len * sizeof(WCHAR) );
    if (ret)
        MultiByteToWideChar( CP_ACP, 0, str, -1, ret, len );
    return ret;
}

static inline LPWSTR strdupW( LPCWSTR src )
{
    LPWSTR dest;
    if (!src) return NULL;
    dest = msi_alloc( (lstrlenW(src)+1)*sizeof(WCHAR) );
    if (dest)
        lstrcpyW(dest, src);
    return dest;
}

#pragma GCC visibility pop

#endif /* __WINE_MSI_PRIVATE__ */
