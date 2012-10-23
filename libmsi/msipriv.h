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

struct tagMSIOBJECT;
typedef struct tagMSIOBJECT MSIOBJECT;

typedef VOID (*msihandledestructor)( MSIOBJECT * );

struct tagMSIOBJECT
{
    unsigned magic;
    unsigned type;
    LONG refcount;
    msihandledestructor destructor;
};

#define MSI_INITIAL_MEDIA_TRANSFORM_OFFSET 10000
#define MSI_INITIAL_MEDIA_TRANSFORM_DISKID 30000

typedef struct tagMSIDATABASE
{
    MSIOBJECT hdr;
    IStorage *storage;
    string_table *strings;
    unsigned bytes_per_strref;
    WCHAR *path;
    WCHAR *deletefile;
    const WCHAR *mode;
    unsigned media_transform_offset;
    unsigned media_transform_disk_id;
    struct list tables;
    struct list transforms;
    struct list streams;
} MSIDATABASE;

typedef struct tagMSIVIEW MSIVIEW;

typedef struct tagMSIQUERY
{
    MSIOBJECT hdr;
    MSIVIEW *view;
    unsigned row;
    MSIDATABASE *db;
    struct list mem;
} MSIQUERY;

/* maybe we can use a Variant instead of doing it ourselves? */
typedef struct tagMSIFIELD
{
    unsigned type;
    union
    {
        int iVal;
        intptr_t pVal;
        WCHAR *szwVal;
        IStream *stream;
    } u;
} MSIFIELD;

typedef struct tagMSIRECORD
{
    MSIOBJECT hdr;
    unsigned count;       /* as passed to MsiCreateRecord */
    MSIFIELD fields[1]; /* nb. array size is count+1 */
} MSIRECORD;

typedef struct _column_info
{
    const WCHAR *table;
    const WCHAR *column;
    int   type;
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
    unsigned (*fetch_int)( MSIVIEW *view, unsigned row, unsigned col, unsigned *val );

    /*
     * fetch_stream - gets a stream from {row,col} in the table
     *
     *  This function is similar to fetch_int, except fetches a
     *    stream instead of an integer.
     */
    unsigned (*fetch_stream)( MSIVIEW *view, unsigned row, unsigned col, IStream **stm );

    /*
     * get_row - gets values from a row
     *
     */
    unsigned (*get_row)( MSIVIEW *view, unsigned row, MSIRECORD **rec );

    /*
     * set_row - sets values in a row as specified by mask
     *
     *  Similar semantics to fetch_int
     */
    unsigned (*set_row)( MSIVIEW *view, unsigned row, MSIRECORD *rec, unsigned mask );

    /*
     * Inserts a new row into the database from the records contents
     */
    unsigned (*insert_row)( MSIVIEW *view, MSIRECORD *record, unsigned row, BOOL temporary );

    /*
     * Deletes a row from the database
     */
    unsigned (*delete_row)( MSIVIEW *view, unsigned row );

    /*
     * execute - loads the underlying data into memory so it can be read
     */
    unsigned (*execute)( MSIVIEW *view, MSIRECORD *record );

    /*
     * close - clears the data read by execute from memory
     */
    unsigned (*close)( MSIVIEW *view );

    /*
     * get_dimensions - returns the number of rows or columns in a table.
     *
     *  The number of rows can only be queried after the execute method
     *   is called. The number of columns can be queried at any time.
     */
    unsigned (*get_dimensions)( MSIVIEW *view, unsigned *rows, unsigned *cols );

    /*
     * get_column_info - returns the name and type of a specific column
     *
     *  The column information can be queried at any time.
     */
    unsigned (*get_column_info)( MSIVIEW *view, unsigned n, const WCHAR **name, unsigned *type,
                             BOOL *temporary, const WCHAR **table_name );

    /*
     * modify - not yet implemented properly
     */
    unsigned (*modify)( MSIVIEW *view, MSIMODIFY eModifyMode, MSIRECORD *record, unsigned row );

    /*
     * delete - destroys the structure completely
     */
    unsigned (*delete)( MSIVIEW * );

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
    unsigned (*find_matching_rows)( MSIVIEW *view, unsigned col, unsigned val, unsigned *row, MSIITERHANDLE *handle );

    /*
     * add_ref - increases the reference count of the table
     */
    unsigned (*add_ref)( MSIVIEW *view );

    /*
     * release - decreases the reference count of the table
     */
    unsigned (*release)( MSIVIEW *view );

    /*
     * add_column - adds a column to the table
     */
    unsigned (*add_column)( MSIVIEW *view, const WCHAR *table, unsigned number, const WCHAR *column, unsigned type, BOOL hold );

    /*
     * remove_column - removes the column represented by table name and column number from the table
     */
    unsigned (*remove_column)( MSIVIEW *view, const WCHAR *table, unsigned number );

    /*
     * sort - orders the table by columns
     */
    unsigned (*sort)( MSIVIEW *view, column_info *columns );

    /*
     * drop - drops the table from the database
     */
    unsigned (*drop)( MSIVIEW *view );
} MSIVIEWOPS;

struct tagMSIVIEW
{
    MSIOBJECT hdr;
    const MSIVIEWOPS *ops;
    MSIDBERROR error;
    const WCHAR *error_column;
};

#define MSI_MAX_PROPS 20

typedef struct tagMSISUMMARYINFO
{
    MSIOBJECT hdr;
    IStorage *storage;
    unsigned update_count;
    PROPVARIANT property[MSI_MAX_PROPS];
} MSISUMMARYINFO;

#define MSIOBJECTTYPE_ANY 0
#define MSIOBJECTTYPE_DATABASE 1
#define MSIOBJECTTYPE_SUMMARYINFO 2
#define MSIOBJECTTYPE_VIEW 3
#define MSIOBJECTTYPE_RECORD 4
#define MSIOBJECTTYPE_PACKAGE 5
#define MSIOBJECTTYPE_PREVIEW 6

/* handle unicode/ascii output in the Msi* API functions */
typedef struct {
    BOOL unicode;
    union {
       char *a;
       WCHAR *w;
    } str;
} awstring;

typedef struct {
    BOOL unicode;
    union {
       const char *a;
       const WCHAR *w;
    } str;
} awcstring;

unsigned msi_strcpy_to_awstring( const WCHAR *str, awstring *awbuf, unsigned *sz );

/* handle functions */
extern void *msihandle2msiinfo(MSIOBJECT *handle, unsigned type);
extern MSIOBJECT *alloc_msihandle( MSIOBJECT * );
extern MSIOBJECT *alloc_msi_remote_handle( IUnknown *unk );
extern void *alloc_msiobject(unsigned type, unsigned size, msihandledestructor destroy );
extern void msiobj_addref(MSIOBJECT *);
extern int msiobj_release(MSIOBJECT *);
extern void msiobj_lock(MSIOBJECT *);
extern void msiobj_unlock(MSIOBJECT *);
extern void msi_free_handle_table(void);

extern void free_cached_tables( MSIDATABASE *db );
extern unsigned MSI_CommitTables( MSIDATABASE *db );


/* string table functions */
enum StringPersistence
{
    StringPersistent = 0,
    StringNonPersistent = 1
};

extern BOOL msi_addstringW( string_table *st, const WCHAR *data, int len, uint16_t refcount, enum StringPersistence persistence );
extern unsigned msi_string2idW( const string_table *st, const WCHAR *buffer, unsigned *id );
extern VOID msi_destroy_stringtable( string_table *st );
extern const WCHAR *msi_string_lookup_id( const string_table *st, unsigned id );
extern HRESULT msi_init_string_table( IStorage *stg );
extern string_table *msi_load_string_table( IStorage *stg, unsigned *bytes_per_strref );
extern unsigned msi_save_string_table( const string_table *st, IStorage *storage, unsigned *bytes_per_strref );
extern unsigned msi_get_string_table_codepage( const string_table *st );
extern unsigned msi_set_string_table_codepage( string_table *st, unsigned codepage );

extern BOOL TABLE_Exists( MSIDATABASE *db, const WCHAR *name );
extern MSICONDITION MSI_DatabaseIsTablePersistent( MSIDATABASE *db, const WCHAR *table );

extern unsigned read_stream_data( IStorage *stg, const WCHAR *stname, BOOL table,
                              uint8_t **pdata, unsigned *psz );
extern unsigned write_stream_data( IStorage *stg, const WCHAR *stname,
                               const void *data, unsigned sz, BOOL bTable );

/* transform functions */
extern unsigned msi_table_apply_transform( MSIDATABASE *db, IStorage *stg );
extern unsigned MSI_DatabaseApplyTransformW( MSIDATABASE *db,
                 const WCHAR *szTransformFile, int iErrorCond );
extern void append_storage_to_db( MSIDATABASE *db, IStorage *stg );

/* record internals */
extern void MSI_CloseRecord( MSIOBJECT * );
extern unsigned MSI_RecordSetIStream( MSIRECORD *, unsigned, IStream *);
extern unsigned MSI_RecordGetIStream( MSIRECORD *, unsigned, IStream **);
extern const WCHAR *MSI_RecordGetString( const MSIRECORD *, unsigned );
extern MSIRECORD *MSI_CreateRecord( unsigned );
extern unsigned MSI_RecordSetInteger( MSIRECORD *, unsigned, int );
extern unsigned MSI_RecordSetIntPtr( MSIRECORD *, unsigned, intptr_t );
extern unsigned MSI_RecordSetStringW( MSIRECORD *, unsigned, const WCHAR *);
extern BOOL MSI_RecordIsNull( MSIRECORD *, unsigned );
extern unsigned MSI_RecordGetStringW( MSIRECORD *, unsigned, WCHAR *, unsigned *);
extern unsigned MSI_RecordGetStringA( MSIRECORD *, unsigned, char *, unsigned *);
extern int MSI_RecordGetInteger( MSIRECORD *, unsigned );
extern intptr_t MSI_RecordGetIntPtr( MSIRECORD *, unsigned );
extern unsigned MSI_RecordReadStream( MSIRECORD *, unsigned, char *, unsigned *);
extern unsigned MSI_RecordSetStream(MSIRECORD *, unsigned, IStream *);
extern unsigned MSI_RecordGetFieldCount( const MSIRECORD *rec );
extern unsigned MSI_RecordStreamToFile( MSIRECORD *, unsigned, const WCHAR *);
extern unsigned MSI_RecordSetStreamFromFileW( MSIRECORD *, unsigned, const WCHAR *);
extern unsigned MSI_RecordCopyField( MSIRECORD *, unsigned, MSIRECORD *, unsigned );
extern MSIRECORD *MSI_CloneRecord( MSIRECORD * );
extern BOOL MSI_RecordsAreEqual( MSIRECORD *, MSIRECORD * );
extern BOOL MSI_RecordsAreFieldsEqual(MSIRECORD *a, MSIRECORD *b, unsigned field);

/* stream internals */
extern void enum_stream_names( IStorage *stg );
extern WCHAR *encode_streamname(BOOL bTable, const WCHAR *in);
extern BOOL decode_streamname(const WCHAR *in, WCHAR *out);

/* database internals */
extern unsigned msi_get_raw_stream( MSIDATABASE *, const WCHAR *, IStream **);
extern unsigned msi_clone_open_stream( MSIDATABASE *, IStorage *, const WCHAR *, IStream ** );
void msi_destroy_stream( MSIDATABASE *, const WCHAR * );
extern unsigned MSI_OpenDatabaseW( const WCHAR *, const WCHAR *, MSIDATABASE **);
extern unsigned MSI_DatabaseOpenViewW(MSIDATABASE *, const WCHAR *, MSIQUERY **);
extern unsigned MSI_OpenQuery( MSIDATABASE *, MSIQUERY **, const WCHAR *, ... );
typedef unsigned (*record_func)( MSIRECORD *, void *);
extern unsigned MSI_IterateRecords( MSIQUERY *, unsigned *, record_func, void *);
extern MSIRECORD *MSI_QueryGetRecord( MSIDATABASE *db, const WCHAR *query, ... );
extern unsigned MSI_DatabaseGetPrimaryKeys( MSIDATABASE *, const WCHAR *, MSIRECORD **);

/* view internals */
extern unsigned MSI_ViewExecute( MSIQUERY*, MSIRECORD * );
extern unsigned MSI_ViewFetch( MSIQUERY*, MSIRECORD ** );
extern unsigned MSI_ViewClose( MSIQUERY* );
extern unsigned MSI_ViewGetColumnInfo(MSIQUERY *, MSICOLINFO, MSIRECORD **);
extern unsigned MSI_ViewModify( MSIQUERY *, MSIMODIFY, MSIRECORD * );
extern unsigned VIEW_find_column( MSIVIEW *, const WCHAR *, const WCHAR *, unsigned *);
extern unsigned msi_view_get_row(MSIDATABASE *, MSIVIEW *, unsigned, MSIRECORD **);

/* summary information */
extern MSISUMMARYINFO *MSI_GetSummaryInformationW( IStorage *stg, unsigned uiUpdateCount );
extern WCHAR *msi_suminfo_dup_string( MSISUMMARYINFO *si, unsigned uiProperty );
extern int msi_suminfo_get_int32( MSISUMMARYINFO *si, unsigned uiProperty );
extern WCHAR *msi_get_suminfo_product( IStorage *stg );
extern unsigned msi_add_suminfo( MSIDATABASE *db, WCHAR ***records, int num_records, int num_columns );

/* Helpers */
extern WCHAR *msi_dup_record_field(MSIRECORD *row, int index);
extern WCHAR *msi_dup_property( MSIDATABASE *db, const WCHAR *prop );
extern unsigned msi_set_property( MSIDATABASE *, const WCHAR *, const WCHAR *);
extern unsigned msi_get_property( MSIDATABASE *, const WCHAR *, WCHAR *, unsigned *);
extern int msi_get_property_int( MSIDATABASE *package, const WCHAR *prop, int def );

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

static inline char *strdupWtoA( const WCHAR *str )
{
    char *ret = NULL;
    unsigned len;

    if (!str) return ret;
    len = WideCharToMultiByte( CP_ACP, 0, str, -1, NULL, 0, NULL, NULL);
    ret = msi_alloc( len );
    if (ret)
        WideCharToMultiByte( CP_ACP, 0, str, -1, ret, len, NULL, NULL );
    return ret;
}

static inline WCHAR *strdupAtoW( const char *str )
{
    WCHAR *ret = NULL;
    unsigned len;

    if (!str) return ret;
    len = MultiByteToWideChar( CP_ACP, 0, str, -1, NULL, 0 );
    ret = msi_alloc( len * sizeof(WCHAR) );
    if (ret)
        MultiByteToWideChar( CP_ACP, 0, str, -1, ret, len );
    return ret;
}

static inline WCHAR *strdupW( const WCHAR *src )
{
    WCHAR *dest;
    if (!src) return NULL;
    dest = msi_alloc( (lstrlenW(src)+1)*sizeof(WCHAR) );
    if (dest)
        lstrcpyW(dest, src);
    return dest;
}

#pragma GCC visibility pop

#endif /* __WINE_MSI_PRIVATE__ */
