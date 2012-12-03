/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002-2005 Mike McCormack for CodeWeavers
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

#include <stdarg.h>
#include <assert.h>

#define COBJMACROS
#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "libmsi.h"
#include "objbase.h"
#include "objidl.h"
#include "winnls.h"
#include "msipriv.h"
#include "query.h"

#include "debug.h"
#include "unicode.h"


#define LibmsiTable_HASH_TABLE_SIZE 37

typedef struct LibmsiColumnHashEntry
{
    struct LibmsiColumnHashEntry *next;
    unsigned value;
    unsigned row;
} LibmsiColumnHashEntry;

typedef struct LibmsiColumnInfo
{
    const WCHAR *tablename;
    unsigned    number;
    const WCHAR *colname;
    unsigned    type;
    unsigned    offset;
    int     ref_count;
    bool    temporary;
    LibmsiColumnHashEntry **hash_table;
} LibmsiColumnInfo;

struct LibmsiTable
{
    uint8_t **data;
    bool *data_persistent;
    unsigned row_count;
    struct list entry;
    LibmsiColumnInfo *colinfo;
    unsigned col_count;
    LibmsiCondition persistent;
    int ref_count;
    WCHAR name[1];
};

/* information for default tables */
static const WCHAR szTables[]  = {'_','T','a','b','l','e','s',0};
static const WCHAR szTable[]   = {'T','a','b','l','e',0};
static const WCHAR szColumns[] = {'_','C','o','l','u','m','n','s',0};
static const WCHAR szNumber[]  = {'N','u','m','b','e','r',0};
static const WCHAR szType[]    = {'T','y','p','e',0};

static const LibmsiColumnInfo _Columns_cols[4] = {
    { szColumns, 1, szTable,  MSITYPE_VALID | MSITYPE_STRING | MSITYPE_KEY | 64, 0, 0, 0, NULL },
    { szColumns, 2, szNumber, MSITYPE_VALID | MSITYPE_KEY | 2,     2, 0, 0, NULL },
    { szColumns, 3, szName,   MSITYPE_VALID | MSITYPE_STRING | 64, 4, 0, 0, NULL },
    { szColumns, 4, szType,   MSITYPE_VALID | 2,                   6, 0, 0, NULL },
};

static const LibmsiColumnInfo _Tables_cols[1] = {
    { szTables,  1, szName,   MSITYPE_VALID | MSITYPE_STRING | MSITYPE_KEY | 64, 0, 0, 0, NULL },
};

#define MAX_STREAM_NAME 0x1f

static inline unsigned bytes_per_column( LibmsiDatabase *db, const LibmsiColumnInfo *col, unsigned bytes_per_strref )
{
    if( MSITYPE_IS_BINARY(col->type) )
        return 2;

    if( col->type & MSITYPE_STRING )
        return bytes_per_strref;

    if( (col->type & 0xff) <= 2)
        return 2;

    if( (col->type & 0xff) != 4 )
        ERR("Invalid column size!\n");

    return 4;
}

static int utf2mime(int x)
{
    if( (x>='0') && (x<='9') )
        return x-'0';
    if( (x>='A') && (x<='Z') )
        return x-'A'+10;
    if( (x>='a') && (x<='z') )
        return x-'a'+10+26;
    if( x=='.' )
        return 10+26+26;
    if( x=='_' )
        return 10+26+26+1;
    return -1;
}

WCHAR *encode_streamname(bool bTable, const WCHAR *in)
{
    unsigned count = MAX_STREAM_NAME;
    unsigned ch, next;
    WCHAR *out_wc;
    CHAR *out;
    CHAR *p;

    if( !bTable )
        count = strlenW( in )+2;
    if (!(out = msi_alloc( count*3 ))) return NULL;
    p = out;

    if( bTable )
    {
        /* UTF-8 encoding of 0x4840.  */
        *p++ = 0xe4;
        *p++ = 0xa1;
        *p++ = 0x80;
        count --;
    }
    while( count -- )
    {
        ch = *in++;
        if( !ch )
        {
            *p = ch;

            /* Convert UTF-8 to WCHAR */
	    out_wc = strdupUTF8toW(out);
	    msi_free(out);
            return out_wc;
        }
        if( ( ch < 0x80 ) && ( utf2mime(ch) >= 0 ) )
        {
            ch = utf2mime(ch);
            next = *in;
            if( next && (next<0x80) ) {
                next = utf2mime(next);
            } else {
                next = -1;
            }
            if( next == -1 )
            {
                /* UTF-8 encoding of 0x4800..0x483f.  */
                *p++ = 0xe4;
                *p++ = 0xa0;
                *p++ = 0x80 | ch;
            } else {
                /* UTF-8 encoding of 0x3800..0x47ff.  */
                *p++ = 0xe3 + (next >> 5);
                *p++ = 0xa0 ^ next;
                *p++ = 0x80 | ch;
                in++;
            }
        } else {
            // *p++ = ch;
            p += WideCharToMultiByte( CP_UTF8, 0, &ch, 1, p, 5, NULL, NULL );
        }
    }
    ERR("Failed to encode stream name (%s)\n",debugstr_w(in));
    msi_free( out );
    return NULL;
}

static int mime2utf(int x)
{
    if( x<10 )
        return x + '0';
    if( x<(10+26))
        return x - 10 + 'A';
    if( x<(10+26+26))
        return x - 10 - 26 + 'a';
    if( x == (10+26+26) )
        return '.';
    return '_';
}

void decode_streamname(const WCHAR *in, WCHAR *out)
{
    unsigned count = 0;
    uint8_t *enc_utf8 = strdupWtoUTF8(in);
    uint8_t *p = enc_utf8;

    uint8_t *dec_utf8 = strdup(enc_utf8);
    uint8_t *q = dec_utf8;

    while ( *p )
    {
        uint8_t ch = *p;
        if( (ch == 0xe3 && p[1] >= 0xa0) || (ch == 0xe4 && p[1] < 0xa0) )
        {
            /* UTF-8 encoding of 0x3800..0x47ff.  */
            *q++ = mime2utf(p[2]&0x7f);
            *q++ = mime2utf(p[1]^0xa0);
            p += 3;
            count += 2;
            continue;
        }
        if( ch == 0xe4 && p[1] == 0xa0 ) {
            /* UTF-8 encoding of 0x4800..0x483f.  */
            *q++ = mime2utf(p[2]&0x7f);
            p += 3;
            count++;
            continue;
        }
        *q++ = *p++;
        if( ch >= 0xc1) {
            *q++ = *p++;
        }
        if( ch >= 0xe0) {
            *q++ = *p++;
        }
        if( ch >= 0xf0) {
            *q++ = *p++;
        }
        count++;
    }
    *q = 0;
    msi_free(enc_utf8);

    /* convert UTF8 to WCHAR */
    MultiByteToWideChar( CP_UTF8, 0, dec_utf8, -1, out, count + 1 );
    msi_free(dec_utf8);
}

void enum_stream_names( IStorage *stg )
{
    IEnumSTATSTG *stgenum = NULL;
    HRESULT r;
    STATSTG stat;
    unsigned n, count;
    WCHAR name[0x40];

    r = IStorage_EnumElements( stg, 0, NULL, 0, &stgenum );
    if( FAILED( r ) )
        return;

    n = 0;
    while( 1 )
    {
        count = 0;
        r = IEnumSTATSTG_Next( stgenum, 1, &stat, &count );
        if( FAILED( r ) || !count )
            break;
        decode_streamname( stat.pwcsName, name );
        TRACE("stream %2d -> %s %s\n", n,
              debugstr_w(stat.pwcsName), debugstr_w(name) );
        CoTaskMemFree( stat.pwcsName );
        n++;
    }

    IEnumSTATSTG_Release( stgenum );
}

unsigned read_stream_data( IStorage *stg, const WCHAR *stname, bool table,
                       uint8_t **pdata, unsigned *psz )
{
    HRESULT r;
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    VOID *data;
    unsigned sz, count;
    IStream *stm = NULL;
    STATSTG stat;
    WCHAR *encname;

    encname = encode_streamname(table, stname);

    TRACE("%s -> %s\n",debugstr_w(stname),debugstr_w(encname));

    r = IStorage_OpenStream(stg, encname, NULL, 
            STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm);
    msi_free( encname );
    if( FAILED( r ) )
    {
        WARN("open stream failed r = %08x - empty table?\n", r);
        return ret;
    }

    r = IStream_Stat(stm, &stat, STATFLAG_NONAME );
    if( FAILED( r ) )
    {
        WARN("open stream failed r = %08x!\n", r);
        goto end;
    }

    if( stat.cbSize.QuadPart >> 32 )
    {
        WARN("Too big!\n");
        goto end;
    }
        
    sz = stat.cbSize.QuadPart;
    data = msi_alloc( sz );
    if( !data )
    {
        WARN("couldn't allocate memory r=%08x!\n", r);
        ret = LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
        goto end;
    }
        
    r = IStream_Read(stm, data, sz, &count );
    if( FAILED( r ) || ( count != sz ) )
    {
        msi_free( data );
        WARN("read stream failed r = %08x!\n", r);
        goto end;
    }

    *pdata = data;
    *psz = sz;
    ret = LIBMSI_RESULT_SUCCESS;

end:
    IStream_Release( stm );

    return ret;
}

unsigned write_stream_data( IStorage *stg, const WCHAR *stname,
                        const void *data, unsigned sz, bool bTable )
{
    HRESULT r;
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    unsigned count;
    IStream *stm = NULL;
    ULARGE_INTEGER size;
    LARGE_INTEGER pos;
    WCHAR *encname;

    encname = encode_streamname(bTable, stname );
    r = IStorage_OpenStream( stg, encname, NULL, 
            STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, &stm);
    if( FAILED(r) )
    {
        r = IStorage_CreateStream( stg, encname,
                STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm);
    }
    msi_free( encname );
    if( FAILED( r ) )
    {
        WARN("open stream failed r = %08x\n", r);
        return ret;
    }

    size.QuadPart = sz;
    r = IStream_SetSize( stm, size );
    if( FAILED( r ) )
    {
        WARN("Failed to SetSize\n");
        goto end;
    }

    pos.QuadPart = 0;
    r = IStream_Seek( stm, pos, STREAM_SEEK_SET, NULL );
    if( FAILED( r ) )
    {
        WARN("Failed to Seek\n");
        goto end;
    }

    if (sz)
    {
        r = IStream_Write(stm, data, sz, &count );
        if( FAILED( r ) || ( count != sz ) )
        {
            WARN("Failed to Write\n");
            goto end;
        }
    }

    ret = LIBMSI_RESULT_SUCCESS;

end:
    IStream_Release( stm );

    return ret;
}

static void msi_free_colinfo( LibmsiColumnInfo *colinfo, unsigned count )
{
    unsigned i;
    for (i = 0; i < count; i++) msi_free( colinfo[i].hash_table );
}

static void free_table( LibmsiTable *table )
{
    unsigned i;
    for( i=0; i<table->row_count; i++ )
        msi_free( table->data[i] );
    msi_free( table->data );
    msi_free( table->data_persistent );
    msi_free_colinfo( table->colinfo, table->col_count );
    msi_free( table->colinfo );
    msi_free( table );
}

static unsigned msi_table_get_row_size( LibmsiDatabase *db, const LibmsiColumnInfo *cols, unsigned count, unsigned bytes_per_strref )
{
    const LibmsiColumnInfo *last_col;

    if (!count)
        return 0;

    if (bytes_per_strref != LONG_STR_BYTES)
    {
        unsigned i, size = 0;
        for (i = 0; i < count; i++) size += bytes_per_column( db, &cols[i], bytes_per_strref );
        return size;
    }
    last_col = &cols[count - 1];
    return last_col->offset + bytes_per_column( db, last_col, bytes_per_strref );
}

/* add this table to the list of cached tables in the database */
static unsigned read_table_from_storage( LibmsiDatabase *db, LibmsiTable *t, IStorage *stg )
{
    uint8_t *rawdata = NULL;
    unsigned rawsize = 0, i, j, row_size, row_size_mem;

    TRACE("%s\n",debugstr_w(t->name));

    row_size = msi_table_get_row_size( db, t->colinfo, t->col_count, db->bytes_per_strref );
    row_size_mem = msi_table_get_row_size( db, t->colinfo, t->col_count, LONG_STR_BYTES );

    /* if we can't read the table, just assume that it's empty */
    read_stream_data( stg, t->name, true, &rawdata, &rawsize );
    if( !rawdata )
        return LIBMSI_RESULT_SUCCESS;

    TRACE("Read %d bytes\n", rawsize );

    if( rawsize % row_size )
    {
        WARN("Table size is invalid %d/%d\n", rawsize, row_size );
        goto err;
    }

    t->row_count = rawsize / row_size;
    t->data = msi_alloc_zero( t->row_count * sizeof (uint16_t*) );
    if( !t->data )
        goto err;
    t->data_persistent = msi_alloc_zero( t->row_count * sizeof(bool));
    if ( !t->data_persistent )
        goto err;

    /* transpose all the data */
    TRACE("Transposing data from %d rows\n", t->row_count );
    for (i = 0; i < t->row_count; i++)
    {
        unsigned ofs = 0, ofs_mem = 0;

        t->data[i] = msi_alloc( row_size_mem );
        if( !t->data[i] )
            goto err;
        t->data_persistent[i] = true;

        for (j = 0; j < t->col_count; j++)
        {
            unsigned m = bytes_per_column( db, &t->colinfo[j], LONG_STR_BYTES );
            unsigned n = bytes_per_column( db, &t->colinfo[j], db->bytes_per_strref );
            unsigned k;

            if ( n != 2 && n != 3 && n != 4 )
            {
                ERR("oops - unknown column width %d\n", n);
                goto err;
            }
            if (t->colinfo[j].type & MSITYPE_STRING && n < m)
            {
                for (k = 0; k < m; k++)
                {
                    if (k < n)
                        t->data[i][ofs_mem + k] = rawdata[ofs * t->row_count + i * n + k];
                    else
                        t->data[i][ofs_mem + k] = 0;
                }
            }
            else
            {
                for (k = 0; k < n; k++)
                    t->data[i][ofs_mem + k] = rawdata[ofs * t->row_count + i * n + k];
            }
            ofs_mem += m;
            ofs += n;
        }
    }

    msi_free( rawdata );
    return LIBMSI_RESULT_SUCCESS;
err:
    msi_free( rawdata );
    return LIBMSI_RESULT_FUNCTION_FAILED;
}

void free_cached_tables( LibmsiDatabase *db )
{
    while( !list_empty( &db->tables ) )
    {
        LibmsiTable *t = LIST_ENTRY( list_head( &db->tables ), LibmsiTable, entry );

        list_remove( &t->entry );
        free_table( t );
    }
}

static LibmsiTable *find_cached_table( LibmsiDatabase *db, const WCHAR *name )
{
    LibmsiTable *t;

    LIST_FOR_EACH_ENTRY( t, &db->tables, LibmsiTable, entry )
        if( !strcmpW( name, t->name ) )
            return t;

    return NULL;
}

static void table_calc_column_offsets( LibmsiDatabase *db, LibmsiColumnInfo *colinfo, unsigned count )
{
    unsigned i;

    for (i = 0; colinfo && i < count; i++)
    {
         assert( i + 1 == colinfo[i].number );
         if (i) colinfo[i].offset = colinfo[i - 1].offset +
                                    bytes_per_column( db, &colinfo[i - 1], LONG_STR_BYTES );
         else colinfo[i].offset = 0;

         TRACE("column %d is [%s] with type %08x ofs %d\n",
               colinfo[i].number, debugstr_w(colinfo[i].colname),
               colinfo[i].type, colinfo[i].offset);
    }
}

static unsigned get_defaulttablecolumns( LibmsiDatabase *db, const WCHAR *name, LibmsiColumnInfo *colinfo, unsigned *sz )
{
    const LibmsiColumnInfo *p;
    unsigned i, n;

    TRACE("%s\n", debugstr_w(name));

    if (!strcmpW( name, szTables ))
    {
        p = _Tables_cols;
        n = 1;
    }
    else if (!strcmpW( name, szColumns ))
    {
        p = _Columns_cols;
        n = 4;
    }
    else return LIBMSI_RESULT_FUNCTION_FAILED;

    for (i = 0; i < n; i++)
    {
        if (colinfo && i < *sz) colinfo[i] = p[i];
        if (colinfo && i >= *sz) break;
    }
    table_calc_column_offsets( db, colinfo, n );
    *sz = n;
    return LIBMSI_RESULT_SUCCESS;
}

static unsigned get_tablecolumns( LibmsiDatabase *db, const WCHAR *szTableName, LibmsiColumnInfo *colinfo, unsigned *sz );

static unsigned table_get_column_info( LibmsiDatabase *db, const WCHAR *name, LibmsiColumnInfo **pcols, unsigned *pcount )
{
    unsigned r, column_count = 0;
    LibmsiColumnInfo *columns;

    /* get the number of columns in this table */
    column_count = 0;
    r = get_tablecolumns( db, name, NULL, &column_count );
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    *pcount = column_count;

    /* if there's no columns, there's no table */
    if (!column_count)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    TRACE("table %s found\n", debugstr_w(name));

    columns = msi_alloc( column_count * sizeof(LibmsiColumnInfo) );
    if (!columns)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    r = get_tablecolumns( db, name, columns, &column_count );
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        msi_free( columns );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }
    *pcols = columns;
    return r;
}

static unsigned get_table( LibmsiDatabase *db, const WCHAR *name, LibmsiTable **table_ret )
{
    LibmsiTable *table;
    unsigned r;

    /* first, see if the table is cached */
    table = find_cached_table( db, name );
    if (table)
    {
        *table_ret = table;
        return LIBMSI_RESULT_SUCCESS;
    }

    /* nonexistent tables should be interpreted as empty tables */
    table = msi_alloc( sizeof(LibmsiTable) + strlenW( name ) * sizeof(WCHAR) );
    if (!table)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    table->row_count = 0;
    table->data = NULL;
    table->data_persistent = NULL;
    table->colinfo = NULL;
    table->col_count = 0;
    table->persistent = LIBMSI_CONDITION_TRUE;
    strcpyW( table->name, name );

    if (!strcmpW( name, szTables ) || !strcmpW( name, szColumns ))
        table->persistent = LIBMSI_CONDITION_NONE;

    r = table_get_column_info( db, name, &table->colinfo, &table->col_count );
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        free_table( table );
        return r;
    }
    r = read_table_from_storage( db, table, db->storage );
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        free_table( table );
        return r;
    }
    list_add_head( &db->tables, &table->entry );
    *table_ret = table;
    return LIBMSI_RESULT_SUCCESS;
}

static unsigned read_table_int( uint8_t *const *data, unsigned row, unsigned col, unsigned bytes )
{
    unsigned ret = 0, i;

    for (i = 0; i < bytes; i++)
        ret += data[row][col + i] << i * 8;

    return ret;
}

static unsigned get_tablecolumns( LibmsiDatabase *db, const WCHAR *szTableName, LibmsiColumnInfo *colinfo, unsigned *sz )
{
    unsigned r, i, n = 0, table_id, count, maxcount = *sz;
    LibmsiTable *table = NULL;

    TRACE("%s\n", debugstr_w(szTableName));

    /* first check if there is a default table with that name */
    r = get_defaulttablecolumns( db, szTableName, colinfo, sz );
    if (r == LIBMSI_RESULT_SUCCESS && *sz)
        return r;

    r = get_table( db, szColumns, &table );
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        ERR("couldn't load _Columns table\n");
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    /* convert table and column names to IDs from the string table */
    r = _libmsi_id_from_stringW( db->strings, szTableName, &table_id );
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        WARN("Couldn't find id for %s\n", debugstr_w(szTableName));
        return r;
    }
    TRACE("Table id is %d, row count is %d\n", table_id, table->row_count);

    /* Note: _Columns table doesn't have non-persistent data */

    /* if maxcount is non-zero, assume it's exactly right for this table */
    memset( colinfo, 0, maxcount * sizeof(*colinfo) );
    count = table->row_count;
    for (i = 0; i < count; i++)
    {
        if (read_table_int( table->data, i, 0, LONG_STR_BYTES) != table_id) continue;
        if (colinfo)
        {
            unsigned id = read_table_int( table->data, i, table->colinfo[2].offset, LONG_STR_BYTES );
            unsigned col = read_table_int( table->data, i, table->colinfo[1].offset, sizeof(uint16_t) ) - (1 << 15);

            /* check the column number is in range */
            if (col < 1 || col > maxcount)
            {
                ERR("column %d out of range\n", col);
                continue;
            }
            /* check if this column was already set */
            if (colinfo[col - 1].number)
            {
                ERR("duplicate column %d\n", col);
                continue;
            }
            colinfo[col - 1].tablename = msi_string_lookup_id( db->strings, table_id );
            colinfo[col - 1].number = col;
            colinfo[col - 1].colname = msi_string_lookup_id( db->strings, id );
            colinfo[col - 1].type = read_table_int( table->data, i, table->colinfo[3].offset,
                                                    sizeof(uint16_t) ) - (1 << 15);
            colinfo[col - 1].offset = 0;
            colinfo[col - 1].ref_count = 0;
            colinfo[col - 1].hash_table = NULL;
        }
        n++;
    }
    TRACE("%s has %d columns\n", debugstr_w(szTableName), n);

    if (colinfo && n != maxcount)
    {
        ERR("missing column in table %s\n", debugstr_w(szTableName));
        msi_free_colinfo( colinfo, maxcount );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }
    table_calc_column_offsets( db, colinfo, n );
    *sz = n;
    return LIBMSI_RESULT_SUCCESS;
}

unsigned msi_create_table( LibmsiDatabase *db, const WCHAR *name, column_info *col_info,
                       LibmsiCondition persistent )
{
    enum StringPersistence string_persistence = (persistent) ? StringPersistent : StringNonPersistent;
    unsigned r, nField;
    LibmsiView *tv = NULL;
    LibmsiRecord *rec = NULL;
    column_info *col;
    LibmsiTable *table;
    unsigned i;

    /* only add tables that don't exist already */
    if( table_view_exists(db, name ) )
    {
        WARN("table %s exists\n", debugstr_w(name));
        return LIBMSI_RESULT_BAD_QUERY_SYNTAX;
    }

    table = msi_alloc( sizeof (LibmsiTable) + strlenW(name)*sizeof (WCHAR) );
    if( !table )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    table->ref_count = 1;
    table->row_count = 0;
    table->data = NULL;
    table->data_persistent = NULL;
    table->colinfo = NULL;
    table->col_count = 0;
    table->persistent = persistent;
    strcpyW( table->name, name );

    for( col = col_info; col; col = col->next )
        table->col_count++;

    table->colinfo = msi_alloc( table->col_count * sizeof(LibmsiColumnInfo) );
    if (!table->colinfo)
    {
        free_table( table );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    for( i = 0, col = col_info; col; i++, col = col->next )
    {
        unsigned table_id = _libmsi_add_string( db->strings, col->table, -1, 1, string_persistence );
        unsigned col_id = _libmsi_add_string( db->strings, col->column, -1, 1, string_persistence );

        table->colinfo[ i ].tablename = msi_string_lookup_id( db->strings, table_id );
        table->colinfo[ i ].number = i + 1;
        table->colinfo[ i ].colname = msi_string_lookup_id( db->strings, col_id );
        table->colinfo[ i ].type = col->type;
        table->colinfo[ i ].offset = 0;
        table->colinfo[ i ].ref_count = 0;
        table->colinfo[ i ].hash_table = NULL;
        table->colinfo[ i ].temporary = col->temporary;
    }
    table_calc_column_offsets( db, table->colinfo, table->col_count);

    r = table_view_create( db, szTables, &tv );
    TRACE("CreateView returned %x\n", r);
    if( r )
    {
        free_table( table );
        return r;
    }

    r = tv->ops->execute( tv, 0 );
    TRACE("tv execute returned %x\n", r);
    if( r )
        goto err;

    rec = libmsi_record_create( 1 );
    if( !rec )
        goto err;

    r = _libmsi_record_set_stringW( rec, 1, name );
    if( r )
        goto err;

    r = tv->ops->insert_row( tv, rec, -1, persistent == LIBMSI_CONDITION_FALSE );
    TRACE("insert_row returned %x\n", r);
    if( r )
        goto err;

    tv->ops->delete( tv );
    tv = NULL;

    msiobj_release( &rec->hdr );
    rec = NULL;

    if( persistent != LIBMSI_CONDITION_FALSE )
    {
        /* add each column to the _Columns table */
        r = table_view_create( db, szColumns, &tv );
        if( r )
            return r;

        r = tv->ops->execute( tv, 0 );
        TRACE("tv execute returned %x\n", r);
        if( r )
            goto err;

        rec = libmsi_record_create( 4 );
        if( !rec )
            goto err;

        r = _libmsi_record_set_stringW( rec, 1, name );
        if( r )
            goto err;

        /*
         * need to set the table, column number, col name and type
         * for each column we enter in the table
         */
        nField = 1;
        for( col = col_info; col; col = col->next )
        {
            r = libmsi_record_set_int( rec, 2, nField );
            if( r )
                goto err;

            r = _libmsi_record_set_stringW( rec, 3, col->column );
            if( r )
                goto err;

            r = libmsi_record_set_int( rec, 4, col->type );
            if( r )
                goto err;

            r = tv->ops->insert_row( tv, rec, -1, false );
            if( r )
                goto err;

            nField++;
        }
        if( !col )
            r = LIBMSI_RESULT_SUCCESS;
    }

err:
    if (rec)
        msiobj_release( &rec->hdr );
    /* FIXME: remove values from the string table on error */
    if( tv )
        tv->ops->delete( tv );

    if (r == LIBMSI_RESULT_SUCCESS)
        list_add_head( &db->tables, &table->entry );
    else
        free_table( table );

    return r;
}

static unsigned save_table( LibmsiDatabase *db, const LibmsiTable *t, unsigned bytes_per_strref )
{
    uint8_t *rawdata = NULL;
    unsigned rawsize, i, j, row_size, row_count;
    unsigned r = LIBMSI_RESULT_FUNCTION_FAILED;

    /* Nothing to do for non-persistent tables */
    if( t->persistent == LIBMSI_CONDITION_FALSE )
        return LIBMSI_RESULT_SUCCESS;

    TRACE("Saving %s\n", debugstr_w( t->name ) );

    row_size = msi_table_get_row_size( db, t->colinfo, t->col_count, bytes_per_strref );
    row_count = t->row_count;
    for (i = 0; i < t->row_count; i++)
    {
        if (!t->data_persistent[i])
        {
            row_count = 1; /* yes, this is bizarre */
            break;
        }
    }
    rawsize = row_count * row_size;
    rawdata = msi_alloc_zero( rawsize );
    if( !rawdata )
    {
        r = LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
        goto err;
    }

    rawsize = 0;
    for (i = 0; i < t->row_count; i++)
    {
        unsigned ofs = 0, ofs_mem = 0;

        if (!t->data_persistent[i]) break;

        for (j = 0; j < t->col_count; j++)
        {
            unsigned m = bytes_per_column( db, &t->colinfo[j], LONG_STR_BYTES );
            unsigned n = bytes_per_column( db, &t->colinfo[j], bytes_per_strref );
            unsigned k;

            if (n != 2 && n != 3 && n != 4)
            {
                ERR("oops - unknown column width %d\n", n);
                goto err;
            }
            if (t->colinfo[j].type & MSITYPE_STRING && n < m)
            {
                unsigned id = read_table_int( t->data, i, ofs_mem, LONG_STR_BYTES );
                if (id > 1 << bytes_per_strref * 8)
                {
                    ERR("string id %u out of range\n", id);
                    goto err;
                }
            }
            for (k = 0; k < n; k++)
            {
                rawdata[ofs * row_count + i * n + k] = t->data[i][ofs_mem + k];
            }
            ofs_mem += m;
            ofs += n;
        }
        rawsize += row_size;
    }

    TRACE("writing %d bytes\n", rawsize);
    r = write_stream_data( db->storage, t->name, rawdata, rawsize, true );

err:
    msi_free( rawdata );
    return r;
}

static void msi_update_table_columns( LibmsiDatabase *db, const WCHAR *name )
{
    LibmsiTable *table;
    unsigned size, offset, old_count;
    unsigned n;

    table = find_cached_table( db, name );
    old_count = table->col_count;
    msi_free_colinfo( table->colinfo, table->col_count );
    msi_free( table->colinfo );
    table->colinfo = NULL;

    table_get_column_info( db, name, &table->colinfo, &table->col_count );
    if (!table->col_count) return;

    size = msi_table_get_row_size( db, table->colinfo, table->col_count, LONG_STR_BYTES );
    offset = table->colinfo[table->col_count - 1].offset;

    for ( n = 0; n < table->row_count; n++ )
    {
        table->data[n] = msi_realloc( table->data[n], size );
        if (old_count < table->col_count)
            memset( &table->data[n][offset], 0, size - offset );
    }
}

/* try to find the table name in the _Tables table */
bool table_view_exists( LibmsiDatabase *db, const WCHAR *name )
{
    unsigned r, table_id, i;
    LibmsiTable *table;

    if( !strcmpW( name, szTables ) || !strcmpW( name, szColumns ) ||
        !strcmpW( name, szStreams ) || !strcmpW( name, szStorages ) )
        return true;

    r = _libmsi_id_from_stringW( db->strings, name, &table_id );
    if( r != LIBMSI_RESULT_SUCCESS )
    {
        TRACE("Couldn't find id for %s\n", debugstr_w(name));
        return false;
    }

    r = get_table( db, szTables, &table );
    if( r != LIBMSI_RESULT_SUCCESS )
    {
        ERR("table %s not available\n", debugstr_w(szTables));
        return false;
    }

    for( i = 0; i < table->row_count; i++ )
    {
        if( read_table_int( table->data, i, 0, LONG_STR_BYTES ) == table_id )
            return true;
    }

    return false;
}

/* below is the query interface to a table */

typedef struct LibmsiTableView
{
    LibmsiView        view;
    LibmsiDatabase   *db;
    LibmsiTable      *table;
    LibmsiColumnInfo *columns;
    unsigned           num_cols;
    unsigned           row_size;
    WCHAR          name[1];
} LibmsiTableView;

static unsigned table_view_fetch_int( LibmsiView *view, unsigned row, unsigned col, unsigned *val )
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    unsigned offset, n;

    if( !tv->table )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( (col==0) || (col>tv->num_cols) )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    /* how many rows are there ? */
    if( row >= tv->table->row_count )
        return LIBMSI_RESULT_NO_MORE_ITEMS;

    if( tv->columns[col-1].offset >= tv->row_size )
    {
        ERR("Stuffed up %d >= %d\n", tv->columns[col-1].offset, tv->row_size );
        ERR("%p %p\n", tv, tv->columns );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    n = bytes_per_column( tv->db, &tv->columns[col - 1], LONG_STR_BYTES );
    if (n != 2 && n != 3 && n != 4)
    {
        ERR("oops! what is %d bytes per column?\n", n );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    offset = tv->columns[col-1].offset;
    *val = read_table_int(tv->table->data, row, offset, n);

    /* TRACE("Data [%d][%d] = %d\n", row, col, *val ); */

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned msi_stream_name( const LibmsiTableView *tv, unsigned row, WCHAR **pstname )
{
    WCHAR *p;
    WCHAR *stname = NULL;
    unsigned i, r, type, ival;
    unsigned len;
    const WCHAR *sval;
    LibmsiView *view = (LibmsiView *) tv;

    TRACE("%p %d\n", tv, row);

    len = strlenW( tv->name ) + 1;
    stname = msi_alloc( len*sizeof(WCHAR) );
    if ( !stname )
    {
       r = LIBMSI_RESULT_OUTOFMEMORY;
       goto err;
    }

    strcpyW( stname, tv->name );

    for ( i = 0; i < tv->num_cols; i++ )
    {
        type = tv->columns[i].type;
        if ( type & MSITYPE_KEY )
        {
            WCHAR number[0x20];

            r = table_view_fetch_int( view, row, i+1, &ival );
            if ( r != LIBMSI_RESULT_SUCCESS )
                goto err;

            if ( tv->columns[i].type & MSITYPE_STRING )
            {
                sval = msi_string_lookup_id( tv->db->strings, ival );
                if ( !sval )
                {
                    r = LIBMSI_RESULT_INVALID_PARAMETER;
                    goto err;
                }
            }
            else
            {
                static const WCHAR fmt[] = { '%','d',0 };
                unsigned n = bytes_per_column( tv->db, &tv->columns[i], LONG_STR_BYTES );

                switch( n )
                {
                case 2:
                    sprintfW( number, fmt, ival-0x8000 );
                    break;
                case 4:
                    sprintfW( number, fmt, ival^0x80000000 );
                    break;
                default:
                    ERR( "oops - unknown column width %d\n", n );
                    r = LIBMSI_RESULT_FUNCTION_FAILED;
                    goto err;
                }
                sval = number;
            }

            len += strlenW( szDot ) + strlenW( sval );
            p = msi_realloc ( stname, len*sizeof(WCHAR) );
            if ( !p )
            {
                r = LIBMSI_RESULT_OUTOFMEMORY;
                goto err;
            }
            stname = p;

            strcatW( stname, szDot );
            strcatW( stname, sval );
        }
        else
           continue;
    }

    *pstname = stname;
    return LIBMSI_RESULT_SUCCESS;

err:
    msi_free( stname );
    *pstname = NULL;
    return r;
}

/*
 * We need a special case for streams, as we need to reference column with
 * the name of the stream in the same table, and the table name
 * which may not be available at higher levels of the query
 */
static unsigned table_view_fetch_stream( LibmsiView *view, unsigned row, unsigned col, IStream **stm )
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    unsigned r;
    WCHAR *encname;
    WCHAR *full_name = NULL;

    if( !view->ops->fetch_int )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    r = msi_stream_name( tv, row, &full_name );
    if ( r != LIBMSI_RESULT_SUCCESS )
    {
        ERR("fetching stream, error = %d\n", r);
        return r;
    }

    encname = encode_streamname( false, full_name );
    r = msi_get_raw_stream( tv->db, encname, stm );
    if( r )
        ERR("fetching stream %s, error = %d\n",debugstr_w(full_name), r);

    msi_free( full_name );
    msi_free( encname );
    return r;
}

static unsigned table_view_set_int( LibmsiTableView *tv, unsigned row, unsigned col, unsigned val )
{
    unsigned offset, n, i;

    if( !tv->table )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( (col==0) || (col>tv->num_cols) )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( row >= tv->table->row_count )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( tv->columns[col-1].offset >= tv->row_size )
    {
        ERR("Stuffed up %d >= %d\n", tv->columns[col-1].offset, tv->row_size );
        ERR("%p %p\n", tv, tv->columns );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    msi_free( tv->columns[col-1].hash_table );
    tv->columns[col-1].hash_table = NULL;

    n = bytes_per_column( tv->db, &tv->columns[col - 1], LONG_STR_BYTES );
    if ( n != 2 && n != 3 && n != 4 )
    {
        ERR("oops! what is %d bytes per column?\n", n );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    offset = tv->columns[col-1].offset;
    for ( i = 0; i < n; i++ )
        tv->table->data[row][offset + i] = (val >> i * 8) & 0xff;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned table_view_get_row( LibmsiView *view, unsigned row, LibmsiRecord **rec )
{
    LibmsiTableView *tv = (LibmsiTableView *)view;

    if (!tv->table)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    return msi_view_get_row(tv->db, view, row, rec);
}

static unsigned _libmsi_add_stream( LibmsiDatabase *db, const WCHAR *name, IStream *data )
{
    static const WCHAR insert[] = {
        'I','N','S','E','R','T',' ','I','N','T','O',' ',
        '`','_','S','t','r','e','a','m','s','`',' ',
        '(','`','N','a','m','e','`',',','`','D','a','t','a','`',')',' ',
        'V','A','L','U','E','S',' ','(','?',',','?',')',0};
    LibmsiQuery *query = NULL;
    LibmsiRecord *rec;
    unsigned r;

    TRACE("%p %s %p\n", db, debugstr_w(name), data);

    rec = libmsi_record_create( 2 );
    if ( !rec )
        return LIBMSI_RESULT_OUTOFMEMORY;

    r = _libmsi_record_set_stringW( rec, 1, name );
    if ( r != LIBMSI_RESULT_SUCCESS )
       goto err;

    r = _libmsi_record_set_IStream( rec, 2, data );
    if ( r != LIBMSI_RESULT_SUCCESS )
       goto err;

    r = _libmsi_database_open_query( db, insert, &query );
    if ( r != LIBMSI_RESULT_SUCCESS )
       goto err;

    r = _libmsi_query_execute( query, rec );

err:
    msiobj_release( &query->hdr );
    msiobj_release( &rec->hdr );
    return r;
}

static unsigned get_table_value_from_record( LibmsiTableView *tv, LibmsiRecord *rec, unsigned iField, unsigned *pvalue )
{
    LibmsiColumnInfo columninfo;
    unsigned r;

    if ( (iField <= 0) ||
         (iField > tv->num_cols) ||
          libmsi_record_is_null( rec, iField ) )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    columninfo = tv->columns[ iField - 1 ];

    if ( MSITYPE_IS_BINARY(columninfo.type) )
    {
        *pvalue = 1; /* refers to the first key column */
    }
    else if ( columninfo.type & MSITYPE_STRING )
    {
        const WCHAR *sval = _libmsi_record_get_string_raw( rec, iField );
        if (sval)
        {
            r = _libmsi_id_from_stringW(tv->db->strings, sval, pvalue);
            if (r != LIBMSI_RESULT_SUCCESS)
                return LIBMSI_RESULT_NOT_FOUND;
        }
        else *pvalue = 0;
    }
    else if ( bytes_per_column( tv->db, &columninfo, LONG_STR_BYTES ) == 2 )
    {
        *pvalue = 0x8000 + libmsi_record_get_integer( rec, iField );
        if ( *pvalue & 0xffff0000 )
        {
            ERR("field %u value %d out of range\n", iField, *pvalue - 0x8000);
            return LIBMSI_RESULT_FUNCTION_FAILED;
        }
    }
    else
    {
        int ival = libmsi_record_get_integer( rec, iField );
        *pvalue = ival ^ 0x80000000;
    }

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned table_view_set_row( LibmsiView *view, unsigned row, LibmsiRecord *rec, unsigned mask )
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    unsigned i, val, r = LIBMSI_RESULT_SUCCESS;

    if ( !tv->table )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    /* test if any of the mask bits are invalid */
    if ( mask >= (1<<tv->num_cols) )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    for ( i = 0; i < tv->num_cols; i++ )
    {
        bool persistent;

        /* only update the fields specified in the mask */
        if ( !(mask&(1<<i)) )
            continue;

        persistent = (tv->table->persistent != LIBMSI_CONDITION_FALSE) &&
                     (tv->table->data_persistent[row]);
        /* FIXME: should we allow updating keys? */

        val = 0;
        if ( !libmsi_record_is_null( rec, i + 1 ) )
        {
            r = get_table_value_from_record (tv, rec, i + 1, &val);
            if ( MSITYPE_IS_BINARY(tv->columns[ i ].type) )
            {
                IStream *stm;
                WCHAR *stname;

                if ( r != LIBMSI_RESULT_SUCCESS )
                    return LIBMSI_RESULT_FUNCTION_FAILED;

                r = _libmsi_record_get_IStream( rec, i + 1, &stm );
                if ( r != LIBMSI_RESULT_SUCCESS )
                    return r;

                r = msi_stream_name( tv, row, &stname );
                if ( r != LIBMSI_RESULT_SUCCESS )
                {
                    IStream_Release( stm );
                    return r;
                }

                r = _libmsi_add_stream( tv->db, stname, stm );
                IStream_Release( stm );
                msi_free ( stname );

                if ( r != LIBMSI_RESULT_SUCCESS )
                    return r;
            }
            else if ( tv->columns[i].type & MSITYPE_STRING )
            {
                unsigned x;

                if ( r != LIBMSI_RESULT_SUCCESS )
                {
                    const WCHAR *sval = _libmsi_record_get_string_raw( rec, i + 1 );
                    val = _libmsi_add_string( tv->db->strings, sval, -1, 1,
                      persistent ? StringPersistent : StringNonPersistent );
                }
                else
                {
                    table_view_fetch_int(&tv->view, row, i + 1, &x);
                    if (val == x)
                        continue;
                }
            }
            else
            {
                if ( r != LIBMSI_RESULT_SUCCESS )
                    return LIBMSI_RESULT_FUNCTION_FAILED;
            }
        }

        r = table_view_set_int( tv, row, i+1, val );
        if ( r != LIBMSI_RESULT_SUCCESS )
            break;
    }
    return r;
}

static unsigned table_create_new_row( LibmsiView *view, unsigned *num, bool temporary )
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    uint8_t **p, *row;
    bool *b;
    unsigned sz;
    uint8_t ***data_ptr;
    bool **data_persist_ptr;
    unsigned *row_count;

    TRACE("%p %s\n", view, temporary ? "true" : "false");

    if( !tv->table )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    row = msi_alloc_zero( tv->row_size );
    if( !row )
        return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;

    row_count = &tv->table->row_count;
    data_ptr = &tv->table->data;
    data_persist_ptr = &tv->table->data_persistent;
    if (*num == -1)
        *num = tv->table->row_count;

    sz = (*row_count + 1) * sizeof (uint8_t*);
    if( *data_ptr )
        p = msi_realloc( *data_ptr, sz );
    else
        p = msi_alloc( sz );
    if( !p )
    {
        msi_free( row );
        return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
    }

    sz = (*row_count + 1) * sizeof (bool);
    if( *data_persist_ptr )
        b = msi_realloc( *data_persist_ptr, sz );
    else
        b = msi_alloc( sz );
    if( !b )
    {
        msi_free( row );
        msi_free( p );
        return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
    }

    *data_ptr = p;
    (*data_ptr)[*row_count] = row;

    *data_persist_ptr = b;
    (*data_persist_ptr)[*row_count] = !temporary;

    (*row_count)++;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned table_view_execute( LibmsiView *view, LibmsiRecord *record )
{
    LibmsiTableView *tv = (LibmsiTableView*)view;

    TRACE("%p %p\n", tv, record);

    TRACE("There are %d columns\n", tv->num_cols );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned table_view_close( LibmsiView *view )
{
    TRACE("%p\n", view );
    
    return LIBMSI_RESULT_SUCCESS;
}

static unsigned table_view_get_dimensions( LibmsiView *view, unsigned *rows, unsigned *cols)
{
    LibmsiTableView *tv = (LibmsiTableView*)view;

    TRACE("%p %p %p\n", view, rows, cols );

    if( cols )
        *cols = tv->num_cols;
    if( rows )
    {
        if( !tv->table )
            return LIBMSI_RESULT_INVALID_PARAMETER;
        *rows = tv->table->row_count;
    }

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned table_view_get_column_info( LibmsiView *view,
                unsigned n, const WCHAR **name, unsigned *type, bool *temporary,
                const WCHAR **table_name )
{
    LibmsiTableView *tv = (LibmsiTableView*)view;

    TRACE("%p %d %p %p\n", tv, n, name, type );

    if( ( n == 0 ) || ( n > tv->num_cols ) )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( name )
    {
        *name = tv->columns[n-1].colname;
        if( !*name )
            return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    if( table_name )
    {
        *table_name = tv->columns[n-1].tablename;
        if( !*table_name )
            return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    if( type )
        *type = tv->columns[n-1].type;

    if( temporary )
        *temporary = tv->columns[n-1].temporary;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned msi_table_find_row( LibmsiTableView *tv, LibmsiRecord *rec, unsigned *row, unsigned *column );

static unsigned table_validate_new( LibmsiTableView *tv, LibmsiRecord *rec, unsigned *column )
{
    unsigned r, row, i;

    /* check there's no null values where they're not allowed */
    for( i = 0; i < tv->num_cols; i++ )
    {
        if ( tv->columns[i].type & MSITYPE_NULLABLE )
            continue;

        if ( MSITYPE_IS_BINARY(tv->columns[i].type) )
            TRACE("skipping binary column\n");
        else if ( tv->columns[i].type & MSITYPE_STRING )
        {
            const WCHAR *str;

            str = _libmsi_record_get_string_raw( rec, i+1 );
            if (str == NULL || str[0] == 0)
            {
                if (column) *column = i;
                return LIBMSI_RESULT_INVALID_DATA;
            }
        }
        else
        {
            unsigned n;

            n = libmsi_record_get_integer( rec, i+1 );
            if (n == MSI_NULL_INTEGER)
            {
                if (column) *column = i;
                return LIBMSI_RESULT_INVALID_DATA;
            }
        }
    }

    /* check there's no duplicate keys */
    r = msi_table_find_row( tv, rec, &row, column );
    if (r == LIBMSI_RESULT_SUCCESS)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    return LIBMSI_RESULT_SUCCESS;
}

static int compare_record( LibmsiTableView *tv, unsigned row, LibmsiRecord *rec )
{
    unsigned r, i, ivalue, x;

    for (i = 0; i < tv->num_cols; i++ )
    {
        if (!(tv->columns[i].type & MSITYPE_KEY)) continue;

        r = get_table_value_from_record( tv, rec, i + 1, &ivalue );
        if (r != LIBMSI_RESULT_SUCCESS)
            return 1;

        r = table_view_fetch_int( &tv->view, row, i + 1, &x );
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            WARN("table_view_fetch_int should not fail here %u\n", r);
            return -1;
        }
        if (ivalue > x)
        {
            return 1;
        }
        else if (ivalue == x)
        {
            if (i < tv->num_cols - 1) continue;
            return 0;
        }
        else
            return -1;
    }
    return 1;
}

static int find_insert_index( LibmsiTableView *tv, LibmsiRecord *rec )
{
    int idx, c, low = 0, high = tv->table->row_count - 1;

    TRACE("%p %p\n", tv, rec);

    while (low <= high)
    {
        idx = (low + high) / 2;
        c = compare_record( tv, idx, rec );

        if (c < 0)
            high = idx - 1;
        else if (c > 0)
            low = idx + 1;
        else
        {
            TRACE("found %u\n", idx);
            return idx;
        }
    }
    TRACE("found %u\n", high + 1);
    return high + 1;
}

static unsigned table_view_insert_row( LibmsiView *view, LibmsiRecord *rec, unsigned row, bool temporary )
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    unsigned i, r;

    TRACE("%p %p %s\n", tv, rec, temporary ? "true" : "false" );

    /* check that the key is unique - can we find a matching row? */
    r = table_validate_new( tv, rec, NULL );
    if( r != LIBMSI_RESULT_SUCCESS )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    if (row == -1)
        row = find_insert_index( tv, rec );

    r = table_create_new_row( view, &row, temporary );
    TRACE("insert_row returned %08x\n", r);
    if( r != LIBMSI_RESULT_SUCCESS )
        return r;

    /* shift the rows to make room for the new row */
    for (i = tv->table->row_count - 1; i > row; i--)
    {
        memmove(&(tv->table->data[i][0]),
                &(tv->table->data[i - 1][0]), tv->row_size);
        tv->table->data_persistent[i] = tv->table->data_persistent[i - 1];
    }

    /* Re-set the persistence flag */
    tv->table->data_persistent[row] = !temporary;
    return table_view_set_row( view, row, rec, (1<<tv->num_cols) - 1 );
}

static unsigned table_view_delete_row( LibmsiView *view, unsigned row )
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    unsigned r, num_rows, num_cols, i;

    TRACE("%p %d\n", tv, row);

    if ( !tv->table )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    r = table_view_get_dimensions( view, &num_rows, &num_cols );
    if ( r != LIBMSI_RESULT_SUCCESS )
        return r;

    if ( row >= num_rows )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    num_rows = tv->table->row_count;
    tv->table->row_count--;

    /* reset the hash tables */
    for (i = 0; i < tv->num_cols; i++)
    {
        msi_free( tv->columns[i].hash_table );
        tv->columns[i].hash_table = NULL;
    }

    for (i = row + 1; i < num_rows; i++)
    {
        memcpy(tv->table->data[i - 1], tv->table->data[i], tv->row_size);
        tv->table->data_persistent[i - 1] = tv->table->data_persistent[i];
    }

    msi_free(tv->table->data[num_rows - 1]);

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned table_view_delete( LibmsiView *view )
{
    LibmsiTableView *tv = (LibmsiTableView*)view;

    TRACE("%p\n", view );

    tv->table = NULL;
    tv->columns = NULL;

    msi_free( tv );

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned table_view_find_matching_rows( LibmsiView *view, unsigned col,
    unsigned val, unsigned *row, MSIITERHANDLE *handle )
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    const LibmsiColumnHashEntry *entry;

    TRACE("%p, %d, %u, %p\n", view, col, val, *handle);

    if( !tv->table )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( (col==0) || (col > tv->num_cols) )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( !tv->columns[col-1].hash_table )
    {
        unsigned i;
        unsigned num_rows = tv->table->row_count;
        LibmsiColumnHashEntry **hash_table;
        LibmsiColumnHashEntry *new_entry;

        if( tv->columns[col-1].offset >= tv->row_size )
        {
            ERR("Stuffed up %d >= %d\n", tv->columns[col-1].offset, tv->row_size );
            ERR("%p %p\n", tv, tv->columns );
            return LIBMSI_RESULT_FUNCTION_FAILED;
        }

        /* allocate contiguous memory for the table and its entries so we
         * don't have to do an expensive cleanup */
        hash_table = msi_alloc(LibmsiTable_HASH_TABLE_SIZE * sizeof(LibmsiColumnHashEntry*) +
            num_rows * sizeof(LibmsiColumnHashEntry));
        if (!hash_table)
            return LIBMSI_RESULT_OUTOFMEMORY;

        memset(hash_table, 0, LibmsiTable_HASH_TABLE_SIZE * sizeof(LibmsiColumnHashEntry*));
        tv->columns[col-1].hash_table = hash_table;

        new_entry = (LibmsiColumnHashEntry *)(hash_table + LibmsiTable_HASH_TABLE_SIZE);

        for (i = 0; i < num_rows; i++, new_entry++)
        {
            unsigned row_value;

            if (view->ops->fetch_int( view, i, col, &row_value ) != LIBMSI_RESULT_SUCCESS)
                continue;

            new_entry->next = NULL;
            new_entry->value = row_value;
            new_entry->row = i;
            if (hash_table[row_value % LibmsiTable_HASH_TABLE_SIZE])
            {
                LibmsiColumnHashEntry *prev_entry = hash_table[row_value % LibmsiTable_HASH_TABLE_SIZE];
                while (prev_entry->next)
                    prev_entry = prev_entry->next;
                prev_entry->next = new_entry;
            }
            else
                hash_table[row_value % LibmsiTable_HASH_TABLE_SIZE] = new_entry;
        }
    }

    if( !*handle )
        entry = tv->columns[col-1].hash_table[val % LibmsiTable_HASH_TABLE_SIZE];
    else
        entry = (*handle)->next;

    while (entry && entry->value != val)
        entry = entry->next;

    *handle = entry;
    if (!entry)
        return LIBMSI_RESULT_NO_MORE_ITEMS;

    *row = entry->row;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned table_view_add_ref(LibmsiView *view)
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    unsigned i;

    TRACE("%p %d\n", view, tv->table->ref_count);

    for (i = 0; i < tv->table->col_count; i++)
    {
        if (tv->table->colinfo[i].type & MSITYPE_TEMPORARY)
            InterlockedIncrement(&tv->table->colinfo[i].ref_count);
    }

    return InterlockedIncrement(&tv->table->ref_count);
}

static unsigned table_view_remove_column(LibmsiView *view, const WCHAR *table, unsigned number)
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    LibmsiRecord *rec = NULL;
    LibmsiView *columns = NULL;
    unsigned row, r;

    rec = libmsi_record_create(2);
    if (!rec)
        return LIBMSI_RESULT_OUTOFMEMORY;

    _libmsi_record_set_stringW(rec, 1, table);
    libmsi_record_set_int(rec, 2, number);

    r = table_view_create(tv->db, szColumns, &columns);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = msi_table_find_row((LibmsiTableView *)columns, rec, &row, NULL);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    r = table_view_delete_row(columns, row);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    msi_update_table_columns(tv->db, table);

done:
    msiobj_release(&rec->hdr);
    columns->ops->delete(columns);
    return r;
}

static unsigned table_view_release(LibmsiView *view)
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    int ref = tv->table->ref_count;
    unsigned i, r;

    TRACE("%p %d\n", view, ref);

    for (i = 0; i < tv->table->col_count; i++)
    {
        if (tv->table->colinfo[i].type & MSITYPE_TEMPORARY)
        {
            ref = InterlockedDecrement(&tv->table->colinfo[i].ref_count);
            if (ref == 0)
            {
                r = table_view_remove_column(view, tv->table->colinfo[i].tablename,
                                        tv->table->colinfo[i].number);
                if (r != LIBMSI_RESULT_SUCCESS)
                    break;
            }
        }
    }

    ref = InterlockedDecrement(&tv->table->ref_count);
    if (ref == 0)
    {
        if (!tv->table->row_count)
        {
            list_remove(&tv->table->entry);
            free_table(tv->table);
            table_view_delete(view);
        }
    }

    return ref;
}

static unsigned table_view_add_column(LibmsiView *view, const WCHAR *table, unsigned number,
                             const WCHAR *column, unsigned type, bool hold)
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    LibmsiTable *msitable;
    LibmsiRecord *rec;
    unsigned r, i;

    rec = libmsi_record_create(4);
    if (!rec)
        return LIBMSI_RESULT_OUTOFMEMORY;

    _libmsi_record_set_stringW(rec, 1, table);
    libmsi_record_set_int(rec, 2, number);
    _libmsi_record_set_stringW(rec, 3, column);
    libmsi_record_set_int(rec, 4, type);

    r = table_view_insert_row(&tv->view, rec, -1, false);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    msi_update_table_columns(tv->db, table);

    if (!hold)
        goto done;

    msitable = find_cached_table(tv->db, table);
    for (i = 0; i < msitable->col_count; i++)
    {
        if (!strcmpW( msitable->colinfo[i].colname, column ))
        {
            InterlockedIncrement(&msitable->colinfo[i].ref_count);
            break;
        }
    }

done:
    msiobj_release(&rec->hdr);
    return r;
}

static unsigned table_view_drop(LibmsiView *view)
{
    LibmsiTableView *tv = (LibmsiTableView*)view;
    LibmsiView *tables = NULL;
    LibmsiRecord *rec = NULL;
    unsigned r, row;
    int i;

    TRACE("dropping table %s\n", debugstr_w(tv->name));

    for (i = tv->table->col_count - 1; i >= 0; i--)
    {
        r = table_view_remove_column(view, tv->table->colinfo[i].tablename,
                                tv->table->colinfo[i].number);
        if (r != LIBMSI_RESULT_SUCCESS)
            return r;
    }

    rec = libmsi_record_create(1);
    if (!rec)
        return LIBMSI_RESULT_OUTOFMEMORY;

    _libmsi_record_set_stringW(rec, 1, tv->name);

    r = table_view_create(tv->db, szTables, &tables);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = msi_table_find_row((LibmsiTableView *)tables, rec, &row, NULL);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    r = table_view_delete_row(tables, row);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    list_remove(&tv->table->entry);
    free_table(tv->table);

done:
    msiobj_release(&rec->hdr);
    tables->ops->delete(tables);

    return r;
}

static const LibmsiViewOps table_ops =
{
    table_view_fetch_int,
    table_view_fetch_stream,
    table_view_get_row,
    table_view_set_row,
    table_view_insert_row,
    table_view_delete_row,
    table_view_execute,
    table_view_close,
    table_view_get_dimensions,
    table_view_get_column_info,
    table_view_delete,
    table_view_find_matching_rows,
    table_view_add_ref,
    table_view_release,
    table_view_add_column,
    table_view_remove_column,
    NULL,
    table_view_drop,
};

unsigned table_view_create( LibmsiDatabase *db, const WCHAR *name, LibmsiView **view )
{
    LibmsiTableView *tv ;
    unsigned r, sz;

    TRACE("%p %s %p\n", db, debugstr_w(name), view );

    if ( !strcmpW( name, szStreams ) )
        return streams_view_create( db, view );
    else if ( !strcmpW( name, szStorages ) )
        return storages_view_create( db, view );

    sz = sizeof *tv + strlenW(name)*sizeof name[0] ;
    tv = alloc_msiobject( sz, NULL );
    if( !tv )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    r = get_table( db, name, &tv->table );
    if( r != LIBMSI_RESULT_SUCCESS )
    {
        msi_free( tv );
        WARN("table not found\n");
        return r;
    }

    TRACE("table %p found with %d columns\n", tv->table, tv->table->col_count);

    /* fill the structure */
    tv->view.ops = &table_ops;
    tv->db = db;
    tv->columns = tv->table->colinfo;
    tv->num_cols = tv->table->col_count;
    tv->row_size = msi_table_get_row_size( db, tv->table->colinfo, tv->table->col_count, LONG_STR_BYTES );

    TRACE("%s one row is %d bytes\n", debugstr_w(name), tv->row_size );

    *view = (LibmsiView*) tv;
    strcpyW( tv->name, name );

    return LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_database_commit_tables( LibmsiDatabase *db )
{
    unsigned r, bytes_per_strref;
    HRESULT hr;
    LibmsiTable *table = NULL;

    TRACE("%p\n",db);

    r = msi_save_string_table( db->strings, db->storage, &bytes_per_strref );
    if( r != LIBMSI_RESULT_SUCCESS )
    {
        WARN("failed to save string table r=%08x\n",r);
        return r;
    }

    LIST_FOR_EACH_ENTRY( table, &db->tables, LibmsiTable, entry )
    {
        r = save_table( db, table, bytes_per_strref );
        if( r != LIBMSI_RESULT_SUCCESS )
        {
            WARN("failed to save table %s (r=%08x)\n",
                  debugstr_w(table->name), r);
            return r;
        }
    }

    hr = IStorage_Commit( db->storage, 0 );
    if (FAILED( hr ))
    {
        WARN("failed to commit changes 0x%08x\n", hr);
        r = LIBMSI_RESULT_FUNCTION_FAILED;
    }
    return r;
}

LibmsiCondition _libmsi_database_is_table_persistent( LibmsiDatabase *db, const WCHAR *table )
{
    LibmsiTable *t;
    unsigned r;

    TRACE("%p %s\n", db, debugstr_w(table));

    if (!table)
        return LIBMSI_CONDITION_ERROR;

    r = get_table( db, table, &t );
    if (r != LIBMSI_RESULT_SUCCESS)
        return LIBMSI_CONDITION_NONE;

    return t->persistent;
}

static unsigned read_raw_int(const uint8_t *data, unsigned col, unsigned bytes)
{
    unsigned ret = 0, i;

    for (i = 0; i < bytes; i++)
        ret += (data[col + i] << i * 8);

    return ret;
}

static unsigned msi_record_encoded_stream_name( const LibmsiTableView *tv, LibmsiRecord *rec, WCHAR **pstname )
{
    WCHAR *stname = NULL;
    WCHAR *sval;
    WCHAR *p;
    unsigned len;
    unsigned i, r;

    TRACE("%p %p\n", tv, rec);

    len = strlenW( tv->name ) + 1;
    stname = msi_alloc( len*sizeof(WCHAR) );
    if ( !stname )
    {
       r = LIBMSI_RESULT_OUTOFMEMORY;
       goto err;
    }

    strcpyW( stname, tv->name );

    for ( i = 0; i < tv->num_cols; i++ )
    {
        if ( tv->columns[i].type & MSITYPE_KEY )
        {
            sval = msi_dup_record_field( rec, i + 1 );
            if ( !sval )
            {
                r = LIBMSI_RESULT_OUTOFMEMORY;
                goto err;
            }

            len += strlenW( szDot ) + strlenW ( sval );
            p = msi_realloc ( stname, len*sizeof(WCHAR) );
            if ( !p )
            {
                r = LIBMSI_RESULT_OUTOFMEMORY;
                goto err;
            }
            stname = p;

            strcatW( stname, szDot );
            strcatW( stname, sval );

            msi_free( sval );
        }
        else
            continue;
    }

    *pstname = encode_streamname( false, stname );
    msi_free( stname );

    return LIBMSI_RESULT_SUCCESS;

err:
    msi_free ( stname );
    *pstname = NULL;
    return r;
}

static LibmsiRecord *msi_get_transform_record( const LibmsiTableView *tv, const string_table *st,
                                            IStorage *stg,
                                            const uint8_t *rawdata, unsigned bytes_per_strref )
{
    unsigned i, val, ofs = 0;
    uint16_t mask;
    LibmsiColumnInfo *columns = tv->columns;
    LibmsiRecord *rec;

    mask = rawdata[0] | (rawdata[1] << 8);
    rawdata += 2;

    rec = libmsi_record_create( tv->num_cols );
    if( !rec )
        return rec;

    TRACE("row ->\n");
    for( i=0; i<tv->num_cols; i++ )
    {
        if ( (mask&1) && (i>=(mask>>8)) )
            break;
        /* all keys must be present */
        if ( (~mask&1) && (~columns[i].type & MSITYPE_KEY) && ((1<<i) & ~mask) )
            continue;

        if( MSITYPE_IS_BINARY(tv->columns[i].type) )
        {
            WCHAR *encname;
            IStream *stm = NULL;
            unsigned r;

            ofs += bytes_per_column( tv->db, &columns[i], bytes_per_strref );

            r = msi_record_encoded_stream_name( tv, rec, &encname );
            if ( r != LIBMSI_RESULT_SUCCESS )
                return NULL;

            r = IStorage_OpenStream( stg, encname, NULL,
                     STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stm );
            if ( r != LIBMSI_RESULT_SUCCESS )
            {
                msi_free( encname );
                return NULL;
            }

            _libmsi_record_load_stream( rec, i+1, stm );
            TRACE(" field %d [%s]\n", i+1, debugstr_w(encname));
            msi_free( encname );
        }
        else if( columns[i].type & MSITYPE_STRING )
        {
            const WCHAR *sval;

            val = read_raw_int(rawdata, ofs, bytes_per_strref);
            sval = msi_string_lookup_id( st, val );
            _libmsi_record_set_stringW( rec, i+1, sval );
            TRACE(" field %d [%s]\n", i+1, debugstr_w(sval));
            ofs += bytes_per_strref;
        }
        else
        {
            unsigned n = bytes_per_column( tv->db, &columns[i], bytes_per_strref );
            switch( n )
            {
            case 2:
                val = read_raw_int(rawdata, ofs, n);
                if (val)
                    libmsi_record_set_int( rec, i+1, val-0x8000 );
                TRACE(" field %d [0x%04x]\n", i+1, val );
                break;
            case 4:
                val = read_raw_int(rawdata, ofs, n);
                if (val)
                    libmsi_record_set_int( rec, i+1, val^0x80000000 );
                TRACE(" field %d [0x%08x]\n", i+1, val );
                break;
            default:
                ERR("oops - unknown column width %d\n", n);
                break;
            }
            ofs += n;
        }
    }
    return rec;
}

static void dump_record( LibmsiRecord *rec )
{
    unsigned i, n;

    n = libmsi_record_get_field_count( rec );
    for( i=1; i<=n; i++ )
    {
        const WCHAR *sval;

        if( libmsi_record_is_null( rec, i ) )
            TRACE("row -> []\n");
        else if( (sval = _libmsi_record_get_string_raw( rec, i )) )
            TRACE("row -> [%s]\n", debugstr_w(sval));
        else
            TRACE("row -> [0x%08x]\n", libmsi_record_get_integer( rec, i ) );
    }
}

static void dump_table( const string_table *st, const uint16_t *rawdata, unsigned rawsize )
{
    const WCHAR *sval;
    unsigned i;

    for( i=0; i<(rawsize/2); i++ )
    {
        sval = msi_string_lookup_id( st, rawdata[i] );
        TRACE(" %04x %s\n", rawdata[i], debugstr_w(sval) );
    }
}

static unsigned* msi_record_to_row( const LibmsiTableView *tv, LibmsiRecord *rec )
{
    const WCHAR *str;
    unsigned i, r, *data;

    data = msi_alloc( tv->num_cols *sizeof (unsigned) );
    for( i=0; i<tv->num_cols; i++ )
    {
        data[i] = 0;

        if ( ~tv->columns[i].type & MSITYPE_KEY )
            continue;

        /* turn the transform column value into a row value */
        if ( ( tv->columns[i].type & MSITYPE_STRING ) &&
             ! MSITYPE_IS_BINARY(tv->columns[i].type) )
        {
            str = _libmsi_record_get_string_raw( rec, i+1 );
            if (str)
            {
                r = _libmsi_id_from_stringW( tv->db->strings, str, &data[i] );

                /* if there's no matching string in the string table,
                   these keys can't match any record, so fail now. */
                if (r != LIBMSI_RESULT_SUCCESS)
                {
                    msi_free( data );
                    return NULL;
                }
            }
            else data[i] = 0;
        }
        else
        {
            data[i] = libmsi_record_get_integer( rec, i+1 );

            if (data[i] == MSI_NULL_INTEGER)
                data[i] = 0;
            else if ((tv->columns[i].type&0xff) == 2)
                data[i] += 0x8000;
            else
                data[i] += 0x80000000;
        }
    }
    return data;
}

static unsigned msi_row_matches( LibmsiTableView *tv, unsigned row, const unsigned *data, unsigned *column )
{
    unsigned i, r, x, ret = LIBMSI_RESULT_FUNCTION_FAILED;

    for( i=0; i<tv->num_cols; i++ )
    {
        if ( ~tv->columns[i].type & MSITYPE_KEY )
            continue;

        /* turn the transform column value into a row value */
        r = table_view_fetch_int( &tv->view, row, i+1, &x );
        if ( r != LIBMSI_RESULT_SUCCESS )
        {
            ERR("table_view_fetch_int shouldn't fail here\n");
            break;
        }

        /* if this key matches, move to the next column */
        if ( x != data[i] )
        {
            ret = LIBMSI_RESULT_FUNCTION_FAILED;
            break;
        }
        if (column) *column = i;
        ret = LIBMSI_RESULT_SUCCESS;
    }
    return ret;
}

static unsigned msi_table_find_row( LibmsiTableView *tv, LibmsiRecord *rec, unsigned *row, unsigned *column )
{
    unsigned i, r = LIBMSI_RESULT_FUNCTION_FAILED, *data;

    data = msi_record_to_row( tv, rec );
    if( !data )
        return r;
    for( i = 0; i < tv->table->row_count; i++ )
    {
        r = msi_row_matches( tv, i, data, column );
        if( r == LIBMSI_RESULT_SUCCESS )
        {
            *row = i;
            break;
        }
    }
    msi_free( data );
    return r;
}

typedef struct
{
    struct list entry;
    WCHAR *name;
} TRANSFORMDATA;

static unsigned msi_table_load_transform( LibmsiDatabase *db, IStorage *stg,
                                      string_table *st, TRANSFORMDATA *transform,
                                      unsigned bytes_per_strref )
{
    uint8_t *rawdata = NULL;
    LibmsiTableView *tv = NULL;
    unsigned r, n, sz, i, mask, num_cols, colcol = 0, rawsize = 0;
    LibmsiRecord *rec = NULL;
    WCHAR coltable[32];
    const WCHAR *name;

    if (!transform)
        return LIBMSI_RESULT_SUCCESS;

    name = transform->name;

    coltable[0] = 0;
    TRACE("%p %p %p %s\n", db, stg, st, debugstr_w(name) );

    /* read the transform data */
    read_stream_data( stg, name, true, &rawdata, &rawsize );
    if ( !rawdata )
    {
        TRACE("table %s empty\n", debugstr_w(name) );
        return LIBMSI_RESULT_INVALID_TABLE;
    }

    /* create a table view */
    r = table_view_create( db, name, (LibmsiView**) &tv );
    if( r != LIBMSI_RESULT_SUCCESS )
        goto err;

    r = tv->view.ops->execute( &tv->view, NULL );
    if( r != LIBMSI_RESULT_SUCCESS )
        goto err;

    TRACE("name = %s columns = %u row_size = %u raw size = %u\n",
          debugstr_w(name), tv->num_cols, tv->row_size, rawsize );

    /* interpret the data */
    for (n = 0; n < rawsize;)
    {
        mask = rawdata[n] | (rawdata[n + 1] << 8);
        if (mask & 1)
        {
            /*
             * if the low bit is set, columns are continuous and
             * the number of columns is specified in the high byte
             */
            sz = 2;
            num_cols = mask >> 8;
            for (i = 0; i < num_cols; i++)
            {
                if( (tv->columns[i].type & MSITYPE_STRING) &&
                    ! MSITYPE_IS_BINARY(tv->columns[i].type) )
                    sz += bytes_per_strref;
                else
                    sz += bytes_per_column( tv->db, &tv->columns[i], bytes_per_strref );
            }
        }
        else
        {
            /*
             * If the low bit is not set, mask is a bitmask.
             * Excepting for key fields, which are always present,
             *  each bit indicates that a field is present in the transform record.
             *
             * mask == 0 is a special case ... only the keys will be present
             * and it means that this row should be deleted.
             */
            sz = 2;
            num_cols = tv->num_cols;
            for (i = 0; i < num_cols; i++)
            {
                if ((tv->columns[i].type & MSITYPE_KEY) || ((1 << i) & mask))
                {
                    if ((tv->columns[i].type & MSITYPE_STRING) &&
                        !MSITYPE_IS_BINARY(tv->columns[i].type))
                        sz += bytes_per_strref;
                    else
                        sz += bytes_per_column( tv->db, &tv->columns[i], bytes_per_strref );
                }
            }
        }

        /* check we didn't run of the end of the table */
        if (n + sz > rawsize)
        {
            ERR("borked.\n");
            dump_table( st, (uint16_t *)rawdata, rawsize );
            break;
        }

        rec = msi_get_transform_record( tv, st, stg, &rawdata[n], bytes_per_strref );
        if (rec)
        {
            WCHAR table[32];
            unsigned sz = 32;
            unsigned number = MSI_NULL_INTEGER;
            unsigned row = 0;

            if (!strcmpW( name, szColumns ))
            {
                _libmsi_record_get_stringW( rec, 1, table, &sz );
                number = libmsi_record_get_integer( rec, 2 );

                /*
                 * Native msi seems writes nul into the Number (2nd) column of
                 * the _Columns table, only when the columns are from a new table
                 */
                if ( number == MSI_NULL_INTEGER )
                {
                    /* reset the column number on a new table */
                    if (strcmpW( coltable, table ))
                    {
                        colcol = 0;
                        strcpyW( coltable, table );
                    }

                    /* fix nul column numbers */
                    libmsi_record_set_int( rec, 2, ++colcol );
                }
            }

            if (TRACE_ON(msidb)) dump_record( rec );

            r = msi_table_find_row( tv, rec, &row, NULL );
            if (r == LIBMSI_RESULT_SUCCESS)
            {
                if (!mask)
                {
                    TRACE("deleting row [%d]:\n", row);
                    r = table_view_delete_row( &tv->view, row );
                    if (r != LIBMSI_RESULT_SUCCESS)
                        WARN("failed to delete row %u\n", r);
                }
                else if (mask & 1)
                {
                    TRACE("modifying full row [%d]:\n", row);
                    r = table_view_set_row( &tv->view, row, rec, (1 << tv->num_cols) - 1 );
                    if (r != LIBMSI_RESULT_SUCCESS)
                        WARN("failed to modify row %u\n", r);
                }
                else
                {
                    TRACE("modifying masked row [%d]:\n", row);
                    r = table_view_set_row( &tv->view, row, rec, mask );
                    if (r != LIBMSI_RESULT_SUCCESS)
                        WARN("failed to modify row %u\n", r);
                }
            }
            else
            {
                TRACE("inserting row\n");
                r = table_view_insert_row( &tv->view, rec, -1, false );
                if (r != LIBMSI_RESULT_SUCCESS)
                    WARN("failed to insert row %u\n", r);
            }

            if (number != MSI_NULL_INTEGER && !strcmpW( name, szColumns ))
                msi_update_table_columns( db, table );

            msiobj_release( &rec->hdr );
        }

        n += sz;
    }

err:
    /* no need to free the table, it's associated with the database */
    msi_free( rawdata );
    if( tv )
        tv->view.ops->delete( &tv->view );

    return LIBMSI_RESULT_SUCCESS;
}

/*
 * msi_table_apply_transform
 *
 * Enumerate the table transforms in a transform storage and apply each one.
 */
unsigned msi_table_apply_transform( LibmsiDatabase *db, IStorage *stg )
{
    struct list transforms;
    IEnumSTATSTG *stgenum = NULL;
    TRANSFORMDATA *transform;
    TRANSFORMDATA *tables = NULL, *columns = NULL;
    HRESULT r;
    STATSTG stat;
    string_table *strings;
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    unsigned bytes_per_strref;

    TRACE("%p %p\n", db, stg );

    strings = msi_load_string_table( stg, &bytes_per_strref );
    if( !strings )
        goto end;

    r = IStorage_EnumElements( stg, 0, NULL, 0, &stgenum );
    if( FAILED( r ) )
        goto end;

    list_init(&transforms);

    while ( true )
    {
        LibmsiTableView *tv = NULL;
        WCHAR name[0x40];
        unsigned count = 0;

        r = IEnumSTATSTG_Next( stgenum, 1, &stat, &count );
        if ( FAILED( r ) || !count )
            break;

        decode_streamname( stat.pwcsName, name );
        CoTaskMemFree( stat.pwcsName );
        if ( name[0] != 0x4840 )
            continue;

        if ( !strcmpW( name+1, szStringPool ) ||
             !strcmpW( name+1, szStringData ) )
            continue;

        transform = msi_alloc_zero( sizeof(TRANSFORMDATA) );
        if ( !transform )
            break;

        list_add_tail( &transforms, &transform->entry );

        transform->name = strdupW( name + 1 );

        if ( !strcmpW( transform->name, szTables ) )
            tables = transform;
        else if (!strcmpW( transform->name, szColumns ) )
            columns = transform;

        TRACE("transform contains stream %s\n", debugstr_w(name));

        /* load the table */
        r = table_view_create( db, transform->name, (LibmsiView**) &tv );
        if( r != LIBMSI_RESULT_SUCCESS )
            continue;

        r = tv->view.ops->execute( &tv->view, NULL );
        if( r != LIBMSI_RESULT_SUCCESS )
        {
            tv->view.ops->delete( &tv->view );
            continue;
        }

        tv->view.ops->delete( &tv->view );
    }

    /*
     * Apply _Tables and _Columns transforms first so that
     * the table metadata is correct, and empty tables exist.
     */
    ret = msi_table_load_transform( db, stg, strings, tables, bytes_per_strref );
    if (ret != LIBMSI_RESULT_SUCCESS && ret != LIBMSI_RESULT_INVALID_TABLE)
        goto end;

    ret = msi_table_load_transform( db, stg, strings, columns, bytes_per_strref );
    if (ret != LIBMSI_RESULT_SUCCESS && ret != LIBMSI_RESULT_INVALID_TABLE)
        goto end;

    ret = LIBMSI_RESULT_SUCCESS;

    while ( !list_empty( &transforms ) )
    {
        transform = LIST_ENTRY( list_head( &transforms ), TRANSFORMDATA, entry );

        if ( strcmpW( transform->name, szColumns ) &&
             strcmpW( transform->name, szTables ) &&
             ret == LIBMSI_RESULT_SUCCESS )
        {
            ret = msi_table_load_transform( db, stg, strings, transform, bytes_per_strref );
        }

        list_remove( &transform->entry );
        msi_free( transform->name );
        msi_free( transform );
    }

    if ( ret == LIBMSI_RESULT_SUCCESS )
        append_storage_to_db( db, stg );

end:
    if ( stgenum )
        IEnumSTATSTG_Release( stgenum );
    if ( strings )
        msi_destroy_stringtable( strings );

    return ret;
}
