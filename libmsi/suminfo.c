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


const CLSID FMTID_SummaryInformation =
        { 0xf29f85e0, 0x4ff9, 0x1068, {0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9}};

#include "pshpack1.h"

typedef struct { 
    WORD wByteOrder;
    WORD wFormat;
    unsigned dwOSVer;
    CLSID clsID;
    unsigned reserved;
} PROPERTYSETHEADER;

typedef struct { 
    FMTID fmtid;
    unsigned dwOffset;
} FORMATIDOFFSET;

typedef struct { 
    unsigned cbSection;
    unsigned cProperties;
} PROPERTYSECTIONHEADER; 
 
typedef struct { 
    unsigned propid;
    unsigned dwOffset;
} PROPERTYIDOFFSET; 

typedef struct {
    unsigned type;
    union {
        int i4;
        short i2;
        uint64_t ft;
        struct {
            unsigned len;
            uint8_t str[1];
        } str;
    } u;
} PROPERTY_DATA;
 
#include "poppack.h"

static HRESULT (WINAPI *pPropVariantChangeType)
    (PROPVARIANT *ppropvarDest, REFPROPVARIANT propvarSrc,
     PROPVAR_CHANGE_FLAGS flags, VARTYPE vt);

#define SECT_HDR_SIZE (sizeof(PROPERTYSECTIONHEADER))

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

static unsigned propvar_changetype(PROPVARIANT *changed, PROPVARIANT *property, VARTYPE vt)
{
    HRESULT hr;
    HMODULE propsys = LoadLibraryA("propsys.dll");
    pPropVariantChangeType = (void *)GetProcAddress(propsys, "PropVariantChangeType");

    if (!pPropVariantChangeType)
    {
        ERR("PropVariantChangeType function missing!\n");
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    hr = pPropVariantChangeType(changed, property, 0, vt);
    return (hr == S_OK) ? LIBMSI_RESULT_SUCCESS : LIBMSI_RESULT_FUNCTION_FAILED;
}

/* FIXME: doesn't deal with endian conversion */
static void read_properties_from_data( PROPVARIANT *prop, uint8_t *data, unsigned sz )
{
    unsigned type;
    unsigned i, size;
    PROPERTY_DATA *propdata;
    PROPVARIANT property;
    PROPVARIANT *ptr;
    PROPVARIANT changed;
    PROPERTYIDOFFSET *idofs;
    PROPERTYSECTIONHEADER *section_hdr;

    section_hdr = (PROPERTYSECTIONHEADER*) &data[0];
    idofs = (PROPERTYIDOFFSET*) &data[SECT_HDR_SIZE];

    /* now set all the properties */
    for( i = 0; i < section_hdr->cProperties; i++ )
    {
        if( idofs[i].propid >= MSI_MAX_PROPS )
        {
            ERR("Unknown property ID %d\n", idofs[i].propid );
            break;
        }

        type = get_type( idofs[i].propid );
        if( type == VT_EMPTY )
        {
            ERR("propid %d has unknown type\n", idofs[i].propid);
            break;
        }

        propdata = (PROPERTY_DATA*) &data[ idofs[i].dwOffset ];

        /* check we don't run off the end of the data */
        size = sz - idofs[i].dwOffset - sizeof(unsigned);
        if( sizeof(unsigned) > size ||
            ( propdata->type == VT_FILETIME && sizeof(uint64_t) > size ) ||
            ( propdata->type == VT_LPSTR && (propdata->u.str.len + sizeof(unsigned)) > size ) )
        {
            ERR("not enough data\n");
            break;
        }

        property.vt = propdata->type;
        if( propdata->type == VT_LPSTR)
        {
            char *str = msi_alloc( propdata->u.str.len );
            memcpy( str, propdata->u.str.str, propdata->u.str.len );
            str[ propdata->u.str.len - 1 ] = 0;
            property.pszVal = str;
        }
        else if( propdata->type == VT_FILETIME )
	{
            property.filetime.dwLowDateTime = (unsigned) (propdata->u.ft & 0xFFFFFFFFULL);
            property.filetime.dwHighDateTime = (unsigned) (propdata->u.ft >> 32);
	}
        else if( propdata->type == VT_I2 )
            property.iVal = propdata->u.i2;
        else if( propdata->type == VT_I4 )
            property.lVal = propdata->u.i4;

        /* check the type is the same as we expect */
        if( type != propdata->type )
        {
            propvar_changetype(&changed, &property, type);
            ptr = &changed;
        }
        else
            ptr = &property;

        prop[ idofs[i].propid ] = *ptr;
    }
}

static unsigned load_summary_info( LibmsiSummaryInfo *si, IStream *stm )
{
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    PROPERTYSETHEADER set_hdr;
    FORMATIDOFFSET format_hdr;
    PROPERTYSECTIONHEADER section_hdr;
    uint8_t *data = NULL;
    LARGE_INTEGER ofs;
    unsigned count, sz;
    HRESULT r;

    TRACE("%p %p\n", si, stm);

    /* read the header */
    sz = sizeof set_hdr;
    r = IStream_Read( stm, &set_hdr, sz, &count );
    if( FAILED(r) || count != sz )
        return ret;

    if( set_hdr.wByteOrder != 0xfffe )
    {
        ERR("property set not big-endian %04X\n", set_hdr.wByteOrder);
        return ret;
    }

    sz = sizeof format_hdr;
    r = IStream_Read( stm, &format_hdr, sz, &count );
    if( FAILED(r) || count != sz )
        return ret;

    /* check the format id is correct */
    if( !IsEqualGUID( &FMTID_SummaryInformation, &format_hdr.fmtid ) )
        return ret;

    /* seek to the location of the section */
    ofs.QuadPart = format_hdr.dwOffset;
    r = IStream_Seek( stm, ofs, STREAM_SEEK_SET, NULL );
    if( FAILED(r) )
        return ret;

    /* read the section itself */
    sz = SECT_HDR_SIZE;
    r = IStream_Read( stm, &section_hdr, sz, &count );
    if( FAILED(r) || count != sz )
        return ret;

    if( section_hdr.cProperties > MSI_MAX_PROPS )
    {
        ERR("too many properties %d\n", section_hdr.cProperties);
        return ret;
    }

    data = msi_alloc( section_hdr.cbSection);
    if( !data )
        return ret;

    memcpy( data, &section_hdr, SECT_HDR_SIZE );

    /* read all the data in one go */
    sz = section_hdr.cbSection - SECT_HDR_SIZE;
    r = IStream_Read( stm, &data[ SECT_HDR_SIZE ], sz, &count );
    if( SUCCEEDED(r) && count == sz )
        read_properties_from_data( si->property, data, sz + SECT_HDR_SIZE );
    else
        ERR("failed to read properties %d %d\n", count, sz);

    msi_free( data );
    return ret;
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

static unsigned save_summary_info( const LibmsiSummaryInfo * si, IStream *stm )
{
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    PROPERTYSETHEADER set_hdr;
    FORMATIDOFFSET format_hdr;
    PROPERTYSECTIONHEADER section_hdr;
    PROPERTYIDOFFSET idofs[MSI_MAX_PROPS];
    uint8_t *data = NULL;
    unsigned count, sz;
    HRESULT r;
    int i;

    /* write the header */
    sz = sizeof set_hdr;
    memset( &set_hdr, 0, sz );
    set_hdr.wByteOrder = 0xfffe;
    set_hdr.wFormat = 0;
    set_hdr.dwOSVer = 0x00020005; /* build 5, platform id 2 */
    /* set_hdr.clsID is {00000000-0000-0000-0000-000000000000} */
    set_hdr.reserved = 1;
    r = IStream_Write( stm, &set_hdr, sz, &count );
    if( FAILED(r) || count != sz )
        return ret;

    /* write the format header */
    sz = sizeof format_hdr;
    format_hdr.fmtid = FMTID_SummaryInformation;
    format_hdr.dwOffset = sizeof format_hdr + sizeof set_hdr;
    r = IStream_Write( stm, &format_hdr, sz, &count );
    if( FAILED(r) || count != sz )
        return ret;

    /* add up how much space the data will take and calculate the offsets */
    section_hdr.cbSection = sizeof section_hdr;
    section_hdr.cbSection += (get_property_count( si->property ) * sizeof idofs[0]);
    section_hdr.cProperties = 0;
    for( i = 0; i < MSI_MAX_PROPS; i++ )
    {
        sz = write_property_to_data( &si->property[i], NULL );
        if( !sz )
            continue;
        idofs[ section_hdr.cProperties ].propid = i;
        idofs[ section_hdr.cProperties ].dwOffset = section_hdr.cbSection;
        section_hdr.cProperties++;
        section_hdr.cbSection += sz;
    }

    data = msi_alloc_zero( section_hdr.cbSection );

    sz = 0;
    memcpy( &data[sz], &section_hdr, sizeof section_hdr );
    sz += sizeof section_hdr;

    memcpy( &data[sz], idofs, section_hdr.cProperties * sizeof idofs[0] );
    sz += section_hdr.cProperties * sizeof idofs[0];

    /* write out the data */
    for( i = 0; i < MSI_MAX_PROPS; i++ )
        sz += write_property_to_data( &si->property[i], &data[sz] );

    r = IStream_Write( stm, data, sz, &count );
    msi_free( data );
    if( FAILED(r) || count != sz )
        return ret;

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiSummaryInfo *_libmsi_get_summary_information( LibmsiDatabase *db, unsigned uiUpdateCount )
{
    IStream *stm = NULL;
    LibmsiSummaryInfo *si;
    unsigned grfMode;
    HRESULT r;

    TRACE("%p %d\n", db, uiUpdateCount );

    si = alloc_msiobject( sizeof (LibmsiSummaryInfo), _libmsi_summary_info_destroy );
    if( !si )
        return si;

    si->update_count = uiUpdateCount;
    msiobj_addref( &db->hdr );
    si->database = db;

    /* read the stream... if we fail, we'll start with an empty property set */
    grfMode = STGM_READ | STGM_SHARE_EXCLUSIVE;
    r = IStorage_OpenStream( db->storage, szSumInfo, 0, grfMode, 0, &stm );
    if( SUCCEEDED(r) )
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

static unsigned suminfo_persist( LibmsiSummaryInfo *si )
{
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    IStream *stm = NULL;
    unsigned grfMode;
    HRESULT r;

    grfMode = STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE;
    r = IStorage_CreateStream( si->database->storage, szSumInfo, grfMode, 0, 0, &stm );
    if( SUCCEEDED(r) )
    {
        ret = save_summary_info( si, stm );
        IStream_Release( stm );
    }
    return ret;
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
