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
#include "config.h"

#ifdef USE_GMTIME_S
#   define gmtime_r(timep, result) gmtime_s(result, timep)
#endif



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
libmsi_summary_info_init (LibmsiSummaryInfo *self)
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
    LibmsiSummaryInfo *self = LIBMSI_SUMMARY_INFO (object);
    unsigned i;

    for (i = 0; i < MSI_MAX_PROPS; i++)
        free_prop (&self->property[i]);

    if (self->database)
        g_object_unref (self->database);

    G_OBJECT_CLASS (libmsi_summary_info_parent_class)->finalize (object);
}

static void
summary_info_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (LIBMSI_IS_SUMMARY_INFO (object));
    LibmsiSummaryInfo *self = LIBMSI_SUMMARY_INFO (object);

    switch (prop_id) {
    case PROP_DATABASE:
        g_return_if_fail (self->database == NULL);
        self->database = g_value_dup_object (value);
        break;
    case PROP_UPDATE_COUNT:
        self->update_count = g_value_get_uint (value);
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
    LibmsiSummaryInfo *self = LIBMSI_SUMMARY_INFO (object);

    switch (prop_id) {
    case PROP_DATABASE:
        g_value_set_object (value, self->database);
        break;
    case PROP_UPDATE_COUNT:
        g_value_set_uint (value, self->update_count);
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
    GsfInput *stm = NULL;
    unsigned r;

    /* read the stream... if we fail, we'll start with an empty property set */
    if (self->database) {
        r = msi_get_raw_stream (self->database, szSumInfo, &stm);
        if (r == 0) {
            load_summary_info (self, stm);
            g_object_unref (G_OBJECT (stm));
        }
    }

    if (G_OBJECT_CLASS (libmsi_summary_info_parent_class)->constructed)
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

    case LIBMSI_PROPERTY_EDITTIME:
    case LIBMSI_PROPERTY_LASTPRINTED:
    case LIBMSI_PROPERTY_CREATED_TM:
    case LIBMSI_PROPERTY_LASTSAVED_TM:
         return OLEVT_FILETIME;

    case LIBMSI_PROPERTY_SOURCE:
    case LIBMSI_PROPERTY_RESTRICT:
    case LIBMSI_PROPERTY_SECURITY:
    case LIBMSI_PROPERTY_VERSION:
         return OLEVT_I4;

    default:
        g_warn_if_reached ();
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

/**
 * libmsi_summary_info_get_properties:
 * @si: a #LibmsiSummaryInfo
 *
 * Returns: (array) (element-type LibmsiProperty) (transfer full): a new #GArray with the list of set properties
 **/
GArray *
libmsi_summary_info_get_properties (LibmsiSummaryInfo *self)
{
    GArray *props;
    int i;

    g_return_val_if_fail (LIBMSI_IS_SUMMARY_INFO (self), NULL);

    props = g_array_new (FALSE, FALSE, sizeof(LibmsiProperty));
    for (i = 0; i < MSI_MAX_PROPS; i++)
        if (self->property[i].vt != OLEVT_EMPTY)
            g_array_append_val (props, i);

    return props;
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

static gchar* filetime_to_string (guint64 ft)
{
    g_autoptr(GDateTime) dt = NULL;

    ft /= 10000000ULL;
    ft -= 134774ULL * 86400ULL;

    dt = g_date_time_new_from_unix_local(ft);

    return g_date_time_format(dt, "%Y/%m/%d %H:%M:%S");
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

    tm.tm_year = strtol( p, &end, 10 ) - 1900;
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
    char *str = NULL;
    gboolean valid;

    idofs = 8;

    /* now set all the properties */
    for( i = 0; i < cProperties; i++ )
    {
        valid = TRUE;
        int propid = read_dword(data, &idofs);
        unsigned dwOffset = read_dword(data, &idofs);
        int proptype;

        if( propid >= MSI_MAX_PROPS )
        {
            g_critical("Unknown property ID %d\n", propid );
            break;
        }

        type = get_type( propid );
        if( type == OLEVT_EMPTY )
        {
            g_critical("propid %d has unknown type\n", propid);
            break;
        }

        property = &prop[ propid ];

        if( dwOffset + 4 > sz )
        {
            g_critical("not enough data for type %d %d \n", dwOffset, sz);
            break;
        }

        proptype = read_dword(data, &dwOffset);
        if( dwOffset + 4 > sz )
        {
            g_critical("not enough data for type %d %d \n", dwOffset, sz);
            break;
        }

        switch(proptype)
        {
        case OLEVT_I2:
        case OLEVT_I4:
            property->intval = read_dword(data, &dwOffset);
            break;
        case OLEVT_FILETIME:
            if( dwOffset + 8 > sz )
            {
                g_critical("not enough data for type %d %d \n", dwOffset, sz);
                valid = FALSE;
                break;
            }
            property->filetime = read_dword(data, &dwOffset);
            property->filetime |= (guint64)read_dword(data, &dwOffset) << 32;
            break;
        case OLEVT_LPSTR:
            len = read_dword(data, &dwOffset);
            if(len == 0 || dwOffset + len > sz )
            {
                g_critical("not enough data for type %d %d %d \n", dwOffset, len, sz);
                valid = FALSE;
                break;
            }
            str = msi_alloc( len );
            memcpy( str, &data[dwOffset], len-1 );
            str[ len - 1 ] = 0;
            break;
        default:
            g_warn_if_reached ();
        }

        if (valid == FALSE) {
            break;
        }

        /* check the type is the same as we expect */
        if( type == OLEVT_LPSTR && proptype == OLEVT_LPSTR)
           property->strval = str;
        else if (type == proptype)
           ;
        else if( proptype == OLEVT_LPSTR) {
            g_return_if_fail(str != NULL);
            if( type == OLEVT_I2 || type == OLEVT_I4) {
                property->intval = atoi( str );
            } else if( type == OLEVT_FILETIME) {
                parse_filetime( str, &property->filetime );
            } else {
                g_critical("invalid type, it can't be converted\n");
                msi_free(str);
                break;
            }
            proptype = type;
            msi_free (str);
        }
        else
        {
            g_critical("invalid type \n");
            break;
        }

        /* Now we now the type is valid, store it */
        property->vt = proptype;
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
        g_critical("property set not little-endian\n");
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
        g_critical("too many properties %d\n", cProperties);
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
    default:
        g_warn_if_reached ();
    }
    return sz;
}

static unsigned suminfo_persist (LibmsiSummaryInfo *si, LibmsiDatabase *database)
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

    r = write_raw_stream_data (database, szSumInfo, data, sz, &stm);
    if (r == 0) {
        g_object_unref(G_OBJECT(stm));
    }
    msi_free( data );
    return r;
}

/**
 * libmsi_summary_info_get_property_type:
 * @si: a #LibmsiSummaryInfo
 * @prop: a #LibmsiProperty to get
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Returns: the property type associated for @prop.
 **/
LibmsiPropertyType
libmsi_summary_info_get_property_type (LibmsiSummaryInfo *self,
                                       LibmsiProperty prop,
                                       GError **error)
{
    g_return_val_if_fail (LIBMSI_SUMMARY_INFO (self), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    if (prop >= MSI_MAX_PROPS) {
        g_set_error (error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_UNKNOWN_PROPERTY,
                     "Unknown property");
        return LIBMSI_PROPERTY_TYPE_EMPTY;
    }

    switch (self->property[prop].vt) {
    case OLEVT_I2:
    case OLEVT_I4:
        return LIBMSI_PROPERTY_TYPE_INT;
    case OLEVT_LPSTR:
        return LIBMSI_PROPERTY_TYPE_STRING;
    case OLEVT_FILETIME:
        return LIBMSI_PROPERTY_TYPE_FILETIME;
    case OLEVT_EMPTY:
        return LIBMSI_PROPERTY_TYPE_EMPTY;
    default:
        g_set_error (error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_FUNCTION_FAILED,
                     "Unknown type");
        return LIBMSI_PROPERTY_TYPE_EMPTY;
    }
}

G_GNUC_INTERNAL gchar *
summary_info_as_string (LibmsiSummaryInfo *si, unsigned uiProperty)
{
    LibmsiOLEVariant *prop = &si->property[uiProperty];

    switch (prop->vt) {
    case OLEVT_I2:
    case OLEVT_I4:
        return g_strdup_printf ("%d", prop->intval);
    case OLEVT_LPSTR:
        return g_strdup (prop->strval);
    case OLEVT_FILETIME:
        return filetime_to_string (prop->filetime);
    case OLEVT_EMPTY:
        return g_strdup ("");
    default:
        g_warn_if_reached ();
    }

    return NULL;
}

static void _summary_info_get_property (LibmsiSummaryInfo *si, unsigned uiProperty,
                                        unsigned *puiDataType, int *pintvalue,
                                        guint64 *pftValue, char *szValueBuf,
                                        unsigned *pcchValueBuf, const gchar **str,
                                        GError **error)
{
    LibmsiOLEVariant *prop;
    LibmsiPropertyType type;

    if (uiProperty >= MSI_MAX_PROPS) {
        g_set_error (error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_UNKNOWN_PROPERTY,
                     "Unknown property");
        return;
    }

    g_object_ref (si);
    prop = &si->property[uiProperty];

    switch (prop->vt) {
    case OLEVT_I2:
    case OLEVT_I4:
        type = LIBMSI_PROPERTY_TYPE_INT;
        if (pintvalue)
            *pintvalue = prop->intval;
        break;
    case OLEVT_LPSTR:
        type = LIBMSI_PROPERTY_TYPE_STRING;
        if (str)
            *str = prop->strval;
        if (pcchValueBuf) {
            unsigned len = 0;

            len = strlen (prop->strval);
            if (szValueBuf)
                strcpyn (szValueBuf, prop->strval, *pcchValueBuf);
            if (len >= *pcchValueBuf)
                g_set_error (error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_MORE_DATA,
                             "The given string is too small");
            *pcchValueBuf = len;
        }
        break;
    case OLEVT_FILETIME:
        type = LIBMSI_PROPERTY_TYPE_FILETIME;
        if (pftValue)
            *pftValue = prop->filetime;
        break;
    case OLEVT_EMPTY:
        // FIXME: should be replaced by a has_property() instead?
        g_set_error (error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_SUCCESS, "Empty property");
        type = LIBMSI_PROPERTY_TYPE_EMPTY;
        break;
    default:
        g_return_if_reached ();
    }

    if (puiDataType)
        *puiDataType = type;

    g_object_unref (si);
}

/**
 * libmsi_summary_info_get_int:
 * @si: a #LibmsiSummaryInfo
 * @prop: a #LibmsiProperty to get
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Returns: the property value or -1 on failure
 **/
gint
libmsi_summary_info_get_int (LibmsiSummaryInfo *self, LibmsiProperty prop,
                             GError **error)
{
    LibmsiPropertyType type;
    gint val;

    g_return_val_if_fail (LIBMSI_SUMMARY_INFO (self), -1);
    g_return_val_if_fail (!error || *error == NULL, -1);

    type = LIBMSI_PROPERTY_TYPE_INT;
    _summary_info_get_property (self, prop, &type, &val,
                                NULL, NULL, NULL, NULL, error);

    return val;
}

/**
 * libmsi_summary_info_get_filetime:
 * @si: a #LibmsiSummaryInfo
 * @prop: a #LibmsiProperty to get
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Returns: the property value or 0 on failure
 **/
guint64
libmsi_summary_info_get_filetime (LibmsiSummaryInfo *self, LibmsiProperty prop,
                                  GError **error)
{
    LibmsiPropertyType type;
    guint64 val;

    g_return_val_if_fail (LIBMSI_SUMMARY_INFO (self), 0);
    g_return_val_if_fail (!error || *error == NULL, 0);

    type = LIBMSI_PROPERTY_TYPE_FILETIME;
    _summary_info_get_property (self, prop, &type, NULL,
                                &val, NULL, NULL, NULL, error);

    return val;
}

/**
 * libmsi_summary_info_get_string:
 * @si: a #LibmsiSummaryInfo
 * @prop: a #LibmsiProperty to get
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Returns: the property value or %NULL on failure
 **/
const gchar *
libmsi_summary_info_get_string (LibmsiSummaryInfo *self, LibmsiProperty prop,
                                GError **error)
{
    LibmsiPropertyType type;
    const gchar *str;

    g_return_val_if_fail (LIBMSI_SUMMARY_INFO (self), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    type = LIBMSI_PROPERTY_TYPE_STRING;
    _summary_info_get_property (self, prop, &type, NULL,
                                NULL, NULL, NULL, &str, error);

    return str;
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
    default:
        g_warn_if_reached ();
    }

    ret = LIBMSI_RESULT_SUCCESS;
end:
    g_object_unref(si);
    return ret;
}

/**
 * libmsi_summary_info_set_string:
 * @si: a #LibmsiSummaryInfo
 * @prop: a #LibmsiProperty to set
 * @value: a string value
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Set string property @prop.
 *
 * Returns: %TRUE on success
 **/
gboolean
libmsi_summary_info_set_string (LibmsiSummaryInfo *self, LibmsiProperty prop,
                                const gchar *value, GError **error)
{
    LibmsiResult ret;

    g_return_val_if_fail (LIBMSI_IS_SUMMARY_INFO (self), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    if (get_type (prop) != OLEVT_LPSTR) {
        g_set_error (error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH, G_STRFUNC);
        return FALSE;
    }

    ret = _libmsi_summary_info_set_property (self, prop, OLEVT_LPSTR, 0, 0, value);
    if (ret != LIBMSI_RESULT_SUCCESS) {
        g_set_error (error, LIBMSI_RESULT_ERROR, ret, G_STRFUNC);
        return FALSE;
    }

    return TRUE;
}

/**
 * libmsi_summary_info_set_int:
 * @si: a #LibmsiSummaryInfo
 * @prop: a #LibmsiProperty to set
 * @value: a value
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Set integer property @prop.
 *
 * Returns: %TRUE on success
 **/
gboolean
libmsi_summary_info_set_int (LibmsiSummaryInfo *self, LibmsiProperty prop,
                             gint value, GError **error)
{
    LibmsiResult ret;
    int type;

    g_return_val_if_fail (LIBMSI_IS_SUMMARY_INFO (self), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    type = get_type (prop);
    if (type != OLEVT_I2 && type != OLEVT_I4) {
        g_set_error (error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH, G_STRFUNC);
        return FALSE;
    }

    ret = _libmsi_summary_info_set_property (self, prop, type, value, 0, NULL);
    if (ret != LIBMSI_RESULT_SUCCESS) {
        g_set_error (error, LIBMSI_RESULT_ERROR, ret, G_STRFUNC);
        return FALSE;
    }

    return TRUE;
}

/**
 * libmsi_summary_info_set_filetime:
 * @si: a #LibmsiSummaryInfo
 * @prop: a #LibmsiProperty to set
 * @value: a value
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Set file time property @prop.
 *
 * Returns: %TRUE on success
 **/
gboolean
libmsi_summary_info_set_filetime (LibmsiSummaryInfo *self, LibmsiProperty prop,
                                  guint64 value, GError **error)
{
    LibmsiResult ret;
    int type;

    g_return_val_if_fail (LIBMSI_IS_SUMMARY_INFO (self), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    type = get_type (prop);
    if (type != OLEVT_FILETIME) {
        g_set_error (error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_DATATYPE_MISMATCH, G_STRFUNC);
        return FALSE;
    }

    ret = _libmsi_summary_info_set_property (self, prop, type, 0, &value, NULL);
    if (ret != LIBMSI_RESULT_SUCCESS) {
        g_set_error (error, LIBMSI_RESULT_ERROR, ret, G_STRFUNC);
        return FALSE;
    }

    return TRUE;
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

    case LIBMSI_PROPERTY_EDITTIME:
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
        g_warning("unhandled prop id %u\n", *pid);
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    return LIBMSI_RESULT_SUCCESS;
}

unsigned msi_add_suminfo( LibmsiDatabase *db, char ***records, int num_records, int num_columns )
{
    unsigned r = LIBMSI_RESULT_FUNCTION_FAILED;
    unsigned i, j;
    LibmsiSummaryInfo *si;

    si = libmsi_summary_info_new (db, num_records * (num_columns / 2), NULL);
    if (!si)
    {
        g_critical("no summary information!\n");
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
        r = suminfo_persist( si, db );

    g_object_unref(si);
    return r;
}

/**
 * libmsi_summary_info_persist:
 * @si: a #LibmsiSummaryInfo
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Save summary informations to the associated database.
 *
 * Returns: %TRUE on success
 **/
gboolean
libmsi_summary_info_persist (LibmsiSummaryInfo *si, GError **error)
{
    unsigned ret;

    g_return_val_if_fail (LIBMSI_IS_SUMMARY_INFO (si), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    TRACE("%p\n", si);

    if (!si->database) {
        g_set_error (error, LIBMSI_RESULT_ERROR,
                     LIBMSI_RESULT_FUNCTION_FAILED, "No database associated");
        return FALSE;
    }

    g_object_ref (si);
    ret = suminfo_persist (si, si->database);
    g_object_unref (si);

    if (ret != LIBMSI_RESULT_SUCCESS)
        g_set_error_literal (error, LIBMSI_RESULT_ERROR, ret, G_STRFUNC);

    return ret == LIBMSI_RESULT_SUCCESS;
}

/**
 * libmsi_summary_info_save:
 * @si: a #LibmsiSummaryInfo
 * @db: a #LibmsiDatabase to save to
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Save summary informations to the associated database.
 *
 * Returns: %TRUE on success
 **/
gboolean
libmsi_summary_info_save (LibmsiSummaryInfo *si, LibmsiDatabase *db, GError **error)
{
    unsigned ret;

    g_return_val_if_fail (LIBMSI_IS_SUMMARY_INFO (si), FALSE);
    g_return_val_if_fail (LIBMSI_IS_DATABASE (db), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    ret = suminfo_persist (si, db);
    if (ret != LIBMSI_RESULT_SUCCESS)
        g_set_error_literal (error, LIBMSI_RESULT_ERROR, ret, G_STRFUNC);

    return ret == LIBMSI_RESULT_SUCCESS;
}

/**
 * libmsi_summary_info_new:
 * @database: (allow-none): an optionnal associated #LibmsiDatabase
 * @update_count: number of changes allowed
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * If @database is provided, the summary informations will be
 * populated during creation, and the libmsi_summary_info_persist()
 * function will save the properties to it. If @database is %NULL, you
 * may still populate properties and then save them to a particular
 * database with the libmsi_summary_info_save() function.
 *
 * Returns: a #LibmsiSummaryInfo or %NULL on failure
 **/
LibmsiSummaryInfo *
libmsi_summary_info_new (LibmsiDatabase *database, unsigned update_count,
                         GError **error)
{
    LibmsiSummaryInfo *self;

    g_return_val_if_fail (!database || LIBMSI_IS_DATABASE (database), NULL);
    g_return_val_if_fail (!error || *error == NULL, NULL);

    self = g_object_new (LIBMSI_TYPE_SUMMARY_INFO,
                         "database", database,
                         "update-count", update_count,
                         NULL);

    return self;
}
