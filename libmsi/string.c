/*
 * String Table Functions
 *
 * Copyright 2002-2004, Mike McCormack for CodeWeavers
 * Copyright 2007 Robert Shearman for CodeWeavers
 * Copyright 2010 Hans Leidekker for CodeWeavers
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
#include <gsf/gsf-msole-utils.h>

#include "debug.h"
#include "libmsi.h"
#include "msipriv.h"

#include "query.h"

#define CP_ACP 0

struct msistring
{
    uint16_t persistent_refcount;
    uint16_t nonpersistent_refcount;
    char *str;
};

struct string_table
{
    unsigned maxcount;         /* the number of strings */
    unsigned freeslot;
    unsigned codepage;
    unsigned sortcount;
    struct msistring *strings; /* an array of strings */
    unsigned *sorted;              /* index */
};

static bool validate_codepage( unsigned codepage )
{
    switch (codepage) {
    case CP_ACP:
    case 37: case 424: case 437: case 500: case 737: case 775: case 850:
    case 852: case 855: case 856: case 857: case 860: case 861: case 862:
    case 863: case 864: case 865: case 866: case 869: case 874: case 875:
    case 878: case 932: case 936: case 949: case 950: case 1006: case 1026:
    case 1250: case 1251: case 1252: case 1253: case 1254: case 1255:
    case 1256: case 1257: case 1258: case 1361:
    case 10000: case 10006: case 10007: case 10029: case 10079: case 10081:
    case 20127: case 20866: case 20932: case 21866: case 28591: case 28592:
    case 28593: case 28594: case 28595: case 28596: case 28597: case 28598:
    case 28599: case 28600: case 28603: case 28604: case 28605: case 28606:
    case 65000: case 65001:
        return true;

    default:
        return false;
    }
}

static string_table *init_stringtable( int entries, unsigned codepage )
{
    string_table *st;

    if (!validate_codepage( codepage ))
        return NULL;

    st = msi_alloc( sizeof (string_table) );
    if( !st )
        return NULL;    
    if( entries < 1 )
        entries = 1;

    st->strings = msi_alloc_zero( sizeof(struct msistring) * entries );
    if( !st->strings )
    {
        msi_free( st );
        return NULL;    
    }

    st->sorted = msi_alloc( sizeof (unsigned) * entries );
    if( !st->sorted )
    {
        msi_free( st->strings );
        msi_free( st );
        return NULL;
    }

    st->maxcount = entries;
    st->freeslot = 1;
    st->codepage = codepage;
    st->sortcount = 0;

    return st;
}

void msi_destroy_stringtable( string_table *st )
{
    unsigned i;

    for( i=0; i<st->maxcount; i++ )
    {
        if( st->strings[i].persistent_refcount ||
            st->strings[i].nonpersistent_refcount )
            msi_free( st->strings[i].str );
    }
    msi_free( st->strings );
    msi_free( st->sorted );
    msi_free( st );
}

static int st_find_free_entry( string_table *st )
{
    unsigned i, sz, *s;
    struct msistring *p;

    TRACE("%p\n", st);

    if( st->freeslot )
    {
        for( i = st->freeslot; i < st->maxcount; i++ )
            if( !st->strings[i].persistent_refcount &&
                !st->strings[i].nonpersistent_refcount )
                return i;
    }
    for( i = 1; i < st->maxcount; i++ )
        if( !st->strings[i].persistent_refcount &&
            !st->strings[i].nonpersistent_refcount )
            return i;

    /* dynamically resize */
    sz = st->maxcount + 1 + st->maxcount/2;
    p = msi_realloc_zero( st->strings, st->maxcount * sizeof(struct msistring), sz * sizeof(struct msistring) );
    if( !p )
        return -1;

    s = msi_realloc( st->sorted, sz*sizeof(unsigned) );
    if( !s )
    {
        msi_free( p );
        return -1;
    }

    st->strings = p;
    st->sorted = s;

    st->freeslot = st->maxcount;
    st->maxcount = sz;
    if( st->strings[st->freeslot].persistent_refcount ||
        st->strings[st->freeslot].nonpersistent_refcount )
        g_critical("oops. expected freeslot to be free...\n");
    return st->freeslot;
}

G_GNUC_PURE
static int find_insert_index( const string_table *st, unsigned string_id )
{
    int i, c, low = 0, high = st->sortcount - 1;

    while (low <= high)
    {
        i = (low + high) / 2;
        c = strcmp( st->strings[string_id].str, st->strings[st->sorted[i]].str );

        if (c < 0)
            high = i - 1;
        else if (c > 0)
            low = i + 1;
        else
            return -1; /* already exists */
    }
    return high + 1;
}

static void insert_string_sorted( string_table *st, unsigned string_id )
{
    int i;

    i = find_insert_index( st, string_id );
    if (i == -1)
        return;

    memmove( &st->sorted[i] + 1, &st->sorted[i], (st->sortcount - i) * sizeof(unsigned) );
    st->sorted[i] = string_id;
    st->sortcount++;
}

static void set_st_entry( string_table *st, unsigned n, char *str, uint16_t refcount, enum StringPersistence persistence )
{
    g_return_if_fail(str != NULL);

    if (persistence == StringPersistent)
    {
        st->strings[n].persistent_refcount = refcount;
        st->strings[n].nonpersistent_refcount = 0;
    }
    else
    {
        st->strings[n].persistent_refcount = 0;
        st->strings[n].nonpersistent_refcount = refcount;
    }

    st->strings[n].str = str;

    insert_string_sorted( st, n );

    if( n < st->maxcount )
        st->freeslot = n + 1;
}

static unsigned _libmsi_id_from_string( const string_table *st, const char *buffer, unsigned *id )
{
    size_t sz;
    unsigned r = LIBMSI_RESULT_INVALID_PARAMETER;
    char *str;
    int codepage;
    GIConv cpconv;

    TRACE("Finding string %s in string table\n", debugstr_a(buffer) );

    if( buffer[0] == 0 )
    {
        *id = 0;
        return LIBMSI_RESULT_SUCCESS;
    }

    codepage = st->codepage ? st->codepage : gsf_msole_iconv_win_codepage();
    cpconv = gsf_msole_iconv_open_codepage_for_export(codepage);
    str = g_convert_with_iconv(buffer, -1, cpconv, NULL, &sz, NULL);
    g_iconv_close(cpconv);
    if( !str )
        return r;

    r = _libmsi_id_from_string_utf8( st, str, id );
    msi_free( str );
    return r;
}

static int msi_addstring( string_table *st, unsigned n, const char *data, int len, uint16_t refcount, enum StringPersistence persistence )
{
    char *str;
    size_t sz;
    int codepage;
    GIConv cpconv;
    GError *err = NULL;

    if( !data )
        return 0;
    if( !data[0] )
        return 0;
    if( n > 0 )
    {
        if( st->strings[n].persistent_refcount ||
            st->strings[n].nonpersistent_refcount )
            return -1;
    }
    else
    {
        if( LIBMSI_RESULT_SUCCESS == _libmsi_id_from_string( st, data, &n ) )
        {
            if (persistence == StringPersistent)
                st->strings[n].persistent_refcount += refcount;
            else
                st->strings[n].nonpersistent_refcount += refcount;
            return n;
        }
        n = st_find_free_entry( st );
        if( n == -1 )
            return -1;
    }

    if( n < 1 )
    {
        g_critical("invalid index adding %s (%d)\n", debugstr_a( data ), n );
        return -1;
    }

    /* allocate a new string */
    codepage = st->codepage ? st->codepage : gsf_msole_iconv_win_codepage();
    cpconv = gsf_msole_iconv_open_for_import(codepage);
    str = g_convert_with_iconv(data, len, cpconv, NULL, &sz, &err);
    g_iconv_close(cpconv);
    if (err) {
        g_warning("iconv failed: %s", err->message);
        g_clear_error(&err);
    } else {
        set_st_entry( st, n, str, refcount, persistence);
    }

    return n;
}

int _libmsi_add_string( string_table *st, const char *data, int len, uint16_t refcount, enum StringPersistence persistence )
{
    unsigned n;
    char *str;

    if( !data )
        return 0;
    if( !data[0] )
        return 0;

    if( _libmsi_id_from_string_utf8( st, data, &n ) == LIBMSI_RESULT_SUCCESS )
    {
        if (persistence == StringPersistent)
            st->strings[n].persistent_refcount += refcount;
        else
            st->strings[n].nonpersistent_refcount += refcount;
        return n;
    }

    n = st_find_free_entry( st );
    if( n == -1 )
        return -1;

    /* allocate a new string */
    if(len<0)
        len = strlen(data);
    TRACE("%s, n = %d len = %d\n", debugstr_a(data), n, len );

    str = msi_alloc( (len+1)*sizeof(char) );
    if( !str )
        return -1;
    memcpy( str, data, len*sizeof(char) );
    str[len] = 0;

    set_st_entry( st, n, str, refcount, persistence );

    return n;
}

/* find the string identified by an id - return null if there's none */
G_GNUC_PURE
const char *msi_string_lookup_id( const string_table *st, unsigned id )
{
    if( id == 0 )
        return szEmpty;

    if( id >= st->maxcount )
        return NULL;

    if( id && !st->strings[id].persistent_refcount && !st->strings[id].nonpersistent_refcount)
        return NULL;

    return st->strings[id].str;
}

/*
 *  _libmsi_string_id
 *
 *  [in] st         - pointer to the string table
 *  [in] id         - id of the string to retrieve
 *  [out] buffer    - destination of the UTF8 string
 *  [in/out] sz     - number of bytes available in the buffer on input
 *                    number of bytes used on output
 *
 * Returned string is not NUL-terminated.
 */
static unsigned _libmsi_string_id( const string_table *st, unsigned id, char *buffer, unsigned *sz )
{
    const char *str_utf8;
    char *str;
    size_t len;
    GIConv cpconv;
    int codepage;

    TRACE("Finding string %d of %d\n", id, st->maxcount);

    str_utf8 = msi_string_lookup_id( st, id );
    if( !str_utf8 )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    codepage = st->codepage ? st->codepage : gsf_msole_iconv_win_codepage();
    cpconv = gsf_msole_iconv_open_codepage_for_export(codepage);
    str = g_convert_with_iconv(str_utf8, -1, cpconv, NULL, &len, NULL);
    g_iconv_close(cpconv);
    if( *sz < len )
    {
        *sz = len;
        return LIBMSI_RESULT_MORE_DATA;
    }
    *sz = len;
    memcpy(buffer, str, len);
    msi_free(str);
    return LIBMSI_RESULT_SUCCESS;
}

/*
 *  _libmsi_id_from_string_utf8
 *
 *  [in] st         - pointer to the string table
 *  [in] str        - string to find in the string table
 *  [out] id        - id of the string, if found
 */
unsigned _libmsi_id_from_string_utf8( const string_table *st, const char *str, unsigned *id )
{
    int i, c, low = 0, high = st->sortcount - 1;

    while (low <= high)
    {
        i = (low + high) / 2;
        c = strcmp( str, st->strings[st->sorted[i]].str );

        if (c < 0)
            high = i - 1;
        else if (c > 0)
            low = i + 1;
        else
        {
            *id = st->sorted[i];
            return LIBMSI_RESULT_SUCCESS;
        }
    }

    return LIBMSI_RESULT_INVALID_PARAMETER;
}

static void string_totalsize( const string_table *st, unsigned *datasize, unsigned *poolsize )
{
    unsigned i, holesize;
    size_t len;
    int codepage;
    GIConv cpconv;
    char *str;

    if( st->strings[0].str || st->strings[0].persistent_refcount || st->strings[0].nonpersistent_refcount)
        g_critical("oops. element 0 has a string\n");

    codepage = st->codepage ? st->codepage : gsf_msole_iconv_win_codepage();

    *poolsize = 4;
    *datasize = 0;
    holesize = 0;
    for( i=1; i<st->maxcount; i++ )
    {
        if( !st->strings[i].persistent_refcount )
        {
            TRACE("[%u] nonpersistent = %s\n", i, debugstr_a(st->strings[i].str));
            (*poolsize) += 4;
        }
        else if( st->strings[i].str )
        {
            TRACE("[%u] = %s\n", i, debugstr_a(st->strings[i].str));
            cpconv = gsf_msole_iconv_open_codepage_for_export(codepage);
            str = g_convert_with_iconv( st->strings[i].str, -1, cpconv, NULL, &len, NULL);
            g_iconv_close(cpconv);
	    msi_free(str);
            (*datasize) += len;
            if (len>0xffff)
                (*poolsize) += 4;
            (*poolsize) += holesize + 4;
            holesize = 0;
        }
        else
            holesize += 4;
    }
    TRACE("data %u pool %u codepage %x\n", *datasize, *poolsize, st->codepage );
}

string_table *msi_init_string_table( unsigned *bytes_per_strref )
{
    string_table *st;

    *bytes_per_strref = sizeof(uint16_t);
    st = init_stringtable( 1, CP_ACP );

    return st;
}

string_table *msi_load_string_table( GsfInfile *stg, unsigned *bytes_per_strref )
{
    string_table *st = NULL;
    char *data = NULL;
    uint16_t *pool = NULL;
    unsigned r, datasize = 0, poolsize = 0, codepage;
    unsigned i, count, offset, len, n, refs;

    r = read_stream_data( stg, szStringPool, (uint8_t **)&pool, &poolsize );
    if( r != LIBMSI_RESULT_SUCCESS)
        goto end;
    r = read_stream_data( stg, szStringData, (uint8_t **)&data, &datasize );
    if( r != LIBMSI_RESULT_SUCCESS)
        goto end;

    if ( (poolsize > 4) && (GUINT16_FROM_LE(pool[1]) & 0x8000) )
        *bytes_per_strref = LONG_STR_BYTES;
    else
        *bytes_per_strref = sizeof(uint16_t);

    count = poolsize/4;
    if( poolsize > 4 )
        codepage = GUINT16_FROM_LE(pool[0]) | ( (GUINT16_FROM_LE(pool[1]) & ~0x8000) << 16 );
    else
        codepage = CP_ACP;

    st = init_stringtable( count, codepage );
    if (!st)
        goto end;

    offset = 0;
    n = 1;
    i = 1;
    while ( i<count )
    {
        /* the string reference count is always the second word */
        refs = GUINT16_FROM_LE(pool[i*2+1]);

        /* empty entries have two zeros, still have a string id */
        if (GUINT16_FROM_LE(pool[i*2]) == 0 && refs == 0)
        {
            i++;
            n++;
            continue;
        }

        /*
         * If a string is over 64k, the previous string entry is made null
         * and the high word of the length is inserted in the null string's
         * reference count field.
         */
        if (GUINT16_FROM_LE(pool[i*2]) == 0)
        {
            len = (GUINT16_FROM_LE(pool[i*2+3]) << 16) + GUINT16_FROM_LE(pool[i*2+2]);
            i += 2;
        }
        else
        {
            len = GUINT16_FROM_LE(pool[i*2]);
            i += 1;
        }

        if ( (offset + len) > datasize )
        {
            g_critical("string table corrupt?\n");
            break;
        }

        r = msi_addstring( st, n, data+offset, len, refs, StringPersistent );
        if( r != n )
            g_critical("Failed to add string %d\n", n );
        n++;
        offset += len;
    }

    if ( datasize != offset )
        g_critical("string table load failed! (%08x != %08x), please report\n", datasize, offset );

    TRACE("Loaded %d strings\n", count);

end:
    msi_free( pool );
    msi_free( data );

    return st;
}

unsigned msi_save_string_table( const string_table *st, LibmsiDatabase *db, unsigned *bytes_per_strref )
{
    unsigned i, datasize = 0, poolsize = 0, sz, used, r, codepage, n;
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    char *data = NULL;
    uint8_t *pool = NULL;

    TRACE("\n");

    /* construct the new table in memory first */
    string_totalsize( st, &datasize, &poolsize );

    TRACE("%u %u %u\n", st->maxcount, datasize, poolsize );

    pool = msi_alloc( poolsize );
    if( ! pool )
    {
        g_warning("Failed to alloc pool %d bytes\n", poolsize );
        goto err;
    }
    data = msi_alloc( datasize );
    if( ! data )
    {
        g_warning("Failed to alloc data %d bytes\n", datasize );
        goto err;
    }

    used = 0;
    codepage = st->codepage;
    pool[0] = codepage & 0xff;
    pool[1] = codepage >> 8;
    pool[2] = codepage >> 16;
    pool[3] = codepage >> 24;
    if (st->maxcount > 0xffff)
    {
        pool[3] |= 0x80;
        *bytes_per_strref = LONG_STR_BYTES;
    }
    else
        *bytes_per_strref = sizeof(uint16_t);

    i = 1;
    for( n=1; n<st->maxcount; n++ )
    {
        if( !st->strings[n].persistent_refcount )
        {
            pool[ i*4 ] = 0;
            pool[ i*4 + 1] = 0;
            pool[ i*4 + 2] = 0;
            pool[ i*4 + 3] = 0;
            i++;
            continue;
        }

        sz = datasize - used;
        r = _libmsi_string_id( st, n, data+used, &sz );
        if( r != LIBMSI_RESULT_SUCCESS )
        {
            g_critical("failed to fetch string\n");
            sz = 0;
        }

        if (sz == 0) {
            pool[ i*4 ] = 0;
            pool[ i*4 + 1 ] = 0;
            pool[ i*4 + 2 ] = 0;
            pool[ i*4 + 3 ] = 0;
            i++;
            continue;
        }

        if (sz >= 0x10000)
        {
            /* Write a dummy entry, with the high part of the length
             * in the reference count.  */
            pool[ i*4 ] = 0;
            pool[ i*4 + 1 ] = 0;
            pool[ i*4 + 2 ] = (sz >> 16);
            pool[ i*4 + 3 ] = (sz >> 24);
            i++;
        }
        pool[ i*4 ]     = sz;
        pool[ i*4 + 1 ] = sz >> 8;
        pool[ i*4 + 2 ] = st->strings[n].persistent_refcount;
        pool[ i*4 + 3 ] = st->strings[n].persistent_refcount >> 8;
        i++;
        used += sz;
        if( used > datasize  )
        {
            g_critical("oops overran %d >= %d\n", used, datasize);
            goto err;
        }
    }

    if( used != datasize )
    {
        g_critical("oops used %d != datasize %d\n", used, datasize);
        goto err;
    }

    /* write the streams */
    r = write_stream_data( db, szStringData, data, datasize );
    TRACE("Wrote StringData r=%08x\n", r);
    if( r )
        goto err;
    r = write_stream_data( db, szStringPool, pool, poolsize );
    TRACE("Wrote StringPool r=%08x\n", r);
    if( r )
        goto err;

    ret = LIBMSI_RESULT_SUCCESS;

err:
    msi_free( data );
    msi_free( pool );

    return ret;
}

G_GNUC_PURE
unsigned msi_get_string_table_codepage( const string_table *st )
{
    return st->codepage;
}

unsigned msi_set_string_table_codepage( string_table *st, unsigned codepage )
{
    if (validate_codepage( codepage ))
    {
        st->codepage = codepage;
        return LIBMSI_RESULT_SUCCESS;
    }
    return LIBMSI_RESULT_FUNCTION_FAILED;
}
