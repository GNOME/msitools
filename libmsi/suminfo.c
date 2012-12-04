/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002, 2005 Mike McCormack for CodeWeavers
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
#include <time.h>
#include <assert.h>

#define COBJMACROS
#define NONAMELESSUNION

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "shlwapi.h"
#include "debug.h"
#include "unicode.h"
#include "libmsi.h"
#include "msipriv.h"
#include "objidl.h"
#include "propvarutil.h"
#include "msiserver.h"


static const uint8_t fmtid_SummaryInformation[16] =
        { 0xe0, 0x85, 0x9f, 0xf2, 0xf9, 0x4f, 0x68, 0x10, 0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9};

static void free_prop( PROPVARIANT *prop )
{
    if (prop->vt == VT_LPSTR)
        msi_free( prop->pszVal );
    prop->vt = VT_EMPTY;
}

static void _libmsi_summary_info_destroy( LibmsiObject *arg )
{
    LibmsiSummaryInfo *si = (LibmsiSummaryInfo *) arg;
    unsigned i;

    for( i = 0; i < MSI_MAX_PROPS; i++ )
        free_prop( &si->property[i] );
    msiobj_release( &si->database->hdr );
}

static unsigned get_type( unsigned uiProperty )
{
    switch( uiProperty )
    {
    case MSI_PID_CODEPAGE:
         return VT_I2;

    case MSI_PID_SUBJECT:
    case MSI_PID_AUTHOR:
    case MSI_PID_KEYWORDS:
    case MSI_PID_COMMENTS:
    case MSI_PID_TEMPLATE:
    case MSI_PID_LASTAUTHOR:
    case MSI_PID_REVNUMBER:
    case MSI_PID_APPNAME:
    case MSI_PID_TITLE:
         return VT_LPSTR;

    case MSI_PID_LASTPRINTED:
    case MSI_PID_CREATE_DTM:
    case MSI_PID_LASTSAVE_DTM:
         return VT_FILETIME;

    case MSI_PID_WORDCOUNT:
    case MSI_PID_CHARCOUNT:
    case MSI_PID_SECURITY:
    case MSI_PID_PAGECOUNT:
         return VT_I4;
    }
    return VT_EMPTY;
}

static unsigned get_property_count( const PROPVARIANT *property )
{
    unsigned i, n = 0;

    if( !property )
        return n;
    for( i = 0; i < MSI_MAX_PROPS; i++ )
        if( property[i].vt != VT_EMPTY )
            n++;
    return n;
}

static unsigned read_word( uint8_t *data, unsigned *ofs )
{
    unsigned val = 0;
    val = data[*ofs];
    val |= data[*ofs+1] << 8;
    *ofs += 2;
    return val;
}

static unsigned read_dword( uint8_t *data, unsigned *ofs )
{
    unsigned val = 0;
    val = data[*ofs];
    val |= data[*ofs+1] << 8;
    val |= data[*ofs+2] << 16;
    val |= data[*ofs+3] << 24;
    *ofs += 4;
    return val;
}

static void parse_filetime( const WCHAR *str, uint64_t *ft )
{
    struct tm tm;
    time_t t;
    const WCHAR *p = str;
    WCHAR *end;


    /* YYYY/MM/DD hh:mm:ss */

    while ( *p == ' ' || *p == '\t' ) p++;

    tm.tm_year = strtolW( p, &end, 10 );
    if (*end != '/') return;
    p = end + 1;

    tm.tm_mon = strtolW( p, &end, 10 ) - 1;
    if (*end != '/') return;
    p = end + 1;

    tm.tm_mday = strtolW( p, &end, 10 );
    if (*end != ' ') return;
    p = end + 1;

    while ( *p == ' ' || *p == '\t' ) p++;

    tm.tm_hour = strtolW( p, &end, 10 );
    if (*end != ':') return;
    p = end + 1;

    tm.tm_min = strtolW( p, &end, 10 );
    if (*end != ':') return;
    p = end + 1;

    tm.tm_sec = strtolW( p, &end, 10 );

    t = mktime(&tm);

    /* Add number of seconds between 1601-01-01 and 1970-01-01,
     * then convert to 100 ns units.
     */
    *ft = (t + 134774ULL * 86400ULL) * 10000000ULL;
}

/* FIXME: doesn't deal with endian conversion */
static void read_properties_from_data( PROPVARIANT *prop, uint8_t *data, unsigned sz, uint32_t cProperties )
{
    unsigned type;
    unsigned i;
    PROPVARIANT *property;
    uint32_t idofs, len;
    char *str;

    idofs = 8;

    /* now set all the properties */
    for( i = 0; i < cProperties; i++ )
    {
        int propid = read_dword(data, &idofs);
        int dwOffset = read_dword(data, &idofs);
        int proptype;

        if( propid >= MSI_MAX_PROPS )
        {
            ERR("Unknown property ID %d\n", propid );
            break;
        }

        type = get_type( propid );
        if( type == VT_EMPTY )
        {
            ERR("propid %d has unknown type\n", propid);
            break;
        }

        property = &prop[ propid ];

        if( dwOffset + 4 > sz )
        {
            ERR("not enough data for type %d %d \n", dwOffset, sz);
            break;
        }

        proptype = read_dword(data, &dwOffset);
        if( dwOffset + 4 > sz )
        {
            ERR("not enough data for type %d %d \n", dwOffset, sz);
            break;
        }

        property->vt = proptype;
        switch(proptype)
        {
        case VT_I2:
            property->iVal = read_dword(data, &dwOffset);
            break;
        case VT_I4:
            property->lVal = read_dword(data, &dwOffset);
            break;
        case VT_FILETIME:
            if( dwOffset + 8 > sz )
            {
                ERR("not enough data for type %d %d \n", dwOffset, sz);
                break;
            }
            property->filetime.dwLowDateTime = read_dword(data, &dwOffset);
            property->filetime.dwHighDateTime = read_dword(data, &dwOffset);
            break;
        case VT_LPSTR:
            len = read_dword(data, &dwOffset);
            if( dwOffset + len > sz )
            {
                ERR("not enough data for type %d %d %d \n", dwOffset, len, sz);
                break;
            }
            str = msi_alloc( len );
            memcpy( str, &data[dwOffset], len-1 );
            str[ len - 1 ] = 0;
            break;
        }

        /* check the type is the same as we expect */
        if( type == VT_LPSTR && proptype == VT_LPSTR)
           property->pszVal = str;
        else if (type == proptype)
           ;
        else if( proptype == VT_LPSTR) {
            if( type == VT_I2) {
                property->iVal = atoi( str );
            } else if( type == VT_I4) {
                property->lVal = atoi( str );
            } else if( type == VT_FILETIME) {
                WCHAR *wstr = strdupAtoW(str);
                uint64_t ft_value;
                parse_filetime( wstr, &ft_value );
                property->filetime.dwLowDateTime = ft_value & 0xFFFFFFFFULL;
                property->filetime.dwHighDateTime = ft_value >> 32;
                msi_free (wstr);
            }
            msi_free (str);
        }
        else
        {
            ERR("invalid type \n");
            break;
        }
    }
}

static unsigned load_summary_info( LibmsiSummaryInfo *si, IStream *stm )
{
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    uint8_t *data = NULL;
    unsigned ofs, dwOffset;
    unsigned count, size, cbSection, cProperties;
    HRESULT r;
    STATSTG stat;

    TRACE("%p %p\n", si, stm);

    r = IStream_Stat(stm, &stat, STATFLAG_NONAME);
    if (FAILED(r))
        return r;

    if (stat.cbSize.QuadPart >> 32)
    {
        ERR("Storage is too large\n");
        return ret;
    }

    size = stat.cbSize.QuadPart;
    data = msi_alloc(size);

    ofs = 0;
    r = IStream_Read( stm, data, size, &count );
    if( FAILED(r) || count != size )
        return ret;

    /* process the set header */
    if( read_word( data, &ofs) != 0xfffe )
    {
        ERR("property set not little-endian\n");
        return ret;
    }

    /* process the format header */

    /* check the format id is correct */
    ofs = 28;
    if( memcmp( &fmtid_SummaryInformation, &data[ofs], 16 ) )
        return ret;

    /* seek to the location of the section */
    ofs += 16;
    ofs = dwOffset = read_dword(data, &ofs);

    /* read the section itself */
    cbSection = read_dword( data, &ofs );
    cProperties = read_dword( data, &ofs );

    if( cProperties > MSI_MAX_PROPS )
    {
        ERR("too many properties %d\n", cProperties);
        return ret;
    }

    /* read all the data in one go */
    read_properties_from_data( si->property,
                               &data[dwOffset],
                               cbSection, cProperties );

    msi_free( data );
    return ret;
}

static unsigned write_word( uint8_t *data, unsigned ofs, unsigned val )
{
    if( data )
    {
        data[ofs++] = val&0xff;
        data[ofs++] = (val>>8)&0xff;
    }
    return 2;
}

static unsigned write_dword( uint8_t *data, unsigned ofs, unsigned val )
{
    if( data )
    {
        data[ofs++] = val&0xff;
        data[ofs++] = (val>>8)&0xff;
        data[ofs++] = (val>>16)&0xff;
        data[ofs++] = (val>>24)&0xff;
    }
    return 4;
}

static unsigned write_filetime( uint8_t *data, unsigned ofs, const uint64_t *ft )
{
    write_dword( data, ofs, (*ft) & 0xFFFFFFFFUL );
    write_dword( data, ofs + 4, (*ft) >> 32 );
    return 8;
}

static unsigned write_string( uint8_t *data, unsigned ofs, const char *str )
{
    unsigned len = strlen( str ) + 1;
    write_dword( data, ofs, len );
    if( data )
        memcpy( &data[ofs + 4], str, len );
    return (7 + len) & ~3;
}

static unsigned write_property_to_data( const PROPVARIANT *prop, uint8_t *data )
{
    unsigned sz = 0;

    if( prop->vt == VT_EMPTY )
        return sz;

    /* add the type */
    sz += write_dword( data, sz, prop->vt );
    switch( prop->vt )
    {
    case VT_I2:
        sz += write_dword( data, sz, prop->iVal );
        break;
    case VT_I4:
        sz += write_dword( data, sz, prop->lVal );
        break;
    case VT_FILETIME: {
        uint64_t i8 = prop->filetime.dwLowDateTime | ((uint64_t)prop->filetime.dwHighDateTime << 32);
        sz += write_filetime( data, sz, &i8 );
        break;
    }
    case VT_LPSTR:
        sz += write_string( data, sz, prop->pszVal );
        break;
    }
    return sz;
}

static unsigned suminfo_persist( LibmsiSummaryInfo *si )
{
    int cProperties, cbSection, dwOffset;
    IStream *stm;
    uint8_t *data = NULL;
    unsigned r, sz;
    int i;

    /* add up how much space the data will take and calculate the offsets */
    cProperties = get_property_count( si->property );
    cbSection = 8 + cProperties * 8;
    for( i = 0; i < MSI_MAX_PROPS; i++ )
        cbSection += write_property_to_data( &si->property[i], NULL );

    sz = 28 + 20 + cbSection;
    data = msi_alloc_zero( sz );

    /* write the set header */
    sz = 0;
    sz += write_word(data, sz, 0xfffe); /* wByteOrder */
    sz += write_word(data, sz, 0);      /* wFormat */
    sz += write_dword(data, sz, 0x00020005); /* dwOSVer - build 5, platform id 2 */
    sz += 16; /* clsID */
    sz += write_dword(data, sz, 1); /* reserved */

    /* write the format header */
    memcpy( &data[sz], &fmtid_SummaryInformation, 16 );
    sz += 16;

    sz += write_dword(data, sz, 28 + 20); /* dwOffset */
    assert(sz == 28 + 20);

    /* write the section header */
    sz += write_dword(data, sz, cbSection);
    sz += write_dword(data, sz, cProperties);
    assert(sz == 28 + 20 + 8);

    dwOffset = 8 + cProperties * 8;
    for( i = 0; i < MSI_MAX_PROPS; i++ )
    {
        int propsz = write_property_to_data( &si->property[i], NULL );
        if( !propsz )
            continue;
        sz += write_dword(data, sz, i);
        sz += write_dword(data, sz, dwOffset);
        dwOffset += propsz;
    }
    assert(dwOffset == cbSection);

    /* write out the data */
    for( i = 0; i < MSI_MAX_PROPS; i++ )
        sz += write_property_to_data( &si->property[i], &data[sz] );

    assert(sz == 28 + 20 + cbSection);

    r = write_raw_stream_data(si->database, szSumInfo, data, sz, &stm);
    if (r == 0) {
        IStream_Release( stm );
    }
    msi_free( data );
    return r;
}

LibmsiSummaryInfo *_libmsi_get_summary_information( LibmsiDatabase *db, unsigned uiUpdateCount )
{
    IStream *stm = NULL;
    LibmsiSummaryInfo *si;
    unsigned grfMode;
    unsigned r;

    TRACE("%p %d\n", db, uiUpdateCount );

    si = alloc_msiobject( sizeof (LibmsiSummaryInfo), _libmsi_summary_info_destroy );
    if( !si )
        return si;

    si->update_count = uiUpdateCount;
    msiobj_addref( &db->hdr );
    si->database = db;

    /* read the stream... if we fail, we'll start with an empty property set */
    grfMode = STGM_READ | STGM_SHARE_EXCLUSIVE;
    r = msi_get_raw_stream( db, szSumInfo, &stm );
    if( r == 0 )
    {
        load_summary_info( si, stm );
        IStream_Release( stm );
    }

    return si;
}

LibmsiResult libmsi_database_get_summary_info( LibmsiDatabase *db, 
              unsigned uiUpdateCount, LibmsiSummaryInfo **psi )
{
    LibmsiSummaryInfo *si;
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;

    TRACE("%d %d %p\n", db, uiUpdateCount, psi);

    if( !psi )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( !db )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &db->hdr);
    si = _libmsi_get_summary_information( db, uiUpdateCount );
    if (si)
    {
        *psi = si;
        ret = LIBMSI_RESULT_SUCCESS;
    }

    msiobj_release( &db->hdr );
    return ret;
}

LibmsiResult libmsi_summary_info_get_property_count(LibmsiSummaryInfo *si, unsigned *pCount)
{
    TRACE("%d %p\n", si, pCount);

    if( !si )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &si->hdr );
    if( pCount )
        *pCount = get_property_count( si->property );
    msiobj_release( &si->hdr );

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiResult libmsi_summary_info_get_property(
      LibmsiSummaryInfo *si, unsigned uiProperty, unsigned *puiDataType, int *piValue,
      uint64_t *pftValue, char *szValueBuf, unsigned *pcchValueBuf)
{
    PROPVARIANT *prop;
    unsigned ret = LIBMSI_RESULT_SUCCESS;

    TRACE("%d %d %p %p %p %p %p\n", si, uiProperty, puiDataType,
          piValue, pftValue, szValueBuf, pcchValueBuf);

    if ( uiProperty >= MSI_MAX_PROPS )
    {
        if (puiDataType) *puiDataType = LIBMSI_PROPERTY_TYPE_EMPTY;
        return LIBMSI_RESULT_UNKNOWN_PROPERTY;
    }

    if( !si )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &si->hdr );
    prop = &si->property[uiProperty];

    switch( prop->vt )
    {
    case VT_I2:
        if( puiDataType )
            *puiDataType = LIBMSI_PROPERTY_TYPE_INT;

        if( piValue )
            *piValue = prop->iVal;
        break;
    case VT_I4:
        if( puiDataType )
            *puiDataType = LIBMSI_PROPERTY_TYPE_INT;

        if( piValue )
            *piValue = prop->lVal;
        break;
    case VT_LPSTR:
        if( puiDataType )
            *puiDataType = LIBMSI_PROPERTY_TYPE_STRING;

        if( pcchValueBuf )
        {
            unsigned len = 0;

            len = strlen( prop->pszVal );
            if( szValueBuf )
                strcpynA(szValueBuf, prop->pszVal, *pcchValueBuf );
            if (len >= *pcchValueBuf)
                ret = LIBMSI_RESULT_MORE_DATA;
            *pcchValueBuf = len;
        }
        break;
    case VT_FILETIME:
        if( puiDataType )
            *puiDataType = LIBMSI_PROPERTY_TYPE_FILETIME;

        if( pftValue )
            *pftValue = prop->filetime.dwLowDateTime | ((uint64_t)prop->filetime.dwHighDateTime << 32);
        break;
    case VT_EMPTY:
        if( puiDataType )
            *puiDataType = LIBMSI_PROPERTY_TYPE_EMPTY;

        break;
    default:
        FIXME("Unknown property variant type\n");
        break;
    }
    msiobj_release( &si->hdr );
    return ret;
}

WCHAR *msi_suminfo_dup_string( LibmsiSummaryInfo *si, unsigned uiProperty )
{
    PROPVARIANT *prop;

    if ( uiProperty >= MSI_MAX_PROPS )
        return NULL;
    prop = &si->property[uiProperty];
    if( prop->vt != VT_LPSTR)
        return NULL;
    return strdupAtoW( prop->pszVal );
}

int msi_suminfo_get_int32( LibmsiSummaryInfo *si, unsigned uiProperty )
{
    PROPVARIANT *prop;

    if ( uiProperty >= MSI_MAX_PROPS )
        return -1;
    prop = &si->property[uiProperty];
    if( prop->vt != VT_I4 )
        return -1;
    return prop->lVal;
}

static LibmsiResult _libmsi_summary_info_set_property( LibmsiSummaryInfo *si, unsigned uiProperty,
               unsigned type, int iValue, uint64_t* pftValue, const char *szValue )
{
    PROPVARIANT *prop;
    unsigned len;
    unsigned ret;

    if( type == VT_LPSTR && !szValue )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( type == VT_FILETIME && !pftValue )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    msiobj_addref( &si->hdr);

    prop = &si->property[uiProperty];

    if( prop->vt == VT_EMPTY )
    {
        ret = LIBMSI_RESULT_FUNCTION_FAILED;
        if( !si->update_count )
            goto end;

        si->update_count--;
    }
    else if( prop->vt != type )
    {
        ret = LIBMSI_RESULT_SUCCESS;
        goto end;
    }

    free_prop( prop );
    prop->vt = type;
    switch( type )
    {
    case VT_I4:
        prop->lVal = iValue;
        break;
    case VT_I2:
        prop->iVal = iValue;
        break;
    case VT_FILETIME:
        prop->filetime.dwLowDateTime = (unsigned) (*pftValue);
        prop->filetime.dwHighDateTime = (unsigned) (*pftValue >> 32);
        break;
    case VT_LPSTR:
        len = strlen( szValue ) + 1;
        prop->pszVal = msi_alloc( len );
        strcpy( prop->pszVal, szValue );
        break;
    }

    ret = LIBMSI_RESULT_SUCCESS;
end:
    msiobj_release( &si->hdr );
    return ret;
}

LibmsiResult libmsi_summary_info_set_property( LibmsiSummaryInfo *si, unsigned uiProperty,
               unsigned uiDataType, int iValue, uint64_t* pftValue, const char *szValue )
{
    int type;

    TRACE("%p %u %u %i %p %p\n", si, uiProperty, type, iValue,
          pftValue, szValue );

    if( !si )
        return LIBMSI_RESULT_INVALID_HANDLE;

    type = get_type( uiProperty );
    switch (type) {
    case VT_EMPTY:
        return LIBMSI_RESULT_DATATYPE_MISMATCH;
    case VT_I2:
    case VT_I4:
        if (uiDataType != LIBMSI_PROPERTY_TYPE_INT) {
            return LIBMSI_RESULT_DATATYPE_MISMATCH;
        }
        break;
    case VT_LPSTR:
        if (uiDataType != LIBMSI_PROPERTY_TYPE_STRING) {
            return LIBMSI_RESULT_DATATYPE_MISMATCH;
        }
        break;
    case VT_FILETIME:
        if (uiDataType != LIBMSI_PROPERTY_TYPE_FILETIME) {
            return LIBMSI_RESULT_DATATYPE_MISMATCH;
        }
        break;
    }

    return _libmsi_summary_info_set_property( si, uiProperty, type, iValue, pftValue, szValue );
}

static unsigned parse_prop( const WCHAR *prop, const WCHAR *value, unsigned *pid, int *int_value,
                        uint64_t *ft_value, char **str_value )
{
    *pid = atoiW( prop );
    switch (*pid)
    {
    case MSI_PID_CODEPAGE:
    case MSI_PID_WORDCOUNT:
    case MSI_PID_CHARCOUNT:
    case MSI_PID_SECURITY:
    case MSI_PID_PAGECOUNT:
        *int_value = atoiW( value );
        break;

    case MSI_PID_LASTPRINTED:
    case MSI_PID_CREATE_DTM:
    case MSI_PID_LASTSAVE_DTM:
        parse_filetime( value, ft_value );
        break;

    case MSI_PID_SUBJECT:
    case MSI_PID_AUTHOR:
    case MSI_PID_KEYWORDS:
    case MSI_PID_COMMENTS:
    case MSI_PID_TEMPLATE:
    case MSI_PID_LASTAUTHOR:
    case MSI_PID_REVNUMBER:
    case MSI_PID_APPNAME:
    case MSI_PID_TITLE:
        *str_value = strdupWtoA(value);
        break;

    default:
        WARN("unhandled prop id %u\n", *pid);
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    return LIBMSI_RESULT_SUCCESS;
}

unsigned msi_add_suminfo( LibmsiDatabase *db, WCHAR ***records, int num_records, int num_columns )
{
    unsigned r = LIBMSI_RESULT_FUNCTION_FAILED;
    unsigned i, j;
    LibmsiSummaryInfo *si;

    si = _libmsi_get_summary_information( db, num_records * (num_columns / 2) );
    if (!si)
    {
        ERR("no summary information!\n");
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    for (i = 0; i < num_records; i++)
    {
        for (j = 0; j < num_columns; j += 2)
        {
            unsigned pid;
            int int_value = 0;
            uint64_t ft_value;
            char *str_value = NULL;

            r = parse_prop( records[i][j], records[i][j + 1], &pid, &int_value, &ft_value, &str_value );
            if (r != LIBMSI_RESULT_SUCCESS)
                goto end;

            assert( get_type(pid) != VT_EMPTY );
            r = _libmsi_summary_info_set_property( si, pid, get_type(pid), int_value, &ft_value, str_value );
            if (r != LIBMSI_RESULT_SUCCESS)
                goto end;

            msi_free(str_value);
        }
    }

end:
    if (r == LIBMSI_RESULT_SUCCESS)
        r = suminfo_persist( si );

    msiobj_release( &si->hdr );
    return r;
}

LibmsiResult libmsi_summary_info_persist( LibmsiSummaryInfo *si )
{
    unsigned ret;

    TRACE("%d\n", si );

    if( !si )
        return LIBMSI_RESULT_INVALID_HANDLE;

    msiobj_addref( &si->hdr);
    ret = suminfo_persist( si );
    msiobj_release( &si->hdr );
    return ret;
}
