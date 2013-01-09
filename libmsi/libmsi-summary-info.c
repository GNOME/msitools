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

#include "libmsi-summary-info.h"

#include "debug.h"
#include "libmsi.h"
#include "msipriv.h"


enum
{
    PROP_0,

    PROP_DATABASE,
    PROP_UPDATE_COUNT,
};

G_DEFINE_TYPE (LibmsiSummaryInfo, libmsi_summary_info, G_TYPE_OBJECT);

static const char szSumInfo[] = "\5SummaryInformation";
static const uint8_t fmtid_SummaryInformation[16] =
        { 0xe0, 0x85, 0x9f, 0xf2, 0xf9, 0x4f, 0x68, 0x10, 0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9};

static unsigned load_summary_info( LibmsiSummaryInfo *si, GsfInput *stm );

static void
libmsi_summary_info_init (LibmsiSummaryInfo *p)
{
}

static void
free_prop (LibmsiOLEVariant *prop)
{
    if (prop->vt == OLEVT_LPSTR)
        msi_free (prop->strval);
    prop->vt = OLEVT_EMPTY;
}

static void
libmsi_summary_info_finalize (GObject *object)
{
    LibmsiSummaryInfo *p = LIBMSI_SUMMARY_INFO (object);
    unsigned i;

    for (i = 0; i < MSI_MAX_PROPS; i++)
        free_prop (&p->property[i]);

    if (p->database)
        g_object_unref (p->database);

    G_OBJECT_CLASS (libmsi_summary_info_parent_class)->finalize (object);
}

static void
summary_info_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (LIBMSI_IS_SUMMARY_INFO (object));
    LibmsiSummaryInfo *p = LIBMSI_SUMMARY_INFO (object);

    switch (prop_id) {
    case PROP_DATABASE:
        g_return_if_fail (p->database == NULL);
        p->database = g_value_dup_object (value);
        break;
    case PROP_UPDATE_COUNT:
        p->update_count = g_value_get_uint (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
summary_info_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (LIBMSI_IS_SUMMARY_INFO (object));
    LibmsiSummaryInfo *p = LIBMSI_SUMMARY_INFO (object);

    switch (prop_id) {
    case PROP_DATABASE:
        g_value_set_object (value, p->database);
        break;
    case PROP_UPDATE_COUNT:
        g_value_set_uint (value, p->update_count);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
libmsi_summary_info_constructed (GObject *object)
{
    LibmsiSummaryInfo *self = LIBMSI_SUMMARY_INFO (object);
    LibmsiSummaryInfo *p = self;
    GsfInput *stm = NULL;
    unsigned r;

    /* read the stream... if we fail, we'll start with an empty property set */
    r = msi_get_raw_stream (p->database, szSumInfo, &stm);
    if (r == 0) {
        load_summary_info (self, stm);
        g_object_unref (G_OBJECT (stm));
    }

    G_OBJECT_CLASS (libmsi_summary_info_parent_class)->constructed (object);
}

static void
libmsi_summary_info_class_init (LibmsiSummaryInfoClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = libmsi_summary_info_finalize;
    object_class->set_property = summary_info_set_property;
    object_class->get_property = summary_info_get_property;
    object_class->constructed = libmsi_summary_info_constructed;

    g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_DATABASE,
        g_param_spec_object ("database", "database", "database", LIBMSI_TYPE_DATABASE,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_UPDATE_COUNT,
        g_param_spec_uint ("update-count", "update-count", "update-count", 0, G_MAXUINT, 0,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS));
}

static unsigned get_type( unsigned uiProperty )
{
    switch( uiProperty )
    {
    case LIBMSI_PROPERTY_CODEPAGE:
         return OLEVT_I2;

    case LIBMSI_PROPERTY_SUBJECT:
    case LIBMSI_PROPERTY_AUTHOR:
    case LIBMSI_PROPERTY_KEYWORDS:
    case LIBMSI_PROPERTY_COMMENTS:
    case LIBMSI_PROPERTY_TEMPLATE:
    case LIBMSI_PROPERTY_LASTAUTHOR:
    case LIBMSI_PROPERTY_UUID:
    case LIBMSI_PROPERTY_APPNAME:
    case LIBMSI_PROPERTY_TITLE:
         return OLEVT_LPSTR;

    case LIBMSI_PROPERTY_LASTPRINTED:
    case LIBMSI_PROPERTY_CREATED_TM:
    case LIBMSI_PROPERTY_LASTSAVED_TM:
         return OLEVT_FILETIME;

    case LIBMSI_PROPERTY_SOURCE:
    case LIBMSI_PROPERTY_RESTRICT:
    case LIBMSI_PROPERTY_SECURITY:
    case LIBMSI_PROPERTY_VERSION:
         return OLEVT_I4;
    }
    return OLEVT_EMPTY;
}

static unsigned get_property_count( const LibmsiOLEVariant *property )
{
    unsigned i, n = 0;

    if( !property )
        return n;
    for( i = 0; i < MSI_MAX_PROPS; i++ )
        if( property[i].vt != OLEVT_EMPTY )
            n++;
    return n;
}

static unsigned read_word( const uint8_t *data, unsigned *ofs )
{
    unsigned val = 0;
    val = data[*ofs];
    val |= data[*ofs+1] << 8;
    *ofs += 2;
    return val;
}

static unsigned read_dword( const uint8_t *data, unsigned *ofs )
{
    unsigned val = 0;
    val = data[*ofs];
    val |= data[*ofs+1] << 8;
    val |= data[*ofs+2] << 16;
    val |= data[*ofs+3] << 24;
    *ofs += 4;
    return val;
}

static void parse_filetime( const char *str, guint64 *ft )
{
    /* set to 0, tm_isdst can make the result vary: */
    struct tm tm = { 0, };
    time_t t;
    const char *p = str;
    char *end;


    /* YYYY/MM/DD hh:mm:ss */

    while ( *p == ' ' || *p == '\t' ) p++;

    tm.tm_year = strtol( p, &end, 10 );
    if (*end != '/') return;
    p = end + 1;

    tm.tm_mon = strtol( p, &end, 10 ) - 1;
    if (*end != '/') return;
    p = end + 1;

    tm.tm_mday = strtol( p, &end, 10 );
    if (*end != ' ') return;
    p = end + 1;

    while ( *p == ' ' || *p == '\t' ) p++;

    tm.tm_hour = strtol( p, &end, 10 );
    if (*end != ':') return;
    p = end + 1;

    tm.tm_min = strtol( p, &end, 10 );
    if (*end != ':') return;
    p = end + 1;

    tm.tm_sec = strtol( p, &end, 10 );

    t = mktime(&tm);

    /* Add number of seconds between 1601-01-01 and 1970-01-01,
     * then convert to 100 ns units.
     */
    *ft = (t + 134774ULL * 86400ULL) * 10000000ULL;
}

/* FIXME: doesn't deal with endian conversion */
static void read_properties_from_data( LibmsiOLEVariant *prop, const uint8_t *data, unsigned sz, uint32_t cProperties )
{
    unsigned type;
    unsigned i;
    LibmsiOLEVariant *property;
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
        if( type == OLEVT_EMPTY )
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
        case OLEVT_I2:
        case OLEVT_I4:
            property->intval = read_dword(data, &dwOffset);
            break;
        case OLEVT_FILETIME:
            if( dwOffset + 8 > sz )
            {
                ERR("not enough data for type %d %d \n", dwOffset, sz);
                break;
            }
            property->filetime = read_dword(data, &dwOffset);
            property->filetime |= (guint64)read_dword(data, &dwOffset) << 32;
            break;
        case OLEVT_LPSTR:
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
        if( type == OLEVT_LPSTR && proptype == OLEVT_LPSTR)
           property->strval = str;
        else if (type == proptype)
           ;
        else if( proptype == OLEVT_LPSTR) {
            if( type == OLEVT_I2 || type == OLEVT_I4) {
                property->intval = atoi( str );
            } else if( type == OLEVT_FILETIME) {
                parse_filetime( str, &property->filetime );
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

static unsigned load_summary_info( LibmsiSummaryInfo *si, GsfInput *stm )
{
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    uint8_t *data = NULL;
    unsigned ofs, dwOffset;
    unsigned cbSection, cProperties;
    off_t sz;

    TRACE("%p %p\n", si, stm);

    sz = gsf_input_size(stm);
    if (!sz)
        return ret;
    data = g_try_malloc(gsf_input_size(stm));
    if (!data)
        return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;

    if (!gsf_input_read(stm, sz, data))
        goto done;

    /* process the set header */
    ofs = 0;
    if( read_word( data, &ofs) != 0xfffe )
    {
        ERR("property set not little-endian\n");
        goto done;
    }

    /* process the format header */

    /* check the format id is correct */
    ofs = 28;
    if( memcmp( &fmtid_SummaryInformation, &data[ofs], 16 ) )
        goto done;

    /* seek to the location of the section */
    ofs += 16;
    ofs = dwOffset = read_dword(data, &ofs);

    /* read the section itself */
    cbSection = read_dword( data, &ofs );
    cProperties = read_dword( data, &ofs );

    if( cProperties > MSI_MAX_PROPS )
    {
        ERR("too many properties %d\n", cProperties);
        goto done;
    }

    /* read all the data in one go */
    read_properties_from_data( si->property,
                               &data[dwOffset],
                               cbSection, cProperties );

done:
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

static unsigned write_filetime( uint8_t *data, unsigned ofs, const guint64 *ft )
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

static unsigned write_property_to_data( const LibmsiOLEVariant *prop, uint8_t *data )
{
    unsigned sz = 0;

    if( prop->vt == OLEVT_EMPTY )
        return sz;

    /* add the type */
    sz += write_dword( data, sz, prop->vt );
    switch( prop->vt )
    {
    case OLEVT_I2:
    case OLEVT_I4:
        sz += write_dword( data, sz, prop->intval );
        break;
    case OLEVT_FILETIME: {
        sz += write_filetime( data, sz, &prop->filetime );
        break;
    }
    case OLEVT_LPSTR:
        sz += write_string( data, sz, prop->strval );
        break;
    }
    return sz;
}

static unsigned suminfo_persist( LibmsiSummaryInfo *si )
{
    int cProperties, cbSection, dwOffset;
    GsfInput *stm;
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
        g_object_unref(G_OBJECT(stm));
    }
    msi_free( data );
    return r;
}

LibmsiSummaryInfo *_libmsi_get_summary_information( LibmsiDatabase *db, unsigned uiUpdateCount )
{
    GsfInput *stm = NULL;
    LibmsiSummaryInfo *si;
    unsigned r;

    TRACE("%p %d\n", db, uiUpdateCount );

    si = libmsi_summary_info_new (db, uiUpdateCount, NULL);

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

    g_object_ref(db);
    si = _libmsi_get_summary_information( db, uiUpdateCount );
    if (si)
    {
        *psi = si;
        ret = LIBMSI_RESULT_SUCCESS;
    }

    g_object_unref(db);
    return ret;
}

LibmsiResult libmsi_summary_info_get_property_count(LibmsiSummaryInfo *si, unsigned *pCount)
{
    TRACE("%d %p\n", si, pCount);

    if( !si )
        return LIBMSI_RESULT_INVALID_HANDLE;

    g_object_ref(si);
    if( pCount )
        *pCount = get_property_count( si->property );
    g_object_unref(si);

    return LIBMSI_RESULT_SUCCESS;
}

LibmsiResult libmsi_summary_info_get_property(
      LibmsiSummaryInfo *si, unsigned uiProperty, unsigned *puiDataType, int *pintvalue,
      guint64 *pftValue, char *szValueBuf, unsigned *pcchValueBuf)
{
    LibmsiOLEVariant *prop;
    unsigned ret = LIBMSI_RESULT_SUCCESS;

    TRACE("%d %d %p %p %p %p %p\n", si, uiProperty, puiDataType,
          pintvalue, pftValue, szValueBuf, pcchValueBuf);

    if ( uiProperty >= MSI_MAX_PROPS )
    {
        if (puiDataType) *puiDataType = LIBMSI_PROPERTY_TYPE_EMPTY;
        return LIBMSI_RESULT_UNKNOWN_PROPERTY;
    }

    if( !si )
        return LIBMSI_RESULT_INVALID_HANDLE;

    g_object_ref(si);
    prop = &si->property[uiProperty];

    switch( prop->vt )
    {
    case OLEVT_I2:
    case OLEVT_I4:
        if( puiDataType )
            *puiDataType = LIBMSI_PROPERTY_TYPE_INT;

        if( pintvalue )
            *pintvalue = prop->intval;
        break;
    case OLEVT_LPSTR:
        if( puiDataType )
            *puiDataType = LIBMSI_PROPERTY_TYPE_STRING;

        if( pcchValueBuf )
        {
            unsigned len = 0;

            len = strlen( prop->strval );
            if( szValueBuf )
                strcpyn(szValueBuf, prop->strval, *pcchValueBuf );
            if (len >= *pcchValueBuf)
                ret = LIBMSI_RESULT_MORE_DATA;
            *pcchValueBuf = len;
        }
        break;
    case OLEVT_FILETIME:
        if( puiDataType )
            *puiDataType = LIBMSI_PROPERTY_TYPE_FILETIME;

        if( pftValue )
            *pftValue = prop->filetime;
        break;
    case OLEVT_EMPTY:
        if( puiDataType )
            *puiDataType = LIBMSI_PROPERTY_TYPE_EMPTY;

        break;
    default:
        FIXME("Unknown property variant type\n");
        break;
    }
    g_object_unref(si);
    return ret;
}

static LibmsiResult _libmsi_summary_info_set_property( LibmsiSummaryInfo *si, unsigned uiProperty,
               unsigned type, int intvalue, guint64* pftValue, const char *szValue )
{
    LibmsiOLEVariant *prop;
    unsigned len;
    unsigned ret;

    if( type == OLEVT_LPSTR && !szValue )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if( type == OLEVT_FILETIME && !pftValue )
        return LIBMSI_RESULT_INVALID_PARAMETER;

    g_object_ref(si);

    prop = &si->property[uiProperty];

    if( prop->vt == OLEVT_EMPTY )
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
    case OLEVT_I2:
    case OLEVT_I4:
        prop->intval = intvalue;
        break;
    case OLEVT_FILETIME:
        prop->filetime = *pftValue;
        break;
    case OLEVT_LPSTR:
        len = strlen( szValue ) + 1;
        prop->strval = msi_alloc( len );
        strcpy( prop->strval, szValue );
        break;
    }

    ret = LIBMSI_RESULT_SUCCESS;
end:
    g_object_unref(si);
    return ret;
}

LibmsiResult libmsi_summary_info_set_property( LibmsiSummaryInfo *si, unsigned uiProperty,
               unsigned uiDataType, int intvalue, guint64* pftValue, const char *szValue )
{
    int type;

    TRACE("%p %u %u %i %p %p\n", si, uiProperty, type, intvalue,
          pftValue, szValue );

    if( !si )
        return LIBMSI_RESULT_INVALID_HANDLE;

    type = get_type( uiProperty );
    switch (type) {
    case OLEVT_EMPTY:
        return LIBMSI_RESULT_DATATYPE_MISMATCH;
    case OLEVT_I2:
    case OLEVT_I4:
        if (uiDataType != LIBMSI_PROPERTY_TYPE_INT) {
            return LIBMSI_RESULT_DATATYPE_MISMATCH;
        }
        break;
    case OLEVT_LPSTR:
        if (uiDataType != LIBMSI_PROPERTY_TYPE_STRING) {
            return LIBMSI_RESULT_DATATYPE_MISMATCH;
        }
        break;
    case OLEVT_FILETIME:
        if (uiDataType != LIBMSI_PROPERTY_TYPE_FILETIME) {
            return LIBMSI_RESULT_DATATYPE_MISMATCH;
        }
        break;
    }

    return _libmsi_summary_info_set_property( si, uiProperty, type, intvalue, pftValue, szValue );
}

static unsigned parse_prop( const char *prop, const char *value, unsigned *pid, int *int_value,
                        guint64 *ft_value, char **str_value )
{
    *pid = atoi( prop );
    switch (*pid)
    {
    case LIBMSI_PROPERTY_CODEPAGE:
    case LIBMSI_PROPERTY_SOURCE:
    case LIBMSI_PROPERTY_RESTRICT:
    case LIBMSI_PROPERTY_SECURITY:
    case LIBMSI_PROPERTY_VERSION:
        *int_value = atoi( value );
        break;

    case LIBMSI_PROPERTY_LASTPRINTED:
    case LIBMSI_PROPERTY_CREATED_TM:
    case LIBMSI_PROPERTY_LASTSAVED_TM:
        parse_filetime( value, ft_value );
        break;

    case LIBMSI_PROPERTY_SUBJECT:
    case LIBMSI_PROPERTY_AUTHOR:
    case LIBMSI_PROPERTY_KEYWORDS:
    case LIBMSI_PROPERTY_COMMENTS:
    case LIBMSI_PROPERTY_TEMPLATE:
    case LIBMSI_PROPERTY_LASTAUTHOR:
    case LIBMSI_PROPERTY_UUID:
    case LIBMSI_PROPERTY_APPNAME:
    case LIBMSI_PROPERTY_TITLE:
        *str_value = strdup(value);
        break;

    default:
        WARN("unhandled prop id %u\n", *pid);
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    return LIBMSI_RESULT_SUCCESS;
}

unsigned msi_add_suminfo( LibmsiDatabase *db, char ***records, int num_records, int num_columns )
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
            guint64 ft_value;
            char *str_value = NULL;

            r = parse_prop( records[i][j], records[i][j + 1], &pid, &int_value, &ft_value, &str_value );
            if (r != LIBMSI_RESULT_SUCCESS)
                goto end;

            assert( get_type(pid) != OLEVT_EMPTY );
            r = _libmsi_summary_info_set_property( si, pid, get_type(pid), int_value, &ft_value, str_value );
            if (r != LIBMSI_RESULT_SUCCESS)
                goto end;

            msi_free(str_value);
        }
    }

end:
    if (r == LIBMSI_RESULT_SUCCESS)
        r = suminfo_persist( si );

    g_object_unref(si);
    return r;
}

LibmsiResult libmsi_summary_info_persist( LibmsiSummaryInfo *si )
{
    unsigned ret;

    TRACE("%d\n", si );

    if( !si )
        return LIBMSI_RESULT_INVALID_HANDLE;

    g_object_ref(si);
    ret = suminfo_persist( si );
    g_object_unref(si);

    return ret;
}

LibmsiSummaryInfo*
libmsi_summary_info_new (LibmsiDatabase *database, unsigned update_count, GError **error)
{
    LibmsiSummaryInfo *self;

    self = g_object_new (LIBMSI_TYPE_SUMMARY_INFO,
                         "database", database,
                         "update-count", update_count,
                         NULL);

    return self;
}
