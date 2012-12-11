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

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winerror.h"

#include "libmsi-record.h"

#include "debug.h"
#include "unicode.h"
#include "libmsi.h"
#include "msipriv.h"
#include "objidl.h"
#include "winnls.h"
#include "ole2.h"

#include "shlwapi.h"

#include "query.h"

enum
{
    PROP_0,

    PROP_COUNT
};

G_DEFINE_TYPE (LibmsiRecord, libmsi_record, G_TYPE_OBJECT);

#define LIBMSI_FIELD_TYPE_NULL   0
#define LIBMSI_FIELD_TYPE_INT    1
#define LIBMSI_FIELD_TYPE_WSTR   3
#define LIBMSI_FIELD_TYPE_STREAM 4

static void
libmsi_record_init (LibmsiRecord *self)
{
}

static void
_libmsi_free_field (LibmsiField *field )
{
    switch (field->type) {
    case LIBMSI_FIELD_TYPE_NULL:
    case LIBMSI_FIELD_TYPE_INT:
        break;
    case LIBMSI_FIELD_TYPE_WSTR:
        g_free (field->u.szwVal);
        field->u.szwVal = NULL;
        break;
    case LIBMSI_FIELD_TYPE_STREAM:
        if (field->u.stream) {
            IStream_Release( field->u.stream );
            field->u.stream = NULL;
        }
        break;
    default:
        ERR ("Invalid field type %d\n", field->type);
    }
}

static void
libmsi_record_finalize (GObject *object)
{
    LibmsiRecord *p = LIBMSI_RECORD (object);
    unsigned i;

    for (i = 0; i <= p->count; i++ )
        _libmsi_free_field (&p->fields[i]);

    g_free (p->fields);

    G_OBJECT_CLASS (libmsi_record_parent_class)->finalize (object);
}

static void
libmsi_record_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (LIBMSI_IS_RECORD (object));
    LibmsiRecord *p = LIBMSI_RECORD (object);

    switch (prop_id) {
    case PROP_COUNT:
        p->count = g_value_get_uint (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
libmsi_record_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (LIBMSI_IS_RECORD (object));
    LibmsiRecord *p = LIBMSI_RECORD (object);

    switch (prop_id) {
    case PROP_COUNT:
        g_value_set_uint (value, p->count);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
libmsi_record_constructed (GObject *object)
{
    LibmsiRecord *self = LIBMSI_RECORD (object);
    LibmsiRecord *p = self;

    // FIXME: +1 could be removed if accessing with idx-1
    p->fields = g_new0 (LibmsiField, p->count + 1);

    G_OBJECT_CLASS (libmsi_record_parent_class)->constructed (object);
}

static void
libmsi_record_class_init (LibmsiRecordClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = libmsi_record_finalize;
    object_class->set_property = libmsi_record_set_property;
    object_class->get_property = libmsi_record_get_property;
    object_class->constructed = libmsi_record_constructed;

    g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_COUNT,
        g_param_spec_uint ("count", "count", "count", 0, 65535, 0,
                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));
}

unsigned libmsi_record_get_field_count( const LibmsiRecord *rec )
{
    if( !rec )
        return -1;

    return rec->count;
}

static bool expr_int_from_string( const WCHAR *str, int *out )
{
    int x = 0;
    const WCHAR *p = str;

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
        WCHAR *str;
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
        case LIBMSI_FIELD_TYPE_WSTR:
            str = strdupW( in->u.szwVal );
            if ( !str )
                r = LIBMSI_RESULT_OUTOFMEMORY;
            else
                out->u.szwVal = str;
            break;
        case LIBMSI_FIELD_TYPE_STREAM:
            IStream_AddRef( in->u.stream );
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

int libmsi_record_get_int( const LibmsiRecord *rec, unsigned field)
{
    int ret = 0;

    TRACE("%p %d\n", rec, field );

    if( !rec )
        return LIBMSI_NULL_INT;

    if( field > rec->count )
        return LIBMSI_NULL_INT;

    switch( rec->fields[field].type )
    {
    case LIBMSI_FIELD_TYPE_INT:
        return rec->fields[field].u.iVal;
    case LIBMSI_FIELD_TYPE_WSTR:
        if( expr_int_from_string( rec->fields[field].u.szwVal, &ret ) )
            return ret;
        return LIBMSI_NULL_INT;
    default:
        break;
    }

    return LIBMSI_NULL_INT;
}

LibmsiResult libmsi_record_clear( LibmsiRecord *rec )
{
    unsigned i;

    TRACE("%d\n", rec );

    if( !rec )
        return LIBMSI_RESULT_INVALID_HANDLE;

    g_object_ref(rec);
    for( i=0; i<=rec->count; i++)
    {
        _libmsi_free_field( &rec->fields[i] );
        rec->fields[i].type = LIBMSI_FIELD_TYPE_NULL;
        rec->fields[i].u.iVal = 0;
    }
    g_object_unref(rec);

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiResult libmsi_record_set_int( LibmsiRecord *rec, unsigned field, int iVal )
{
    TRACE("%p %u %d\n", rec, field, iVal);

    if( !rec )
        return LIBMSI_RESULT_INVALID_HANDLE;

    if( field > rec->count )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    _libmsi_free_field( &rec->fields[field] );
    rec->fields[field].type = LIBMSI_FIELD_TYPE_INT;
    rec->fields[field].u.iVal = iVal;

    return LIBMSI_RESULT_SUCCESS;
}

gboolean libmsi_record_is_null( const LibmsiRecord *rec, unsigned field )
{
    bool r = true;

    TRACE("%p %d\n", rec, field );

    if( !rec )
        return 0;

    r = ( field > rec->count ) ||
        ( rec->fields[field].type == LIBMSI_FIELD_TYPE_NULL );

    return r;
}

gchar* libmsi_record_get_string(const LibmsiRecord *self, unsigned field)
{
    g_return_val_if_fail (LIBMSI_IS_RECORD (self), NULL);

    TRACE ("%p %d\n", self, field);

    if (field > self->count)
        return g_strdup (""); // FIXME: really?

    switch (self->fields[field].type) {
    case LIBMSI_FIELD_TYPE_INT:
        return g_strdup_printf ("%d", self->fields[field].u.iVal);
    case LIBMSI_FIELD_TYPE_WSTR:
        return strdupWtoA (self->fields[field].u.szwVal);
    case LIBMSI_FIELD_TYPE_NULL:
        return g_strdup ("");
    }

    return NULL;
}

const WCHAR *_libmsi_record_get_string_raw( const LibmsiRecord *rec, unsigned field )
{
    if( field > rec->count )
        return NULL;

    if( rec->fields[field].type != LIBMSI_FIELD_TYPE_WSTR )
        return NULL;

    return rec->fields[field].u.szwVal;
}

unsigned _libmsi_record_get_stringW(const LibmsiRecord *rec, unsigned field,
               WCHAR *szValue, unsigned *pcchValue)
{
    unsigned len=0, ret;
    WCHAR buffer[16];
    static const WCHAR szFormat[] = { '%','d',0 };

    TRACE("%p %d %p %p\n", rec, field, szValue, pcchValue);

    if( field > rec->count )
    {
        if ( szValue && *pcchValue > 0 )
            szValue[0] = 0;

        *pcchValue = 0;
        return LIBMSI_RESULT_SUCCESS;
    }

    ret = LIBMSI_RESULT_SUCCESS;
    switch( rec->fields[field].type )
    {
    case LIBMSI_FIELD_TYPE_INT:
        wsprintfW(buffer, szFormat, rec->fields[field].u.iVal);
        len = strlenW( buffer );
        if (szValue)
            strcpynW(szValue, buffer, *pcchValue);
        break;
    case LIBMSI_FIELD_TYPE_WSTR:
        len = strlenW( rec->fields[field].u.szwVal );
        if (szValue)
            strcpynW(szValue, rec->fields[field].u.szwVal, *pcchValue);
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

LibmsiResult libmsi_record_set_string( LibmsiRecord *rec, unsigned field, const char *szValue )
{
    WCHAR *str;

    TRACE("%d %d %s\n", rec, field, debugstr_a(szValue));

    if( !rec )
        return LIBMSI_RESULT_INVALID_HANDLE;

    str = strdupAtoW( szValue );
    return _libmsi_record_set_stringW( rec, field, str );
}

unsigned _libmsi_record_set_stringW( LibmsiRecord *rec, unsigned field, const WCHAR *szValue )
{
    WCHAR *str;

    TRACE("%p %d %s\n", rec, field, debugstr_w(szValue));

    if( field > rec->count )
        return LIBMSI_RESULT_INVALID_FIELD;

    _libmsi_free_field( &rec->fields[field] );

    if( szValue && szValue[0] )
    {
        str = strdupW( szValue );
        rec->fields[field].type = LIBMSI_FIELD_TYPE_WSTR;
        rec->fields[field].u.szwVal = str;
    }
    else
    {
        rec->fields[field].type = LIBMSI_FIELD_TYPE_NULL;
        rec->fields[field].u.szwVal = NULL;
    }

    return 0;
}

/* read the data in a file into an IStream */
static unsigned _libmsi_addstream_from_file(const char *szFile, IStream **pstm)
{
    unsigned sz, szHighWord = 0, read;
    HANDLE handle;
    HGLOBAL hGlob = 0;
    HRESULT hr;
    ULARGE_INTEGER ulSize;

    TRACE("reading %s\n", debugstr_a(szFile));

    /* read the file into memory */
    handle = CreateFileA(szFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if( handle == INVALID_HANDLE_VALUE )
        return GetLastError();
    sz = GetFileSize(handle, &szHighWord);
    if( sz != INVALID_FILE_SIZE && szHighWord == 0 )
    {
        hGlob = GlobalAlloc(GMEM_FIXED, sz);
        if( hGlob )
        {
            bool r = ReadFile(handle, hGlob, sz, &read, NULL);
            if( !r )
            {
                GlobalFree(hGlob);
                hGlob = 0;
            }
        }
    }
    CloseHandle(handle);
    if( !hGlob )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    /* make a stream out of it, and set the correct file size */
    hr = CreateStreamOnHGlobal(hGlob, true, pstm);
    if( FAILED( hr ) )
    {
        GlobalFree(hGlob);
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    /* set the correct size - CreateStreamOnHGlobal screws it up */
    ulSize.QuadPart = sz;
    IStream_SetSize(*pstm, ulSize);

    TRACE("read %s, %d bytes into IStream %p\n", debugstr_a(szFile), sz, *pstm);

    return LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_record_load_stream(LibmsiRecord *rec, unsigned field, IStream *stream)
{
    if ( (field == 0) || (field > rec->count) )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    _libmsi_free_field( &rec->fields[field] );
    rec->fields[field].type = LIBMSI_FIELD_TYPE_STREAM;
    rec->fields[field].u.stream = stream;

    return LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_record_load_stream_from_file(LibmsiRecord *rec, unsigned field, const char *szFilename)
{
    IStream *stm = NULL;
    HRESULT r;

    if( (field == 0) || (field > rec->count) )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    /* no filename means we should seek back to the start of the stream */
    if( !szFilename )
    {
        LARGE_INTEGER ofs;
        ULARGE_INTEGER cur;

        if( rec->fields[field].type != LIBMSI_FIELD_TYPE_STREAM )
            return LIBMSI_RESULT_INVALID_FIELD;

        stm = rec->fields[field].u.stream;
        if( !stm )
            return LIBMSI_RESULT_INVALID_FIELD;

        ofs.QuadPart = 0;
        r = IStream_Seek( stm, ofs, STREAM_SEEK_SET, &cur );
        if( FAILED( r ) )
            return LIBMSI_RESULT_FUNCTION_FAILED;
    }
    else
    {
        /* read the file into a stream and save the stream in the record */
        r = _libmsi_addstream_from_file(szFilename, &stm);
        if( r != LIBMSI_RESULT_SUCCESS )
            return r;

        /* if all's good, store it in the record */
        _libmsi_record_load_stream(rec, field, stm);
    }

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiResult libmsi_record_load_stream(LibmsiRecord *rec, unsigned field, const char *szFilename)
{
    unsigned ret;

    TRACE("%d %d %s\n", rec, field, debugstr_a(szFilename));

    if( !rec )
        return LIBMSI_RESULT_INVALID_HANDLE;

    g_object_ref(rec);
    ret = _libmsi_record_load_stream_from_file( rec, field, szFilename );
    g_object_unref(rec);

    return ret;
}

unsigned _libmsi_record_save_stream(const LibmsiRecord *rec, unsigned field, char *buf, unsigned *sz)
{
    unsigned count;
    HRESULT r;
    IStream *stm;

    TRACE("%p %d %p %p\n", rec, field, buf, sz);

    if( !sz )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( field > rec->count)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if ( rec->fields[field].type == LIBMSI_FIELD_TYPE_NULL )
    {
        *sz = 0;
        return LIBMSI_RESULT_INVALID_DATA;
    }

    if( rec->fields[field].type != LIBMSI_FIELD_TYPE_STREAM )
        return LIBMSI_RESULT_INVALID_DATATYPE;

    stm = rec->fields[field].u.stream;
    if( !stm )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    /* if there's no buffer pointer, calculate the length to the end */
    if( !buf )
    {
        LARGE_INTEGER ofs;
        ULARGE_INTEGER end, cur;

        ofs.QuadPart = cur.QuadPart = 0;
        end.QuadPart = 0;
        IStream_Seek( stm, ofs, STREAM_SEEK_SET, &cur );
        IStream_Seek( stm, ofs, STREAM_SEEK_END, &end );
        ofs.QuadPart = cur.QuadPart;
        IStream_Seek( stm, ofs, STREAM_SEEK_SET, &cur );
        *sz = end.QuadPart - cur.QuadPart;

        return LIBMSI_RESULT_SUCCESS;
    }

    /* read the data */
    count = 0;
    r = IStream_Read( stm, buf, *sz, &count );
    if( FAILED( r ) )
    {
        *sz = 0;
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    *sz = count;

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiResult libmsi_record_save_stream(LibmsiRecord *rec, unsigned field, char *buf, unsigned *sz)
{
    unsigned ret;

    TRACE("%d %d %p %p\n", rec, field, buf, sz);

    if( !rec )
        return LIBMSI_RESULT_INVALID_HANDLE;

    g_object_ref(rec);
    ret = _libmsi_record_save_stream( rec, field, buf, sz );
    g_object_unref(rec);

    return ret;
}

unsigned _libmsi_record_set_IStream( LibmsiRecord *rec, unsigned field, IStream *stm )
{
    TRACE("%p %d %p\n", rec, field, stm);

    if( field > rec->count )
        return LIBMSI_RESULT_INVALID_FIELD;

    _libmsi_free_field( &rec->fields[field] );

    rec->fields[field].type = LIBMSI_FIELD_TYPE_STREAM;
    rec->fields[field].u.stream = stm;
    IStream_AddRef( stm );

    return LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_record_get_IStream( const LibmsiRecord *rec, unsigned field, IStream **pstm)
{
    TRACE("%p %d %p\n", rec, field, pstm);

    if( field > rec->count )
        return LIBMSI_RESULT_INVALID_FIELD;

    if( rec->fields[field].type != LIBMSI_FIELD_TYPE_STREAM )
        return LIBMSI_RESULT_INVALID_FIELD;

    *pstm = rec->fields[field].u.stream;
    IStream_AddRef( *pstm );

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiRecord *_libmsi_record_clone(LibmsiRecord *rec)
{
    LibmsiRecord *clone;
    unsigned r, i, count;

    count = libmsi_record_get_field_count(rec);
    clone = libmsi_record_new(count);
    if (!clone)
        return NULL;

    for (i = 0; i <= count; i++)
    {
        if (rec->fields[i].type == LIBMSI_FIELD_TYPE_STREAM)
        {
            if (FAILED(IStream_Clone(rec->fields[i].u.stream,
                                     &clone->fields[i].u.stream)))
            {
                g_object_unref(clone);
                return NULL;
            }
            clone->fields[i].type = LIBMSI_FIELD_TYPE_STREAM;
        }
        else
        {
            r = _libmsi_record_copy_field(rec, i, clone, i);
            if (r != LIBMSI_RESULT_SUCCESS)
            {
                g_object_unref(clone);
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

        case LIBMSI_FIELD_TYPE_WSTR:
            if (strcmpW(a->fields[field].u.szwVal, b->fields[field].u.szwVal))
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

WCHAR *msi_dup_record_field( LibmsiRecord *rec, int field )
{
    unsigned sz = 0;
    WCHAR *str;
    unsigned r;

    if (libmsi_record_is_null( rec, field )) return NULL;

    r = _libmsi_record_get_stringW( rec, field, NULL, &sz );
    if (r != LIBMSI_RESULT_SUCCESS)
        return NULL;

    sz++;
    str = msi_alloc( sz * sizeof(WCHAR) );
    if (!str) return NULL;
    str[0] = 0;
    r = _libmsi_record_get_stringW( rec, field, str, &sz );
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        ERR("failed to get string!\n");
        msi_free( str );
        return NULL;
    }
    return str;
}

LibmsiRecord *
libmsi_record_new (guint count)
{
    return g_object_new (LIBMSI_TYPE_RECORD,
                         "count", count,
                         NULL);
}
