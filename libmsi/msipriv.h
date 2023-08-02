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
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <glib.h>

#include <gsf/gsf.h>
#include <gsf/gsf-input.h>
#include <gsf/gsf-infile.h>
#include <gsf/gsf-output.h>
#include <gsf/gsf-outfile.h>
#include <gsf/gsf-input-memory.h>
#include <gsf/gsf-input-stdio.h>
#include <gsf/gsf-output-stdio.h>
#include <gsf/gsf-infile-msole.h>
#include <gsf/gsf-outfile-msole.h>


#include "libmsi.h"
#include "list.h"

#pragma GCC visibility push(hidden)

#ifndef MAX_PATH
#define MAX_PATH PATH_MAX
#endif

typedef LibmsiResultError LibmsiResult;

typedef enum LibmsiCondition
{
    LIBMSI_CONDITION_FALSE = 0,
    LIBMSI_CONDITION_TRUE  = 1,
    LIBMSI_CONDITION_NONE  = 2,
    LIBMSI_CONDITION_ERROR = 3,
} LibmsiCondition;

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

#define NO_MORE_ITEMS G_MAXINT

#define MSITYPE_IS_BINARY(type) (((type) & ~MSITYPE_NULLABLE) == (MSITYPE_STRING|MSITYPE_VALID))

struct _LibmsiTable;
typedef struct _LibmsiTable LibmsiTable;

struct string_table;
typedef struct string_table string_table;

#define MSI_INITIAL_MEDIA_TRANSFORM_OFFSET 10000
#define MSI_INITIAL_MEDIA_TRANSFORM_DISKID 30000

struct _LibmsiDatabase
{
    GObject parent;

    GsfInfile *infile;
    GsfOutfile *outfile;
    string_table *strings;
    unsigned bytes_per_strref;
    char *path;
    char *outpath;
    bool rename_outpath;
    guint flags;
    unsigned media_transform_offset;
    unsigned media_transform_disk_id;
    struct list tables;
    struct list transforms;
    struct list streams;
    struct list storages;
};

typedef struct _LibmsiView LibmsiView;

struct _LibmsiQuery
{
    GObject parent;

    LibmsiView *view;
    unsigned row;
    LibmsiDatabase *database;
    gchar *query;
    struct list mem;
};

/* maybe we can use a Variant instead of doing it ourselves? */
typedef struct _LibmsiField
{
    unsigned type;
    union
    {
        int iVal;
        char *szVal;
        GsfInput *stream;
    } u;
} LibmsiField;

struct _LibmsiRecord
{
    GObject parent;

    unsigned count;       /* as passed to libmsi_record_new */
    LibmsiField *fields;  /* nb. array size is count+1 */
};

typedef struct _column_info
{
    const char *table;
    const char *column;
    int   type;
    bool   temporary;
    struct expr *val;
    struct _column_info *next;
} column_info;

typedef const struct _LibmsiColumnHashEntry *MSIITERHANDLE;

typedef struct _LibmsiViewOps
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
    unsigned (*fetch_int)( LibmsiView *view, unsigned row, unsigned col, unsigned *val );

    /*
     * fetch_stream - gets a stream from {row,col} in the table
     *
     *  This function is similar to fetch_int, except fetches a
     *    stream instead of an integer.
     */
    unsigned (*fetch_stream)( LibmsiView *view, unsigned row, unsigned col, GsfInput **stm );

    /*
     * get_row - gets values from a row
     *
     */
    unsigned (*get_row)( LibmsiView *view, unsigned row, LibmsiRecord **rec );

    /*
     * set_row - sets values in a row as specified by mask
     *
     *  Similar semantics to fetch_int
     */
    unsigned (*set_row)( LibmsiView *view, unsigned row, LibmsiRecord *rec, unsigned mask );

    /*
     * Inserts a new row into the database from the records contents
     */
    unsigned (*insert_row)( LibmsiView *view, LibmsiRecord *record, unsigned row, bool temporary );

    /*
     * Deletes a row from the database
     */
    unsigned (*delete_row)( LibmsiView *view, unsigned row );

    /*
     * execute - loads the underlying data into memory so it can be read
     */
    unsigned (*execute)( LibmsiView *view, LibmsiRecord *record );

    /*
     * close - clears the data read by execute from memory
     */
    unsigned (*close)( LibmsiView *view );

    /*
     * get_dimensions - returns the number of rows or columns in a table.
     *
     *  The number of rows can only be queried after the execute method
     *   is called. The number of columns can be queried at any time.
     */
    unsigned (*get_dimensions)( LibmsiView *view, unsigned *rows, unsigned *cols );

    /*
     * get_column_info - returns the name and type of a specific column
     *
     *  The column information can be queried at any time.
     */
    unsigned (*get_column_info)( LibmsiView *view, unsigned n, const char **name, unsigned *type,
                             bool *temporary, const char **table_name );

    /*
     * delete - destroys the structure completely
     */
    unsigned (*delete)( LibmsiView * );

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
    unsigned (*find_matching_rows)( LibmsiView *view, unsigned col, unsigned val, unsigned *row, MSIITERHANDLE *handle );

    /*
     * add_ref - increases the reference count of the table
     */
    unsigned (*add_ref)( LibmsiView *view );

    /*
     * release - decreases the reference count of the table
     */
    unsigned (*release)( LibmsiView *view );

    /*
     * add_column - adds a column to the table
     */
    unsigned (*add_column)( LibmsiView *view, const char *table, unsigned number, const char *column, unsigned type, bool hold );

    /*
     * remove_column - removes the column represented by table name and column number from the table
     */
    unsigned (*remove_column)( LibmsiView *view, const char *table, unsigned number );

    /*
     * sort - orders the table by columns
     */
    unsigned (*sort)( LibmsiView *view, column_info *columns );

    /*
     * drop - drops the table from the database
     */
    unsigned (*drop)( LibmsiView *view );
} LibmsiViewOps;

struct _LibmsiView
{
    const LibmsiViewOps *ops;
    LibmsiDBError error;
    const char *error_column;
};

#define MSI_MAX_PROPS 20

enum LibmsiOLEVariantType
{
    OLEVT_EMPTY = 0,
    OLEVT_NULL = 1,
    OLEVT_I2 = 2,
    OLEVT_I4 = 3,
    OLEVT_LPSTR = 30,
    OLEVT_FILETIME = 64,
};

typedef struct _LibmsiOLEVariant
{
    enum LibmsiOLEVariantType vt;
    union {
        int intval;
        char *strval;
        uint64_t filetime;
    };
} LibmsiOLEVariant;

struct _LibmsiSummaryInfo
{
    GObject parent;

    LibmsiDatabase *database;
    unsigned update_count;
    LibmsiOLEVariant property[MSI_MAX_PROPS];
};

typedef struct _LibmsiIStream LibmsiIStream;

extern const guint8 clsid_msi_transform[16];
extern const guint8 clsid_msi_database[16];
extern const guint8 clsid_msi_patch[16];

/* handle unicode/ascii output in the Msi* API functions */
typedef struct {
    bool unicode;
    union {
       char *a;
       char *w;
    } str;
} awstring;

typedef struct {
    bool unicode;
    union {
       const char *a;
       const char *w;
    } str;
} awcstring;

unsigned msi_strcpy_to_awstring( const char *str, awstring *awbuf, unsigned *sz );

extern void free_cached_tables( LibmsiDatabase *db );
extern unsigned _libmsi_database_commit_tables( LibmsiDatabase *db, unsigned bytes_per_strref );


/* string table functions */
enum StringPersistence
{
    StringPersistent = 0,
    StringNonPersistent = 1
};

extern int _libmsi_add_string( string_table *st, const char *data, int len, uint16_t refcount, enum StringPersistence persistence );
extern unsigned _libmsi_id_from_string_utf8( const string_table *st, const char *buffer, unsigned *id );
extern void msi_destroy_stringtable( string_table *st );
extern const char *msi_string_lookup_id( const string_table *st, unsigned id );
extern string_table *msi_init_string_table( unsigned *bytes_per_strref );
extern string_table *msi_load_string_table( GsfInfile *stg, unsigned *bytes_per_strref );
extern unsigned msi_save_string_table( const string_table *st, LibmsiDatabase *db, unsigned *bytes_per_strref );
extern unsigned msi_get_string_table_codepage( const string_table *st );
extern unsigned msi_set_string_table_codepage( string_table *st, unsigned codepage );

unsigned _libmsi_open_table( LibmsiDatabase *db, const char *name, bool encoded );
extern bool table_view_exists( LibmsiDatabase *db, const char *name );
extern LibmsiCondition _libmsi_database_is_table_persistent( LibmsiDatabase *db, const char *table );

extern unsigned read_stream_data( GsfInfile *stg, const char *stname,
                              uint8_t **pdata, unsigned *psz );
extern unsigned write_stream_data( LibmsiDatabase *db, const char *stname,
                               const void *data, unsigned sz );
extern unsigned write_raw_stream_data( LibmsiDatabase *db, const char *stname,
                        const void *data, unsigned sz, GsfInput **outstm );
extern unsigned _libmsi_database_commit_streams( LibmsiDatabase *db );

/* transform functions */
extern unsigned msi_table_apply_transform( LibmsiDatabase *db, GsfInfile *stg );
extern unsigned _libmsi_database_apply_transform( LibmsiDatabase *db,
                 const char *szTransformFile);
extern void append_storage_to_db( LibmsiDatabase *db, GsfInfile *stg );
extern unsigned _libmsi_database_commit_storages( LibmsiDatabase *db );

/* record internals */
extern void _libmsi_record_destroy( LibmsiRecord * );
extern unsigned _libmsi_record_set_gsf_input( LibmsiRecord *, unsigned, GsfInput *);
extern unsigned _libmsi_record_get_gsf_input( const LibmsiRecord *, unsigned, GsfInput **);
extern const char *_libmsi_record_get_string_raw( const LibmsiRecord *, unsigned );
extern unsigned _libmsi_record_get_string( const LibmsiRecord *, unsigned, char *, unsigned *);
extern unsigned _libmsi_record_save_stream( const LibmsiRecord *, unsigned, char *, unsigned *);
extern unsigned _libmsi_record_load_stream(LibmsiRecord *, unsigned, GsfInput *);
extern unsigned _libmsi_record_load_stream_from_file( LibmsiRecord *, unsigned, const char *);
extern unsigned _libmsi_record_copy_field( LibmsiRecord *, unsigned, LibmsiRecord *, unsigned );
extern LibmsiRecord *_libmsi_record_clone( LibmsiRecord * );
extern bool _libmsi_record_compare( const LibmsiRecord *, const LibmsiRecord * );
extern bool _libmsi_record_compare_fields(const LibmsiRecord *a, const LibmsiRecord *b, unsigned field);

/* stream internals */
extern void enum_stream_names( GsfInfile *stg );
extern char *encode_streamname(bool bTable, const char *in);
extern char *decode_streamname(const char *in);

/* database internals */
extern LibmsiResult _libmsi_database_start_transaction(LibmsiDatabase *db);
extern LibmsiResult _libmsi_database_open(LibmsiDatabase *db);
extern LibmsiResult _libmsi_database_close(LibmsiDatabase *db, bool committed);
unsigned msi_create_stream( LibmsiDatabase *db, const char *stname, GsfInput *stm );
extern unsigned msi_get_raw_stream( LibmsiDatabase *, const char *, GsfInput **);
void msi_destroy_stream( LibmsiDatabase *, const char * );
extern unsigned msi_enum_db_streams(LibmsiDatabase *, unsigned (*fn)(const char *, GsfInput *, void *), void *);
unsigned msi_create_storage( LibmsiDatabase *db, const char *stname, GsfInput *stm );
unsigned msi_open_storage( LibmsiDatabase *db, const char *stname );
void msi_destroy_storage( LibmsiDatabase *db, const char *stname );
extern unsigned msi_enum_db_storages(LibmsiDatabase *, unsigned (*fn)(const char *, GsfInfile *, void *), void *);
extern unsigned _libmsi_database_open_query(LibmsiDatabase *, const char *, LibmsiQuery **);
extern unsigned _libmsi_query_open( LibmsiDatabase *, LibmsiQuery **, const char *, ... ) G_GNUC_PRINTF(3,4);
typedef unsigned (*record_func)( LibmsiRecord *, void *);
extern unsigned _libmsi_query_iterate_records( LibmsiQuery *, unsigned *, record_func, void *);
extern LibmsiRecord *_libmsi_query_get_record( LibmsiDatabase *db, const char *query, ... ) G_GNUC_PRINTF(2,3);
extern unsigned _libmsi_database_get_primary_keys( LibmsiDatabase *, const char *, LibmsiRecord **);

/* view internals */
extern LibmsiResult _libmsi_query_execute( LibmsiQuery*, LibmsiRecord * );
extern LibmsiResult _libmsi_query_fetch( LibmsiQuery*, LibmsiRecord ** );
extern LibmsiResult _libmsi_query_get_column_info(LibmsiQuery *, LibmsiColInfo, LibmsiRecord **);
extern unsigned _libmsi_view_find_column( LibmsiView *, const char *, const char *, unsigned *);
extern unsigned msi_view_get_row(LibmsiDatabase *, LibmsiView *, unsigned, LibmsiRecord **);

/* summary information */
extern unsigned msi_add_suminfo( LibmsiDatabase *db, char ***records, int num_records, int num_columns );
gchar* summary_info_as_string (LibmsiSummaryInfo *si, unsigned uiProperty);

/* IStream internals */
LibmsiIStream * libmsi_istream_new (GsfInput *input);

/* Helpers */
extern char *msi_dup_record_field(LibmsiRecord *row, int index);
extern char *msi_dup_property( LibmsiDatabase *db, const char *prop );
extern unsigned msi_set_property( LibmsiDatabase *, const char *, const char *);
extern unsigned msi_get_property( LibmsiDatabase *, const char *, char *, unsigned *);
extern int msi_get_property_int( LibmsiDatabase *package, const char *prop, int def );

/* common strings */
static const char szEmpty[] = "";
static const char szStreams[] = "_Streams";
static const char szStorages[] = "_Storages";
static const char szStringData[] = "_StringData";
static const char szStringPool[] = "_StringPool";
static const char szName[] = "Name";
static const char szData[] = "Data";

/* memory allocation macro functions */

static void *msi_alloc( size_t len ) G_GNUC_ALLOC_SIZE(1);
static inline void *msi_alloc( size_t len )
{
    return malloc(len);
}

static void *msi_alloc_zero( size_t len ) G_GNUC_ALLOC_SIZE(1);
static inline void *msi_alloc_zero( size_t len )
{
    return calloc(len, 1);
}

static void *msi_realloc( void *mem, size_t len ) G_GNUC_ALLOC_SIZE(2);
static inline void *msi_realloc( void *mem, size_t len )
{
    return realloc(mem, len);
}

static void *msi_realloc_zero( void *mem, size_t oldlen, size_t len ) G_GNUC_ALLOC_SIZE(3);
static inline void *msi_realloc_zero( void *mem, size_t oldlen, size_t len )
{
    mem = realloc( mem, len );
    memset((char*)mem + oldlen, 0, len - oldlen);
    return mem;
}

static inline void msi_free( void *mem )
{
    free(mem);
}

static inline char *strcpyn( char *dst, const char *src, unsigned count )
{
    char *d = dst;
    const char *s = src;

    while ((count > 1) && *s)
    {
        count--;
        *d++ = *s++;
    }
    if (count) *d = 0;
    return dst;
}

#pragma GCC visibility pop

#endif /* __WINE_MSI_PRIVATE__ */
