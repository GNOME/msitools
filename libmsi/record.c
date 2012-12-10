/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002-2004 Mike McCormack for CodeWeavers
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

#include "debug.h"
#include "libmsi.h"
#include "msipriv.h"

#include "query.h"


#define LIBMSI_FIELD_TYPE_NULL   0
#define LIBMSI_FIELD_TYPE_INT    1
#define LIBMSI_FIELD_TYPE_STR   3
#define LIBMSI_FIELD_TYPE_STREAM 4

static void _libmsi_free_field( LibmsiField *field )
{
    switch( field->type )
    {
    case LIBMSI_FIELD_TYPE_NULL:
    case LIBMSI_FIELD_TYPE_INT:
        break;
    case LIBMSI_FIELD_TYPE_STR:
        msi_free( field->u.szVal);
        break;
    case LIBMSI_FIELD_TYPE_STREAM:
        g_object_unref(G_OBJECT(field->u.stream));
        break;
    default:
        ERR("Invalid field type %d\n", field->type);
    }
}

void _libmsi_record_destroy( LibmsiObject *arg )
{
    LibmsiRecord *rec = (LibmsiRecord *) arg;
    unsigned i;

    for( i=0; i<=rec->count; i++ )
        _libmsi_free_field( &rec->fields[i] );
}

LibmsiRecord *libmsi_record_create( unsigned cParams )
{
    LibmsiRecord *rec;
    unsigned len;

    TRACE("%d\n", cParams);

    if( cParams>65535 )
        return NULL;

    len = sizeof (LibmsiRecord) + sizeof (LibmsiField)*cParams;
    rec = alloc_msiobject( len, _libmsi_record_destroy );
    if( rec )
        rec->count = cParams;
    return rec;
}

unsigned libmsi_record_get_field_count( const LibmsiRecord *rec )
{
    if( !rec )
        return -1;

    return rec->count;
}

static bool expr_int_from_string( const char *str, int *out )
{
    int x = 0;
    const char *p = str;

    if( *p == '-' ) /* skip the minus sign */
        p++;
    while ( *p )
    {
        if( (*p < '0') || (*p > '9') )
            return false;
        x *= 10;
        x += (*p - '0');
        p++;
    }

    if( str[0] == '-' ) /* check if it's negative */
        x = -x;
    *out = x; 

    return true;
}

unsigned _libmsi_record_copy_field( LibmsiRecord *in_rec, unsigned in_n,
                          LibmsiRecord *out_rec, unsigned out_n )
{
    unsigned r = LIBMSI_RESULT_SUCCESS;

    if ( in_n > in_rec->count || out_n > out_rec->count )
        r = LIBMSI_RESULT_FUNCTION_FAILED;
    else if ( in_rec != out_rec || in_n != out_n )
    {
        char *str;
        LibmsiField *in, *out;

        in = &in_rec->fields[in_n];
        out = &out_rec->fields[out_n];

        switch ( in->type )
        {
        case LIBMSI_FIELD_TYPE_NULL:
            break;
        case LIBMSI_FIELD_TYPE_INT:
            out->u.iVal = in->u.iVal;
            break;
        case LIBMSI_FIELD_TYPE_STR:
            str = strdup( in->u.szVal );
            if ( !str )
                r = LIBMSI_RESULT_OUTOFMEMORY;
            else
                out->u.szVal = str;
            break;
        case LIBMSI_FIELD_TYPE_STREAM:
            g_object_ref(G_OBJECT(in->u.stream));
            out->u.stream = in->u.stream;
            break;
        default:
            ERR("invalid field type %d\n", in->type);
        }
        if (r == LIBMSI_RESULT_SUCCESS)
            out->type = in->type;
    }

    return r;
}

int libmsi_record_get_integer( const LibmsiRecord *rec, unsigned iField)
{
    int ret = 0;

    TRACE("%p %d\n", rec, iField );

    if( !rec )
        return MSI_NULL_INTEGER;

    if( iField > rec->count )
        return MSI_NULL_INTEGER;

    switch( rec->fields[iField].type )
    {
    case LIBMSI_FIELD_TYPE_INT:
        return rec->fields[iField].u.iVal;
    case LIBMSI_FIELD_TYPE_STR:
        if( expr_int_from_string( rec->fields[iField].u.szVal, &ret ) )
            return ret;
        return MSI_NULL_INTEGER;
    default:
        break;
    }

    return MSI_NULL_INTEGER;
}

LibmsiResult libmsi_record_clear_data( LibmsiRecord *rec )
{
    unsigned i;

    TRACE("%d\n", rec );

    if( !rec )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &rec->hdr );
    for( i=0; i<=rec->count; i++)
    {
        _libmsi_free_field( &rec->fields[i] );
        rec->fields[i].type = LIBMSI_FIELD_TYPE_NULL;
        rec->fields[i].u.iVal = 0;
    }
    msiobj_release( &rec->hdr );

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiResult libmsi_record_set_int( LibmsiRecord *rec, unsigned iField, int iVal )
{
    TRACE("%p %u %d\n", rec, iField, iVal);

    if( !rec )
        return LIBMSI_RESULT_INVALID_HANDLE;

    if( iField > rec->count )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    _libmsi_free_field( &rec->fields[iField] );
    rec->fields[iField].type = LIBMSI_FIELD_TYPE_INT;
    rec->fields[iField].u.iVal = iVal;

    return LIBMSI_RESULT_SUCCESS;
}

bool libmsi_record_is_null( const LibmsiRecord *rec, unsigned iField )
{
    bool r = true;

    TRACE("%p %d\n", rec, iField );

    if( !rec )
        return 0;

    r = ( iField > rec->count ) ||
        ( rec->fields[iField].type == LIBMSI_FIELD_TYPE_NULL );

    return r;
}

LibmsiResult libmsi_record_get_string(const LibmsiRecord *rec, unsigned iField,
               char *szValue, unsigned *pcchValue)
{
    unsigned len=0, ret;
    char buffer[16];

    TRACE("%p %d %p %p\n", rec, iField, szValue, pcchValue);

    if( !rec )
        return LIBMSI_RESULT_INVALID_HANDLE;

    if( iField > rec->count )
    {
        if ( szValue && *pcchValue > 0 )
            szValue[0] = 0;

        *pcchValue = 0;
        return LIBMSI_RESULT_SUCCESS;
    }

    ret = LIBMSI_RESULT_SUCCESS;
    switch( rec->fields[iField].type )
    {
    case LIBMSI_FIELD_TYPE_INT:
        sprintf(buffer, "%d", rec->fields[iField].u.iVal);
        len = strlen( buffer );
        if (szValue)
            strcpyn(szValue, buffer, *pcchValue);
        break;
    case LIBMSI_FIELD_TYPE_STR:
	len = strlen( rec->fields[iField].u.szVal );
        if (szValue)
            strcpyn(szValue, rec->fields[iField].u.szVal, *pcchValue );
        break;
    case LIBMSI_FIELD_TYPE_NULL:
        if( szValue && *pcchValue > 0 )
            szValue[0] = 0;
        break;
    default:
        ret = LIBMSI_RESULT_INVALID_PARAMETER;
        break;
    }

    if( szValue && *pcchValue <= len )
        ret = LIBMSI_RESULT_MORE_DATA;
    *pcchValue = len;

    return ret;
}

const char *_libmsi_record_get_string_raw( const LibmsiRecord *rec, unsigned iField )
{
    if( iField > rec->count )
        return NULL;

    if( rec->fields[iField].type != LIBMSI_FIELD_TYPE_STR )
        return NULL;

    return rec->fields[iField].u.szVal;
}

unsigned _libmsi_record_get_string(const LibmsiRecord *rec, unsigned iField,
               char *szValue, unsigned *pcchValue)
{
    unsigned len=0, ret;
    char buffer[16];
    static const char szFormat[] = "%d";

    TRACE("%p %d %p %p\n", rec, iField, szValue, pcchValue);

    if( iField > rec->count )
    {
        if ( szValue && *pcchValue > 0 )
            szValue[0] = 0;

        *pcchValue = 0;
        return LIBMSI_RESULT_SUCCESS;
    }

    ret = LIBMSI_RESULT_SUCCESS;
    switch( rec->fields[iField].type )
    {
    case LIBMSI_FIELD_TYPE_INT:
        sprintf(buffer, szFormat, rec->fields[iField].u.iVal);
        len = strlen( buffer );
        if (szValue)
            strcpyn(szValue, buffer, *pcchValue);
        break;
    case LIBMSI_FIELD_TYPE_STR:
        len = strlen( rec->fields[iField].u.szVal );
        if (szValue)
            strcpyn(szValue, rec->fields[iField].u.szVal, *pcchValue);
        break;
    case LIBMSI_FIELD_TYPE_NULL:
        if( szValue && *pcchValue > 0 )
            szValue[0] = 0;
        break;
    default:
        break;
    }

    if( szValue && *pcchValue <= len )
        ret = LIBMSI_RESULT_MORE_DATA;
    *pcchValue = len;

    return ret;
}

unsigned libmsi_record_get_field_size(const LibmsiRecord *rec, unsigned iField)
{
    TRACE("%p %d\n", rec, iField);

    if( !rec )
        return 0;

    if( iField > rec->count )
        return 0;

    switch( rec->fields[iField].type )
    {
    case LIBMSI_FIELD_TYPE_INT:
        return sizeof (int);
    case LIBMSI_FIELD_TYPE_STR:
        return strlen( rec->fields[iField].u.szVal );
    case LIBMSI_FIELD_TYPE_NULL:
        break;
    case LIBMSI_FIELD_TYPE_STREAM:
        return gsf_input_size( rec->fields[iField].u.stream );
    }
    return 0;
}

LibmsiResult libmsi_record_set_string( LibmsiRecord *rec, unsigned iField, const char *szValue )
{
    char *str;

    TRACE("%d %d %s\n", rec, iField, debugstr_a(szValue));

    if( !rec )
        return LIBMSI_RESULT_INVALID_HANDLE;

    if( iField > rec->count )
        return LIBMSI_RESULT_INVALID_FIELD;

    _libmsi_free_field( &rec->fields[iField] );

    if( szValue && szValue[0] )
    {
        str = strdup( szValue );
        rec->fields[iField].type = LIBMSI_FIELD_TYPE_STR;
        rec->fields[iField].u.szVal = str;
    }
    else
    {
        rec->fields[iField].type = LIBMSI_FIELD_TYPE_NULL;
        rec->fields[iField].u.szVal = NULL;
    }

    return 0;
}

/* read the data in a file into a memory-backed GsfInput */
static unsigned _libmsi_addstream_from_file(const char *szFile, GsfInput **pstm)
{
    GsfInput *stm;
    char *data;
    off_t sz;

    stm = gsf_input_stdio_new(szFile, NULL);
    if (!stm)
    {
        WARN("open file failed for %s\n", debugstr_a(szFile));
        return LIBMSI_RESULT_OPEN_FAILED;
    }

    sz = gsf_input_size(stm);
    if (sz == 0)
    {
        data = g_malloc(1);
    }
    else
    {
        data = g_try_malloc(sz);
        if (!data)
            return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;

        if (!gsf_input_read(stm, sz, data))
        {
            g_object_unref(G_OBJECT(stm));
            return LIBMSI_RESULT_FUNCTION_FAILED;
        }
    }

    g_object_unref(G_OBJECT(stm));
    *pstm = gsf_input_memory_new(data, sz, true);

    TRACE("read %s, %d bytes into GsfInput %p\n", debugstr_a(szFile), sz, *pstm);

    return LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_record_load_stream(LibmsiRecord *rec, unsigned iField, GsfInput *stream)
{
    if ( (iField == 0) || (iField > rec->count) )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    _libmsi_free_field( &rec->fields[iField] );
    rec->fields[iField].type = LIBMSI_FIELD_TYPE_STREAM;
    rec->fields[iField].u.stream = stream;

    return LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_record_load_stream_from_file(LibmsiRecord *rec, unsigned iField, const char *szFilename)
{
    GsfInput *stm;
    unsigned r;

    if( (iField == 0) || (iField > rec->count) )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    /* no filename means we should seek back to the start of the stream */
    if( !szFilename )
    {
        if( rec->fields[iField].type != LIBMSI_FIELD_TYPE_STREAM )
            return LIBMSI_RESULT_INVALID_FIELD;

        stm = rec->fields[iField].u.stream;
        if( !stm )
            return LIBMSI_RESULT_INVALID_FIELD;

        gsf_input_seek( stm, 0, G_SEEK_SET );
    }
    else
    {
        /* read the file into a stream and save the stream in the record */
        r = _libmsi_addstream_from_file(szFilename, &stm);
        if( r != LIBMSI_RESULT_SUCCESS )
            return r;

        /* if all's good, store it in the record */
        _libmsi_record_load_stream(rec, iField, stm);
    }

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiResult libmsi_record_load_stream(LibmsiRecord *rec, unsigned iField, const char *szFilename)
{
    unsigned ret;

    TRACE("%d %d %s\n", rec, iField, debugstr_a(szFilename));

    if( !rec )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &rec->hdr );
    ret = _libmsi_record_load_stream_from_file( rec, iField, szFilename );
    msiobj_release( &rec->hdr );
    return ret;
}

unsigned _libmsi_record_save_stream(const LibmsiRecord *rec, unsigned iField, char *buf, unsigned *sz)
{
    uint64_t left;
    GsfInput *stm;

    TRACE("%p %d %p %p\n", rec, iField, buf, sz);

    if( !sz )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( iField > rec->count)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if ( rec->fields[iField].type == LIBMSI_FIELD_TYPE_NULL )
    {
        *sz = 0;
        return LIBMSI_RESULT_INVALID_DATA;
    }

    if( rec->fields[iField].type != LIBMSI_FIELD_TYPE_STREAM )
        return LIBMSI_RESULT_INVALID_DATATYPE;

    stm = rec->fields[iField].u.stream;
    if( !stm )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    left = gsf_input_size(stm) - gsf_input_tell(stm);

    /* if there's no buffer pointer, calculate the length to the end */
    if( !buf )
    {
        *sz = left;

        return LIBMSI_RESULT_SUCCESS;
    }

    /* read the data */
    if (*sz > left)
        *sz = left;

    if (*sz > 0 && !gsf_input_read( stm, *sz, buf ))
    {
        *sz = 0;
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiResult libmsi_record_save_stream(LibmsiRecord *rec, unsigned iField, char *buf, unsigned *sz)
{
    unsigned ret;

    TRACE("%d %d %p %p\n", rec, iField, buf, sz);

    if( !rec )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &rec->hdr );
    ret = _libmsi_record_save_stream( rec, iField, buf, sz );
    msiobj_release( &rec->hdr );
    return ret;
}

unsigned _libmsi_record_set_gsf_input( LibmsiRecord *rec, unsigned iField, GsfInput *stm )
{
    TRACE("%p %d %p\n", rec, iField, stm);

    if( iField > rec->count )
        return LIBMSI_RESULT_INVALID_FIELD;

    _libmsi_free_field( &rec->fields[iField] );

    rec->fields[iField].type = LIBMSI_FIELD_TYPE_STREAM;
    rec->fields[iField].u.stream = stm;
    g_object_ref(G_OBJECT(stm));

    return LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_record_get_gsf_input( const LibmsiRecord *rec, unsigned iField, GsfInput **pstm)
{
    TRACE("%p %d %p\n", rec, iField, pstm);

    if( iField > rec->count )
        return LIBMSI_RESULT_INVALID_FIELD;

    if( rec->fields[iField].type != LIBMSI_FIELD_TYPE_STREAM )
        return LIBMSI_RESULT_INVALID_FIELD;

    *pstm = rec->fields[iField].u.stream;
    g_object_ref(G_OBJECT(*pstm));

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiRecord *_libmsi_record_clone(LibmsiRecord *rec)
{
    LibmsiRecord *clone;
    unsigned r, i, count;

    count = libmsi_record_get_field_count(rec);
    clone = libmsi_record_create(count);
    if (!clone)
        return NULL;

    for (i = 0; i <= count; i++)
    {
        if (rec->fields[i].type == LIBMSI_FIELD_TYPE_STREAM)
        {
            GsfInput *stm = gsf_input_dup(rec->fields[i].u.stream, NULL);
            if (!stm)
            {
                msiobj_release(&clone->hdr);
                return NULL;
            }
	    clone->fields[i].u.stream = stm;
            clone->fields[i].type = LIBMSI_FIELD_TYPE_STREAM;
        }
        else
        {
            r = _libmsi_record_copy_field(rec, i, clone, i);
            if (r != LIBMSI_RESULT_SUCCESS)
            {
                msiobj_release(&clone->hdr);
                return NULL;
            }
        }
    }

    return clone;
}

bool _libmsi_record_compare_fields(const LibmsiRecord *a, const LibmsiRecord *b, unsigned field)
{
    if (a->fields[field].type != b->fields[field].type)
        return false;

    switch (a->fields[field].type)
    {
        case LIBMSI_FIELD_TYPE_NULL:
            break;

        case LIBMSI_FIELD_TYPE_INT:
            if (a->fields[field].u.iVal != b->fields[field].u.iVal)
                return false;
            break;

        case LIBMSI_FIELD_TYPE_STR:
            if (strcmp(a->fields[field].u.szVal, b->fields[field].u.szVal))
                return false;
            break;

        case LIBMSI_FIELD_TYPE_STREAM:
        default:
            return false;
    }
    return true;
}


bool _libmsi_record_compare(const LibmsiRecord *a, const LibmsiRecord *b)
{
    unsigned i;

    if (a->count != b->count)
        return false;

    for (i = 0; i <= a->count; i++)
    {
        if (!_libmsi_record_compare_fields( a, b, i ))
            return false;
    }

    return true;
}

char *msi_dup_record_field( LibmsiRecord *rec, int field )
{
    unsigned sz = 0;
    char *str;
    unsigned r;

    if (libmsi_record_is_null( rec, field )) return NULL;

    r = _libmsi_record_get_string( rec, field, NULL, &sz );
    if (r != LIBMSI_RESULT_SUCCESS)
        return NULL;

    sz++;
    str = msi_alloc( sz * sizeof(char) );
    if (!str) return NULL;
    str[0] = 0;
    r = _libmsi_record_get_string( rec, field, str, &sz );
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        ERR("failed to get string!\n");
        msi_free( str );
        return NULL;
    }
    return str;
}
