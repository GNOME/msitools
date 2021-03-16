/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2002,2003,2004,2005 Mike McCormack for CodeWeavers
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
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "libmsi-enums.h"
#include "libmsi-database.h"

#include "debug.h"
#include "libmsi.h"
#include "msipriv.h"
#include "query.h"

enum
{
    PROP_0,

    PROP_PATH,
    PROP_FLAGS,
    PROP_OUTPATH,
};

G_DEFINE_TYPE (LibmsiDatabase, libmsi_database, G_TYPE_OBJECT);

const guint8 clsid_msi_transform[16] = { 0x82, 0x10, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46 };
const guint8 clsid_msi_database[16] = { 0x84, 0x10, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46 };
const guint8 clsid_msi_patch[16] = { 0x86, 0x10, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0,0x00, 0x00,0x00,0x00,0x00,0x00,0x46 };

/*
 *  .MSI  file format
 *
 *  An .msi file is a structured storage file.
 *  It contains a number of streams.
 *  A stream for each table in the database.
 *  Two streams for the string table in the database.
 *  Any binary data in a table is a reference to a stream.
 */

#define IS_INTMSIDBOPEN(x)      \
      ((x) >= LIBMSI_DB_OPEN_READONLY && (x) <= LIBMSI_DB_OPEN_CREATE)

typedef struct _LibmsiTransform {
    struct list entry;
    GsfInfile *stg;
} LibmsiTransform;

typedef struct _LibmsiStorage {
    struct list entry;
    char *name;
    GsfInfile *stg;
} LibmsiStorage;

typedef struct _LibmsiStream {
    struct list entry;
    char *name;
    GsfInput *stm;
} LibmsiStream;

GQuark
libmsi_result_error_quark (void)
{
  return g_quark_from_static_string ("libmsi-result-error-quark");
}

GQuark
libmsi_db_error_quark (void)
{
  return g_quark_from_static_string ("libmsi-db-error-quark");
}

static void
libmsi_database_init (LibmsiDatabase *self)
{
    list_init (&self->tables);
    list_init (&self->transforms);
    list_init (&self->streams);
    list_init (&self->storages);
}

static void
libmsi_database_constructed (GObject *object)
{
    if (G_OBJECT_CLASS (libmsi_database_parent_class)->constructed)
        G_OBJECT_CLASS (libmsi_database_parent_class)->constructed (object);
}

static void
free_transforms (LibmsiDatabase *db)
{
    while (!list_empty(&db->transforms)) {
        LibmsiTransform *t = LIST_ENTRY(list_head(&db->transforms),
                                        LibmsiTransform, entry);
        list_remove(&t->entry);
        g_object_unref(G_OBJECT(t->stg));
        msi_free(t);
    }
}

static void
libmsi_database_finalize (GObject *object)
{
    LibmsiDatabase *self = LIBMSI_DATABASE (object);

    _libmsi_database_close (self, false);
    free_cached_tables (self);
    free_transforms (self);

    g_free (self->path);

    G_OBJECT_CLASS (libmsi_database_parent_class)->finalize (object);
}

static void
libmsi_database_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (LIBMSI_IS_DATABASE (object));
    LibmsiDatabase *self = LIBMSI_DATABASE (object);

    switch (prop_id) {
    case PROP_PATH:
        g_return_if_fail (self->path == NULL);
        self->path = g_value_dup_string (value);
        break;
    case PROP_FLAGS:
        self->flags = g_value_get_flags (value);
        break;
    case PROP_OUTPATH:
        g_return_if_fail (self->outpath == NULL);
        self->outpath = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
libmsi_database_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (LIBMSI_IS_DATABASE (object));
    LibmsiDatabase *self = LIBMSI_DATABASE (object);

    switch (prop_id) {
    case PROP_PATH:
        g_value_set_string (value, self->path);
        break;
    case PROP_FLAGS:
        g_value_set_flags (value, self->flags);
        break;
    case PROP_OUTPATH:
        g_value_set_string (value, self->outpath);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
libmsi_database_class_init (LibmsiDatabaseClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = libmsi_database_finalize;
    object_class->set_property = libmsi_database_set_property;
    object_class->get_property = libmsi_database_get_property;
    object_class->constructed = libmsi_database_constructed;

    g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_PATH,
        g_param_spec_string ("path", "path", "path", NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_FLAGS,
        g_param_spec_flags ("flags", "flags", "flags", LIBMSI_TYPE_DB_FLAGS, 0,
                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                            G_PARAM_STATIC_STRINGS));

    g_object_class_install_property (G_OBJECT_CLASS (klass), PROP_OUTPATH,
        g_param_spec_string ("outpath", "outpath", "outpath", NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                             G_PARAM_STATIC_STRINGS));
}

unsigned msi_open_storage( LibmsiDatabase *db, const char *stname )
{
    unsigned r = LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
    LibmsiStorage *storage;
    GsfInput *in;

    LIST_FOR_EACH_ENTRY( storage, &db->storages, LibmsiStorage, entry )
    {
        if( !strcmp( stname, storage->name ) )
        {
            TRACE("found %s\n", debugstr_a(stname));
            return r;
        }
    }

    if (!(storage = msi_alloc_zero( sizeof(LibmsiStorage) ))) return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
    storage->name = strdup( stname );
    if (!storage->name)
        goto done;

    in = gsf_infile_child_by_name(db->infile, stname);
    if (!GSF_IS_INFILE(in))
        goto done;

    storage->stg = GSF_INFILE(in);
    if (!storage->stg)
        goto done;

    list_add_tail( &db->storages, &storage->entry );
    r = LIBMSI_RESULT_SUCCESS;

done:
    if (r != LIBMSI_RESULT_SUCCESS) {
        msi_free(storage->name);
        msi_free(storage);
    }

    return r;
}

unsigned msi_create_storage( LibmsiDatabase *db, const char *stname, GsfInput *stm )
{
    LibmsiStorage *storage;
    GsfInfile *origstg = NULL;
    bool found = false;
    unsigned r;

    if (db->flags & LIBMSI_DB_FLAGS_READONLY)
        return LIBMSI_RESULT_ACCESS_DENIED;

    LIST_FOR_EACH_ENTRY( storage, &db->storages, LibmsiStorage, entry )
    {
        if( !strcmp( stname, storage->name ) )
        {
            TRACE("found %s\n", debugstr_a(stname));
            found = true;
            break;
        }
    }

    if (!found) {
        if (!(storage = msi_alloc_zero( sizeof(LibmsiStorage) ))) return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
        storage->name = strdup( stname );
        if (!storage->name)
        {
            msi_free(storage);
            return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
        }
    }

    origstg = gsf_infile_msole_new(stm, NULL);
    if (origstg == NULL)
        goto done;

    if (found) {
        if (storage->stg)
            g_object_unref(G_OBJECT(storage->stg));
    } else {
        list_add_tail( &db->storages, &storage->entry );
    }

    storage->stg = origstg;
    g_object_ref(G_OBJECT(storage->stg));

    r = LIBMSI_RESULT_SUCCESS;

done:
    if (r != LIBMSI_RESULT_SUCCESS) {
        if (!found) {
            msi_free(storage->name);
            msi_free(storage);
        }
    }

    if (origstg)
        g_object_unref(G_OBJECT(origstg));

    return r;
}

void msi_destroy_storage( LibmsiDatabase *db, const char *stname )
{
    LibmsiStorage *storage, *storage2;

    LIST_FOR_EACH_ENTRY_SAFE( storage, storage2, &db->storages, LibmsiStorage, entry )
    {
        if (!strcmp( stname, storage->name ))
        {
            TRACE("destroying %s\n", debugstr_a(stname));

            list_remove( &storage->entry );
            g_object_unref(G_OBJECT(storage->stg));
            msi_free( storage );
            break;
        }
    }
}

static unsigned find_infile_stream( LibmsiDatabase *db, const char *name, GsfInput **stm )
{
    LibmsiStream *stream;

    LIST_FOR_EACH_ENTRY( stream, &db->streams, LibmsiStream, entry )
    {
        if( !strcmp( name, stream->name ) )
        {
            TRACE("found %s\n", debugstr_a(name));
            *stm = stream->stm;
            return LIBMSI_RESULT_SUCCESS;
        }
    }

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

static unsigned msi_alloc_stream( LibmsiDatabase *db, const char *stname, GsfInput *stm)
{
    LibmsiStream *stream;

    TRACE("%p %s %p", db, debugstr_a(stname), stm);
    if (!(stream = msi_alloc( sizeof(LibmsiStream) ))) return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
    stream->name = strdup( stname );
    stream->stm = stm;
    g_object_ref(G_OBJECT(stm));
    list_add_tail( &db->streams, &stream->entry );
    return LIBMSI_RESULT_SUCCESS;
}

unsigned write_raw_stream_data( LibmsiDatabase *db, const char *stname,
                        const void *data, unsigned sz, GsfInput **outstm )
{
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    GsfInput *stm = NULL;
    guint8 *mem;
    LibmsiStream *stream;

    if (db->flags & LIBMSI_DB_FLAGS_READONLY)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    LIST_FOR_EACH_ENTRY( stream, &db->streams, LibmsiStream, entry )
    {
        if( !strcmp( stname, stream->name ) )
        {
            msi_destroy_stream( db, stname );
            break;
        }
    }

    mem = g_try_malloc(sz == 0 ? 1 : sz);
    if (!mem)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    if (data || sz)
        memcpy(mem, data, sz);

    stm = gsf_input_memory_new(mem, sz, true);
    ret = msi_alloc_stream( db, stname, stm);
    *outstm = stm;
    return ret;
}

unsigned msi_create_stream( LibmsiDatabase *db, const char *stname, GsfInput *stm )
{
    LibmsiStream *stream;
    char *encname = NULL;
    unsigned r = LIBMSI_RESULT_FUNCTION_FAILED;
    bool found = false;

    if (db->flags & LIBMSI_DB_FLAGS_READONLY)
        return LIBMSI_RESULT_ACCESS_DENIED;

    encname = encode_streamname(false, stname);

    LIST_FOR_EACH_ENTRY( stream, &db->streams, LibmsiStream, entry )
    {
        if( !strcmp( encname, stream->name ) )
        {
            found = true;
            break;
        }
    }

    if (found) {
        if (stream->stm)
            g_object_unref(G_OBJECT(stream->stm));
        stream->stm = stm;
        g_object_ref(G_OBJECT(stream->stm));
        r = LIBMSI_RESULT_SUCCESS;
    } else
        r = msi_alloc_stream( db, encname, stm );

    msi_free(encname);
    return r;
}

unsigned msi_enum_db_streams(LibmsiDatabase *db,
                             unsigned (*fn)(const char *, GsfInput *, void *),
                             void *opaque)
{
    unsigned r;
    LibmsiStream *stream, *stream2;

    LIST_FOR_EACH_ENTRY_SAFE( stream, stream2, &db->streams, LibmsiStream, entry )
    {
        GsfInput *stm;

        stm = stream->stm;
        g_object_ref(G_OBJECT(stm));
        r = fn( stream->name, stm, opaque);
        g_object_unref(G_OBJECT(stm));

        if (r) {
            return r;
        }
    }

    return LIBMSI_RESULT_SUCCESS;
}

unsigned msi_enum_db_storages(LibmsiDatabase *db,
                              unsigned (*fn)(const char *, GsfInfile *, void *),
                              void *opaque)
{
    unsigned r;
    LibmsiStorage *storage, *storage2;

    LIST_FOR_EACH_ENTRY_SAFE( storage, storage2, &db->storages, LibmsiStorage, entry )
    {
        GsfInfile *stg;

        stg = storage->stg;
        g_object_ref(G_OBJECT(stg));
        r = fn( storage->name, stg, opaque);
        g_object_unref(G_OBJECT(stg));

        if (r) {
            return r;
        }
    }

    return LIBMSI_RESULT_SUCCESS;
}

static
unsigned clone_infile_stream( LibmsiDatabase *db, const char *name, GsfInput **stm )
{
    GsfInput *stream;

    if (find_infile_stream( db, name, &stream ) == LIBMSI_RESULT_SUCCESS)
    {
        stream = gsf_input_dup( stream, NULL );
        if( !stream )
        {
            g_warning("failed to clone stream\n");
            return LIBMSI_RESULT_FUNCTION_FAILED;
        }

        gsf_input_seek( stream, 0, G_SEEK_SET );
        *stm = stream;
        return LIBMSI_RESULT_SUCCESS;
    }

    return LIBMSI_RESULT_FUNCTION_FAILED;
}

unsigned msi_get_raw_stream( LibmsiDatabase *db, const char *stname, GsfInput **stm )
{
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    g_autofree char *decoded = NULL;
    LibmsiTransform *transform;

    decoded = decode_streamname(stname);
    TRACE("%s -> %s\n", debugstr_a(stname), debugstr_a(decoded));

    if (clone_infile_stream( db, stname, stm ) == LIBMSI_RESULT_SUCCESS)
        return LIBMSI_RESULT_SUCCESS;

    LIST_FOR_EACH_ENTRY( transform, &db->transforms, LibmsiTransform, entry )
    {
        *stm = gsf_infile_child_by_name( transform->stg, stname );
        if (*stm)
        {
            ret = LIBMSI_RESULT_SUCCESS;
            break;
        }
    }

    return ret;
}

void msi_destroy_stream( LibmsiDatabase *db, const char *stname )
{
    LibmsiStream *stream, *stream2;

    LIST_FOR_EACH_ENTRY_SAFE( stream, stream2, &db->streams, LibmsiStream, entry )
    {
        if (!strcmp( stname, stream->name ))
        {
            TRACE("destroying %s\n", debugstr_a(stname));

            list_remove( &stream->entry );
            g_object_unref(G_OBJECT(stream->stm));
            msi_free( stream );
            break;
        }
    }
}

static void free_storages( LibmsiDatabase *db )
{
    while( !list_empty( &db->storages ) )
    {
        LibmsiStorage *s = LIST_ENTRY(list_head( &db->storages ), LibmsiStorage, entry);
        list_remove( &s->entry );
        g_object_unref(G_OBJECT(s->stg));
        msi_free( s->name );
        msi_free( s );
    }
}

static void free_streams( LibmsiDatabase *db )
{
    while( !list_empty( &db->streams ) )
    {
        LibmsiStream *s = LIST_ENTRY(list_head( &db->streams ), LibmsiStream, entry);
        list_remove( &s->entry );
        g_object_unref(G_OBJECT(s->stm));
        msi_free( s->name );
        msi_free( s );
    }
}

void append_storage_to_db( LibmsiDatabase *db, GsfInfile *stg )
{
    LibmsiTransform *t;

    t = msi_alloc( sizeof *t );
    t->stg = stg;
    g_object_ref(G_OBJECT(t->stg));
    list_add_head( &db->transforms, &t->entry );

#if 0
    /* the transform may add or replace streams...
     *
     * FIXME: Hmm, the MSI is always searched before the transform though.
     * For now disable this. */
    free_streams( db );
#endif
}

LibmsiResult _libmsi_database_close(LibmsiDatabase *db, bool committed)
{
    TRACE("%p %d\n", db, committed);

    if ( db->strings )
    {
        msi_destroy_stringtable( db->strings);
        db->strings = NULL;
    }

    if ( db->infile )
    {
        g_object_unref(G_OBJECT(db->infile));
        db->infile = NULL;
    }

    if ( db->outfile )
    {
        gsf_output_close(GSF_OUTPUT(db->outfile));
        g_object_unref(G_OBJECT(db->outfile));
        db->outfile = NULL;
    }
    free_streams( db );
    free_storages( db );

    if (db->outpath) {
        if (!committed) {
            unlink( db->outpath );
            msi_free( db->outpath );
        } else if (db->rename_outpath) {
            unlink(db->path);
            rename(db->outpath, db->path);
            msi_free( db->outpath );
        } else {
            msi_free( db->path );
            db->path = db->outpath;
        }
    }
    db->outpath = NULL;
    return LIBMSI_RESULT_SUCCESS;
}

LibmsiResult _libmsi_database_start_transaction(LibmsiDatabase *db)
{
    unsigned ret = LIBMSI_RESULT_SUCCESS;
    GsfOutput *out;
    GsfOutfile *stg = NULL;
    char *tmpfile = NULL;
    char path[PATH_MAX];

    if (db->flags & LIBMSI_DB_FLAGS_READONLY)
        return LIBMSI_RESULT_SUCCESS;

    db->rename_outpath = false;
    if( !db->outpath )
    {
        strcpy( path, db->path );
        if (db->flags & LIBMSI_DB_FLAGS_TRANSACT)
	{
            strcat( path, ".tmp" );
            db->rename_outpath = true;
	}
        db->outpath = strdup(path);
    }

    TRACE("%p %s\n", db, db->outpath);

    out = gsf_output_stdio_new(db->outpath, NULL);
    if (!out)
    {
        g_warning("open file failed for %s\n", debugstr_a(db->outpath));
        return LIBMSI_RESULT_OPEN_FAILED;
    }
    stg = gsf_outfile_msole_new(out);
    g_object_unref(G_OBJECT(out));
    if (!stg)
    {
        g_warning("open failed for %s\n", debugstr_a(db->outpath));
        return LIBMSI_RESULT_OPEN_FAILED;
    }

    if (!gsf_outfile_msole_set_class_id(GSF_OUTFILE_MSOLE(stg),
                                        db->flags & LIBMSI_DB_FLAGS_PATCH ?
                                        clsid_msi_patch : clsid_msi_database ))
    {
        g_warning("set guid failed\n");
        ret = LIBMSI_RESULT_FUNCTION_FAILED;
        goto end;
    }

    db->outfile = stg;
    g_object_ref(G_OBJECT(db->outfile));

end:
    if (ret) {
        if (db->outfile)
            g_object_unref(G_OBJECT(db->outfile));
        db->outfile = NULL;
    }
    if (stg)
        g_object_unref(G_OBJECT(stg));
    msi_free(tmpfile);
    return ret;
}

static char *msi_read_text_archive(const char *path, unsigned *len)
{
    char *data;
    size_t nread;

    if (!g_file_get_contents(path, &data, &nread, NULL))
        return NULL;

    while (!data[nread - 1]) nread--;
    *len = nread;
    return data;
}

static void msi_parse_line(char **line, char ***entries, unsigned *num_entries, unsigned *len)
{
    char *ptr = *line;
    char *save;
    unsigned i, count = 1, chars_left = *len;

    *entries = NULL;

    /* stay on this line */
    while (chars_left && *ptr != '\n')
    {
        /* entries are separated by tabs */
        if (*ptr == '\t')
            count++;

        ptr++;
        chars_left--;
    }

    *entries = msi_alloc(count * sizeof(char *));
    if (!*entries)
        return;

    /* store pointers into the data */
    chars_left = *len;
    for (i = 0, ptr = *line; i < count; i++)
    {
        while (chars_left && *ptr == '\r')
        {
            ptr++;
            chars_left--;
        }
        save = ptr;

        while (chars_left && *ptr != '\t' && *ptr != '\n' && *ptr != '\r')
        {
            if (!*ptr) *ptr = '\n'; /* convert embedded nulls to \n */
            if (ptr > *line && *ptr == '\x19' && *(ptr - 1) == '\x11')
            {
                *ptr = '\n';
                *(ptr - 1) = '\r';
            }
            ptr++;
            chars_left--;
        }

        /* NULL-separate the data */
        if (*ptr == '\n' || *ptr == '\r')
        {
            while (chars_left && (*ptr == '\n' || *ptr == '\r'))
            {
                *(ptr++) = 0;
                chars_left--;
            }
        }
        else if (*ptr)
        {
            *(ptr++) = 0;
            chars_left--;
        }
        (*entries)[i] = save;
    }

    /* move to the next line if there's more, else EOF */
    *line = ptr;
    *len = chars_left;
    if (num_entries)
        *num_entries = count;
}

static char *msi_build_createsql_prelude(char *table)
{
    char *prelude;
    unsigned size;

    static const char create_fmt[] = "CREATE TABLE `%s` (";

    size = sizeof(create_fmt)/sizeof(create_fmt[0]) + strlen(table) - 2;
    prelude = msi_alloc(size * sizeof(char));
    if (!prelude)
        return NULL;

    sprintf(prelude, create_fmt, table);
    return prelude;
}

static char *msi_build_createsql_columns(char **columns_data, char **types, unsigned num_columns)
{
    char *columns;
    char *p;
    const char *type;
    unsigned sql_size = 1, i, len;
    char expanded[128], *ptr;
    char size[10], comma[2], extra[30];

    static const char column_fmt[] = "`%s` %s%s%s%s ";
    static const char size_fmt[] = "(%s)";
    static const char type_char[] = "CHAR";
    static const char type_int[] = "INT";
    static const char type_long[] = "LONG";
    static const char type_object[] = "OBJECT";
    static const char type_notnull[] = " NOT NULL";
    static const char localizable[] = " LOCALIZABLE";

    columns = msi_alloc_zero(sql_size * sizeof(char));
    if (!columns)
        return NULL;

    for (i = 0; i < num_columns; i++)
    {
        type = NULL;
        comma[1] = size[0] = extra[0] = '\0';

        if (i == num_columns - 1)
            comma[0] = '\0';
        else
            comma[0] = ',';

        ptr = &types[i][1];
        len = atol(ptr);
        extra[0] = '\0';

        switch (types[i][0])
        {
            case 'l':
                strcpy(extra, type_notnull);
                /* fall through */
            case 'L':
                strcat(extra, localizable);
                type = type_char;
                sprintf(size, size_fmt, ptr);
                break;
            case 's':
                strcpy(extra, type_notnull);
                /* fall through */
            case 'S':
                type = type_char;
                sprintf(size, size_fmt, ptr);
                break;
            case 'i':
                strcpy(extra, type_notnull);
                /* fall through */
            case 'I':
                if (len <= 2)
                    type = type_int;
                else if (len == 4)
                    type = type_long;
                else
                {
                    g_warning("invalid int width %u\n", len);
                    msi_free(columns);
                    return NULL;
                }
                break;
            case 'v':
                strcpy(extra, type_notnull);
                /* fall through */
            case 'V':
                type = type_object;
                break;
            default:
                g_critical("Unknown type: %c\n", types[i][0]);
                msi_free(columns);
                return NULL;
        }

        sprintf(expanded, column_fmt, columns_data[i], type, size, extra, comma);
        sql_size += strlen(expanded);

        p = msi_realloc(columns, sql_size * sizeof(char));
        if (!p)
        {
            msi_free(columns);
            return NULL;
        }
        columns = p;

        strcat(columns, expanded);
    }

    return columns;
}

static char *msi_build_createsql_postlude(char **primary_keys, unsigned num_keys)
{
    char *postlude;
    char *keys;
    char *ptr;
    unsigned size, key_size, i;

    static const char key_fmt[] = "`%s`, ";
    static const char postlude_fmt[] = "PRIMARY KEY %s)";

    for (i = 0, size = 1; i < num_keys; i++)
        size += strlen(key_fmt) + strlen(primary_keys[i]) - 2;

    keys = msi_alloc(size * sizeof(char));
    if (!keys)
        return NULL;

    for (i = 0, ptr = keys; i < num_keys; i++)
    {
        key_size = strlen(key_fmt) + strlen(primary_keys[i]) -2;
        sprintf(ptr, key_fmt, primary_keys[i]);
        ptr += key_size;
    }

    /* remove final ', ' */
    *(ptr - 2) = '\0';

    size = strlen(postlude_fmt) + size - 1;
    postlude = msi_alloc(size * sizeof(char));
    if (!postlude)
        goto done;

    sprintf(postlude, postlude_fmt, keys);

done:
    msi_free(keys);
    return postlude;
}

static unsigned msi_add_table_to_db(LibmsiDatabase *db, char **columns, char **types, char **labels, unsigned num_labels, unsigned num_columns)
{
    unsigned r = LIBMSI_RESULT_OUTOFMEMORY;
    unsigned size;
    LibmsiQuery *view = NULL;
    char *create_sql = NULL;
    char *prelude;
    char *columns_sql;
    char *postlude;
    GError *error = NULL; // FIXME: move error handling to caller

    prelude = msi_build_createsql_prelude(labels[0]);
    columns_sql = msi_build_createsql_columns(columns, types, num_columns);
    postlude = msi_build_createsql_postlude(labels + 1, num_labels - 1); /* skip over table name */

    if (!prelude || !columns_sql || !postlude)
        goto done;

    size = strlen(prelude) + strlen(columns_sql) + strlen(postlude) + 1;
    create_sql = msi_alloc(size * sizeof(char));
    if (!create_sql)
        goto done;

    strcpy(create_sql, prelude);
    strcat(create_sql, columns_sql);
    strcat(create_sql, postlude);

    view = libmsi_query_new(db, create_sql, &error);
    if (!view)
        goto done;

    r = _libmsi_query_execute(view, NULL);
    libmsi_query_close(view, &error);

done:
    if (error)
        g_critical ("%s", error->message);

    g_clear_error (&error);
    if (view)
        g_object_unref(view);
    msi_free(prelude);
    msi_free(columns_sql);
    msi_free(postlude);
    msi_free(create_sql);
    return r;
}

static unsigned construct_record(unsigned num_columns, char **types,
                             char **data, const char *name, LibmsiRecord **rec)
{
    unsigned i;

    *rec = libmsi_record_new(num_columns);
    if (!*rec)
        return LIBMSI_RESULT_OUTOFMEMORY;

    for (i = 0; i < num_columns; i++)
    {
        switch (types[i][0])
        {
            case 'L': case 'l': case 'S': case 's':
                libmsi_record_set_string(*rec, i + 1, data[i]);
                break;
            case 'I': case 'i':
                if (*data[i])
                    libmsi_record_set_int(*rec, i + 1, atoi(data[i]));
                break;
            case 'V': case 'v':
                if (*data[i])
                {
                    unsigned r;
                    char *file = g_build_filename (name, data[i], NULL);
                    r = _libmsi_record_load_stream_from_file(*rec, i + 1, file);
                    g_free (file);
                    if (r != LIBMSI_RESULT_SUCCESS)
                        return LIBMSI_RESULT_FUNCTION_FAILED;
                }
                break;
            default:
                g_critical("Unhandled column type: %c\n", types[i][0]);
                g_object_unref(*rec);
                return LIBMSI_RESULT_FUNCTION_FAILED;
        }
    }

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned msi_add_records_to_table(LibmsiDatabase *db, char **columns, char **types,
                                     char **labels, char ***records,
                                     int num_columns, int num_records)
{
    unsigned r, num_rows, num_cols;
    int i;
    LibmsiView *view;
    LibmsiRecord *rec;

    r = table_view_create(db, labels[0], &view);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = view->ops->get_dimensions( view, &num_rows, &num_cols );
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    while (num_rows > 0)
    {
        r = view->ops->delete_row(view, --num_rows);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;
    }

    for (i = 0; i < num_records; i++)
    {
        r = construct_record(num_columns, types, records[i], labels[0], &rec);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;

        r = view->ops->insert_row(view, rec, -1, false);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            g_object_unref(rec);
            goto done;
        }

        g_object_unref(rec);
    }

done:
    msi_free(view);
    return r;
}

static unsigned _libmsi_database_import(LibmsiDatabase *db, const char *path)
{
    unsigned r = LIBMSI_RESULT_OUTOFMEMORY;
    unsigned len, i;
    unsigned num_labels = 0;
    unsigned num_types = 0;
    unsigned num_columns = 0;
    unsigned num_records = 0;
    char **columns = NULL;
    char **types = NULL;
    char **labels = NULL;
    char *ptr;
    char *data = NULL;
    char ***records = NULL;
    char ***temp_records;

    static const char suminfo[] = "_SummaryInformation";
    static const char forcecodepage[] = "_ForceCodepage";

    TRACE("%p %s\n", db, debugstr_a(path));

    data = msi_read_text_archive( path, &len );
    if (!data)
        goto done;

    ptr = data;
    msi_parse_line( &ptr, &columns, &num_columns, &len );
    msi_parse_line( &ptr, &types, &num_types, &len );
    msi_parse_line( &ptr, &labels, &num_labels, &len );

    if (num_columns == 1 && !columns[0][0] && num_labels == 1 && !labels[0][0] &&
        num_types == 2 && !strcmp( types[1], forcecodepage ))
    {
        r = msi_set_string_table_codepage( db->strings, atoi( types[0] ) );
        goto done;
    }

    if (num_columns != num_types)
    {
        r = LIBMSI_RESULT_FUNCTION_FAILED;
        goto done;
    }

    records = msi_alloc(sizeof(char **));
    if (!records)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        goto done;
    }

    /* read in the table records */
    while (len)
    {
        msi_parse_line( &ptr, &records[num_records], NULL, &len );

        num_records++;
        temp_records = msi_realloc(records, (num_records + 1) * sizeof(char **));
        if (!temp_records)
        {
            r = LIBMSI_RESULT_OUTOFMEMORY;
            goto done;
        }
        records = temp_records;
    }

    if (!strcmp(labels[0], suminfo))
    {
        r = msi_add_suminfo( db, records, num_records, num_columns );
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            r = LIBMSI_RESULT_FUNCTION_FAILED;
            goto done;
        }
    }
    else
    {
        if (!table_view_exists(db, labels[0]))
        {
            r = msi_add_table_to_db( db, columns, types, labels, num_labels, num_columns );
            if (r != LIBMSI_RESULT_SUCCESS)
            {
                r = LIBMSI_RESULT_FUNCTION_FAILED;
                goto done;
            }
        }

        r = msi_add_records_to_table( db, columns, types, labels, records, num_columns, num_records);
    }

done:
    msi_free(data);
    msi_free(columns);
    msi_free(types);
    msi_free(labels);

    for (i = 0; i < num_records; i++)
        msi_free(records[i]);

    msi_free(records);

    return r;
}

/**
 * libmsi_database_import:
 * @db: a %LibmsiDatabase
 * @path: path to a table file
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Import a table to the database from file @path.
 *
 * Returns: %TRUE on success
 **/
gboolean
libmsi_database_import (LibmsiDatabase *db,
                        const char *path,
                        GError **error)
{
    unsigned r;

    TRACE("%p %s\n",db,debugstr_a(path));

    g_return_val_if_fail (LIBMSI_IS_DATABASE (db), FALSE);
    g_return_val_if_fail (path, FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    g_object_ref(db);
    r = _libmsi_database_import(db, path);
    g_object_unref(db);

    if (r != LIBMSI_RESULT_SUCCESS)
        g_set_error (error, LIBMSI_RESULT_ERROR, r, G_STRFUNC);

    return r == LIBMSI_RESULT_SUCCESS;
}

static gboolean
msi_export_stream (GsfInput *gsfin, GFile *table_dir, gchar **str,
                   GError **error)
{
    GError *err = NULL;
    GFile *file = NULL;
    GInputStream *in = NULL;
    GOutputStream *out = NULL;
    gssize spliced = -1;

    if (!table_dir)
        goto end;

    if (!g_file_make_directory_with_parents (table_dir, NULL, &err) &&
        !g_error_matches (err, G_IO_ERROR, G_IO_ERROR_EXISTS)) {
        g_propagate_error (error, err);
        goto end;
    }

    *str = g_strdup (g_object_get_data (G_OBJECT (gsfin), "stname"));
    file = g_file_get_child (table_dir, *str);
    out = G_OUTPUT_STREAM (g_file_replace (file, NULL, FALSE, 0, NULL, error));
    in = G_INPUT_STREAM (libmsi_istream_new (gsfin));
    spliced = g_output_stream_splice (out, in, 0, NULL, NULL);

end:
    g_clear_error (&err);
    if (file)
        g_object_unref (file);
    if (out)
        g_object_unref (out);
    if (in)
        g_object_unref (in);

    return spliced != -1;

}
static unsigned msi_export_record(int fd, LibmsiRecord *row,
                                  unsigned start, GFile *table_dir,
                                  GError **error)
{
    GsfInput *in = NULL;
    unsigned i, count;
    unsigned success = LIBMSI_RESULT_FUNCTION_FAILED;

    count = libmsi_record_get_field_count (row);
    for (i = start; i <= count; i++) {
        char *str;

        _libmsi_record_get_gsf_input (row, i, &in);
        if (in) {
            if (!msi_export_stream (in, table_dir, &str, error))
                goto end;
            g_object_unref (in);
            in = NULL;
        } else {
            str = libmsi_record_get_string (row, i);
            if (!str)
                goto end;
        }

        /* TODO full_write */
        if (write (fd, str, strlen (str)) != strlen (str)) {
            g_free (str);
            goto end;
        }

        g_free (str);

        const char *sep = (i < count) ? "\t" : "\r\n";
        if (write (fd, sep, strlen (sep)) != strlen (sep))
            goto end;
    }

    success = LIBMSI_RESULT_SUCCESS;

end:
    if (in)
        g_object_unref (in);

    return success;
}

typedef struct _ExportRow {
    int fd;
    GFile *table_dir;
    GError **error;
} ExportRow;

static unsigned msi_export_row( LibmsiRecord *row, void *arg )
{
    ExportRow *export = arg;

    return msi_export_record (export->fd, row, 1,
                              export->table_dir, export->error);
}

static LibmsiResult msi_export_forcecodepage( int fd, unsigned codepage )
{
    static const char fmt[] = "\r\n\r\n%u\t_ForceCodepage\r\n";
    char data[sizeof(fmt) + 10];
    unsigned sz;

    sprintf( data, fmt, codepage );

    sz = strlen(data) + 1;
    if (write( fd, data, sz ) != sz)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    return LIBMSI_RESULT_SUCCESS;
}

static LibmsiResult msi_export_summaryinfo (LibmsiDatabase *db, int fd, GError **error)
{
    static const char header[] =
        "PropertyId\tValue\r\ni2\tl255\r\n_SummaryInformation\tPropertyId\r\n";
    LibmsiResult result = LIBMSI_RESULT_FUNCTION_FAILED;
    LibmsiSummaryInfo *si = libmsi_summary_info_new (db, 0, error);
    gchar *str = NULL;
    gssize sz;
    int i;

    if (!si)
        goto end;

    sz = strlen (header);
    if (write (fd, header, sz) != sz)
        goto end;

    for (i = 0; i < MSI_MAX_PROPS; i++)
        if (si->property[i].vt != OLEVT_EMPTY) {
            gchar *val = summary_info_as_string (si, i);
            if (!val)
                goto end;
            str = g_strdup_printf ("%d\t%s\r\n", i, val);
            sz = strlen (str);
            if (write (fd, str, sz) != sz)
                goto end;
            g_free (str);
            str = NULL;
        }

    result = LIBMSI_RESULT_SUCCESS;

end:
    g_free (str);
    if (si)
        g_object_unref (si);
    return result;
}

static LibmsiResult _libmsi_database_export(LibmsiDatabase *db, const char *table,
                                        int fd, GError **error)
{
    static const char query[] = "select * from %s";
    LibmsiRecord *rec = NULL;
    LibmsiQuery *view = NULL;
    LibmsiResult r;

    TRACE("%p %s %d\n", db, debugstr_a(table), fd );

    if (!strcmp(table, "_ForceCodepage")) {
        unsigned codepage = msi_get_string_table_codepage (db->strings);
        return msi_export_forcecodepage (fd, codepage);
    } else if (!strcmp (table, "_SummaryInformation")) {
        return msi_export_summaryinfo (db, fd, error);
    }

    r = _libmsi_query_open( db, &view, query, table );
    if (r == LIBMSI_RESULT_SUCCESS)
    {
        /* write out row 1, the column names */
        r = _libmsi_query_get_column_info(view, LIBMSI_COL_INFO_NAMES, &rec);
        if (r == LIBMSI_RESULT_SUCCESS)
        {
            msi_export_record( fd, rec, 1, NULL, error);
            g_object_unref(rec);
        }

        /* write out row 2, the column types */
        r = _libmsi_query_get_column_info(view, LIBMSI_COL_INFO_TYPES, &rec);
        if (r == LIBMSI_RESULT_SUCCESS)
        {
            msi_export_record( fd, rec, 1, NULL, error);
            g_object_unref(rec);
        }

        /* write out row 3, the table name + keys */
        r = _libmsi_database_get_primary_keys( db, table, &rec );
        if (r == LIBMSI_RESULT_SUCCESS)
        {
            libmsi_record_set_string( rec, 0, table );
            msi_export_record( fd, rec, 0, NULL, error);
            g_object_unref(rec);
        }

        /* write out row 4 onwards, the data */
        ExportRow export = {
            .fd = fd,
            .table_dir = g_file_new_for_path (table),
            .error = error
        };
        r = _libmsi_query_iterate_records( view, 0, msi_export_row, &export);

        g_object_unref (export.table_dir);
        g_object_unref (view);
    }

    return r;
}

/**
 * libmsi_database_export:
 * @db: a %LibmsiDatabase
 * @table: a table name
 * @fd: a file descriptor
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Writes a file containing the table data as tab separated ASCII.
 * The format is as follows:
 *
 * row1 : colname1 <tab> colname2 <tab> .... colnameN <cr> <lf>
 * row2 : coltype1 <tab> coltype2 <tab> .... coltypeN <cr> <lf>
 * row3 : tablename <tab> key1 <tab> key2 <tab> ... keyM <cr> <lf>
 *
 * Followed by the data, starting at row 1 with one row per line
 *
 * row4 : data <tab> data <tab> data <tab> ... data <cr> <lf>
 *
 * Returns: %TRUE on success
 **/
gboolean
libmsi_database_export (LibmsiDatabase *db,
                        const char *table,
                        int fd,
                        GError **error)
{
    unsigned r = LIBMSI_RESULT_OUTOFMEMORY;

    TRACE("%p %s %d\n", db, table, fd);

    g_return_val_if_fail (LIBMSI_IS_DATABASE (db), FALSE);
    g_return_val_if_fail (table, FALSE);
    g_return_val_if_fail (fd >= 0, FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    g_object_ref(db);
    r = _libmsi_database_export (db, table, fd, error);
    g_object_unref(db);

    if (r != LIBMSI_RESULT_SUCCESS && error && !*error)
        g_set_error (error, LIBMSI_RESULT_ERROR, r, G_STRFUNC);

    return r == LIBMSI_RESULT_SUCCESS;
}

typedef struct _tagMERGETABLE
{
    struct list entry;
    struct list rows;
    char *name;
    unsigned numconflicts;
    char **columns;
    unsigned numcolumns;
    char **types;
    unsigned numtypes;
    char **labels;
    unsigned numlabels;
} MERGETABLE;

typedef struct _tagMERGEROW
{
    struct list entry;
    LibmsiRecord *data;
} MERGEROW;

typedef struct _tagMERGEDATA
{
    LibmsiDatabase *db;
    LibmsiDatabase *merge;
    MERGETABLE *curtable;
    LibmsiQuery *curview;
    struct list *tabledata;
} MERGEDATA;

static bool merge_type_match(const char *type1, const char *type2)
{
    if (((type1[0] == 'l') || (type1[0] == 's')) &&
        ((type2[0] == 'l') || (type2[0] == 's')))
        return true;

    if (((type1[0] == 'L') || (type1[0] == 'S')) &&
        ((type2[0] == 'L') || (type2[0] == 'S')))
        return true;

    return !strcmp( type1, type2 );
}

static unsigned merge_verify_colnames(LibmsiQuery *dbview, LibmsiQuery *mergeview)
{
    LibmsiRecord *dbrec, *mergerec;
    unsigned r, i, count;

    r = _libmsi_query_get_column_info(dbview, LIBMSI_COL_INFO_NAMES, &dbrec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = _libmsi_query_get_column_info(mergeview, LIBMSI_COL_INFO_NAMES, &mergerec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    count = libmsi_record_get_field_count(dbrec);
    for (i = 1; i <= count; i++)
    {
        if (!_libmsi_record_get_string_raw(mergerec, i))
            break;

        if (strcmp( _libmsi_record_get_string_raw( dbrec, i ), _libmsi_record_get_string_raw( mergerec, i ) ))
        {
            r = LIBMSI_RESULT_DATATYPE_MISMATCH;
            goto done;
        }
    }

    g_object_unref(dbrec);
    g_object_unref(mergerec);
    dbrec = mergerec = NULL;

    r = _libmsi_query_get_column_info(dbview, LIBMSI_COL_INFO_TYPES, &dbrec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = _libmsi_query_get_column_info(mergeview, LIBMSI_COL_INFO_TYPES, &mergerec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    count = libmsi_record_get_field_count(dbrec);
    for (i = 1; i <= count; i++)
    {
        if (!_libmsi_record_get_string_raw(mergerec, i))
            break;

        if (!merge_type_match(_libmsi_record_get_string_raw(dbrec, i),
                     _libmsi_record_get_string_raw(mergerec, i)))
        {
            r = LIBMSI_RESULT_DATATYPE_MISMATCH;
            break;
        }
    }

done:
    g_object_unref(dbrec);
    g_object_unref(mergerec);

    return r;
}

static unsigned merge_verify_primary_keys(LibmsiDatabase *db, LibmsiDatabase *mergedb,
                                      const char *table)
{
    LibmsiRecord *dbrec, *mergerec = NULL;
    unsigned r, i, count;

    r = _libmsi_database_get_primary_keys(db, table, &dbrec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = _libmsi_database_get_primary_keys(mergedb, table, &mergerec);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    count = libmsi_record_get_field_count(dbrec);
    if (count != libmsi_record_get_field_count(mergerec))
    {
        r = LIBMSI_RESULT_DATATYPE_MISMATCH;
        goto done;
    }

    for (i = 1; i <= count; i++)
    {
        if (strcmp( _libmsi_record_get_string_raw( dbrec, i ), _libmsi_record_get_string_raw( mergerec, i ) ))
        {
            r = LIBMSI_RESULT_DATATYPE_MISMATCH;
            goto done;
        }
    }

done:
    g_object_unref(dbrec);
    g_object_unref(mergerec);

    return r;
}

static char *get_key_value(LibmsiQuery *view, const char *key, LibmsiRecord *rec)
{
    LibmsiRecord *colnames;
    char *str;
    unsigned r, i = 0;
    int cmp;

    r = _libmsi_query_get_column_info(view, LIBMSI_COL_INFO_NAMES, &colnames);
    if (r != LIBMSI_RESULT_SUCCESS)
        return NULL;

    do
    {
        str = msi_dup_record_field(colnames, ++i);
        cmp = strcmp( key, str );
        msi_free(str);
    } while (cmp);

    g_object_unref(colnames);

    if (_libmsi_record_get_string_raw(rec, i))  /* check record field is a string */
        /* quote string record fields */
        str = g_strdup_printf ("'%s'", _libmsi_record_get_string_raw (rec, i));
    else
        str = libmsi_record_get_string (rec, i);

    return str;
}

static char *create_diff_row_query(LibmsiDatabase *merge, LibmsiQuery *view,
                                    char *table, LibmsiRecord *rec)
{
    GString *query;
    char *val;
    const char *key;
    LibmsiRecord *keys;
    unsigned r, i, count;

    r = _libmsi_database_get_primary_keys(merge, table, &keys);
    if (r != LIBMSI_RESULT_SUCCESS)
        return NULL;

    query = g_string_sized_new(256);
    g_string_printf (query, "SELECT * FROM %s WHERE ", table);
    count = libmsi_record_get_field_count(keys);
    for (i = 1; i <= count; i++)
    {
        key = _libmsi_record_get_string_raw(keys, i);
        val = get_key_value(view, key, rec);

        if (i == count)
            g_string_append_printf (query, "`%s` = %s", key, val);
        else
            g_string_append_printf (query, "`%s` = %s AND ", key, val);
    }

    g_object_unref(keys);
    return  g_string_free (query, FALSE);
}

static unsigned merge_diff_row(LibmsiRecord *rec, void *param)
{
    MERGEDATA *data = param;
    MERGETABLE *table = data->curtable;
    MERGEROW *mergerow;
    LibmsiQuery *dbview = NULL;
    LibmsiRecord *row = NULL;
    char *query = NULL;
    unsigned r = LIBMSI_RESULT_SUCCESS;
    GError *err = NULL;

    if (table_view_exists(data->db, table->name))
    {
        query = create_diff_row_query(data->merge, data->curview, table->name, rec);
        if (!query)
            return LIBMSI_RESULT_OUTOFMEMORY;

        dbview = libmsi_query_new(data->db, query, &err);
        if (err)
            goto done;

        r = _libmsi_query_execute(dbview, NULL);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;

        r = _libmsi_query_fetch(dbview, &row);
        if (r == LIBMSI_RESULT_SUCCESS && !_libmsi_record_compare(rec, row))
        {
            table->numconflicts++;
            goto done;
        }
        else if (r != NO_MORE_ITEMS)
            goto done;

        r = LIBMSI_RESULT_SUCCESS;
    }

    mergerow = msi_alloc(sizeof(MERGEROW));
    if (!mergerow)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        goto done;
    }

    mergerow->data = _libmsi_record_clone(rec);
    if (!mergerow->data)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        msi_free(mergerow);
        goto done;
    }

    list_add_tail(&table->rows, &mergerow->entry);

done:
    if (err)
        g_critical("%s", err->message);
    g_clear_error(&err);
    msi_free(query);
    g_object_unref(row);
    g_object_unref(dbview);
    return r;
}

static unsigned msi_get_table_labels(LibmsiDatabase *db, const char *table, char ***labels, unsigned *numlabels)
{
    unsigned r, i, count;
    LibmsiRecord *prec = NULL;

    r = _libmsi_database_get_primary_keys(db, table, &prec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    count = libmsi_record_get_field_count(prec);
    *numlabels = count + 1;
    *labels = msi_alloc((*numlabels)*sizeof(char *));
    if (!*labels)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        goto end;
    }

    (*labels)[0] = strdup(table);
    for (i=1; i<=count; i++ )
    {
        (*labels)[i] = strdup(_libmsi_record_get_string_raw(prec, i));
    }

end:
    g_object_unref(prec);
    return r;
}

static unsigned msi_get_query_columns(LibmsiQuery *query, char ***columns, unsigned *numcolumns)
{
    unsigned r, i, count;
    LibmsiRecord *prec = NULL;

    r = _libmsi_query_get_column_info(query, LIBMSI_COL_INFO_NAMES, &prec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    count = libmsi_record_get_field_count(prec);
    *columns = msi_alloc(count*sizeof(char *));
    if (!*columns)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        goto end;
    }

    for (i=1; i<=count; i++ )
    {
        (*columns)[i-1] = strdup(_libmsi_record_get_string_raw(prec, i));
    }

    *numcolumns = count;

end:
    g_object_unref(prec);
    return r;
}

static unsigned msi_get_query_types(LibmsiQuery *query, char ***types, unsigned *numtypes)
{
    unsigned r, i, count;
    LibmsiRecord *prec = NULL;

    r = _libmsi_query_get_column_info(query, LIBMSI_COL_INFO_TYPES, &prec);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    count = libmsi_record_get_field_count(prec);
    *types = msi_alloc(count*sizeof(char *));
    if (!*types)
    {
        r = LIBMSI_RESULT_OUTOFMEMORY;
        goto end;
    }

    *numtypes = count;
    for (i=1; i<=count; i++ )
    {
        (*types)[i-1] = strdup(_libmsi_record_get_string_raw(prec, i));
    }

end:
    g_object_unref(prec);
    return r;
}

static void merge_free_rows(MERGETABLE *table)
{
    struct list *item, *cursor;

    LIST_FOR_EACH_SAFE(item, cursor, &table->rows)
    {
        MERGEROW *row = LIST_ENTRY(item, MERGEROW, entry);

        list_remove(&row->entry);
        g_object_unref(row);
        msi_free(row);
    }
}

static void free_merge_table(MERGETABLE *table)
{
    unsigned i;

    if (table->labels != NULL)
    {
        for (i = 0; i < table->numlabels; i++)
            msi_free(table->labels[i]);

        msi_free(table->labels);
    }

    if (table->columns != NULL)
    {
        for (i = 0; i < table->numcolumns; i++)
            msi_free(table->columns[i]);

        msi_free(table->columns);
    }

    if (table->types != NULL)
    {
        for (i = 0; i < table->numtypes; i++)
            msi_free(table->types[i]);

        msi_free(table->types);
    }

    msi_free(table->name);
    merge_free_rows(table);

    msi_free(table);
}

static unsigned msi_get_merge_table (LibmsiDatabase *db, const char *name, MERGETABLE **ptable)
{
    unsigned r;
    MERGETABLE *table;
    LibmsiQuery *mergeview = NULL;

    static const char query[] = "SELECT * FROM %s";

    table = msi_alloc_zero(sizeof(MERGETABLE));
    if (!table)
    {
       *ptable = NULL;
       return LIBMSI_RESULT_OUTOFMEMORY;
    }

    r = msi_get_table_labels(db, name, &table->labels, &table->numlabels);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto err;

    r = _libmsi_query_open(db, &mergeview, query, name);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto err;

    r = msi_get_query_columns(mergeview, &table->columns, &table->numcolumns);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto err;

    r = msi_get_query_types(mergeview, &table->types, &table->numtypes);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto err;

    list_init(&table->rows);

    table->name = strdup(name);
    table->numconflicts = 0;

    g_object_unref(mergeview);
    *ptable = table;
    return LIBMSI_RESULT_SUCCESS;

err:
    g_object_unref(mergeview);
    free_merge_table(table);
    *ptable = NULL;
    return r;
}

static unsigned merge_diff_tables(LibmsiRecord *rec, void *param)
{
    MERGEDATA *data = param;
    MERGETABLE *table;
    LibmsiQuery *dbview = NULL;
    LibmsiQuery *mergeview = NULL;
    const char *name;
    unsigned r;

    static const char query[] = "SELECT * FROM %s";

    name = _libmsi_record_get_string_raw(rec, 1);

    r = _libmsi_query_open(data->merge, &mergeview, query, name);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    if (table_view_exists(data->db, name))
    {
        r = _libmsi_query_open(data->db, &dbview, query, name);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;

        r = merge_verify_colnames(dbview, mergeview);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;

        r = merge_verify_primary_keys(data->db, data->merge, name);
        if (r != LIBMSI_RESULT_SUCCESS)
            goto done;
    }

    r = msi_get_merge_table(data->merge, name, &table);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    data->curtable = table;
    data->curview = mergeview;
    r = _libmsi_query_iterate_records(mergeview, NULL, merge_diff_row, data);
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        free_merge_table(table);
        goto done;
    }

    list_add_tail(data->tabledata, &table->entry);

done:
    g_object_unref(dbview);
    g_object_unref(mergeview);
    return r;
}

static unsigned gather_merge_data(LibmsiDatabase *db, LibmsiDatabase *merge,
                              struct list *tabledata)
{
    static const char query[] = "SELECT * FROM _Tables";
    LibmsiQuery *view;
    MERGEDATA data;
    unsigned r;

    r = _libmsi_database_open_query(merge, query, &view);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    data.db = db;
    data.merge = merge;
    data.tabledata = tabledata;
    r = _libmsi_query_iterate_records(view, NULL, merge_diff_tables, &data);
    g_object_unref(view);
    return r;
}

static unsigned merge_table(LibmsiDatabase *db, MERGETABLE *table)
{
    unsigned r;
    MERGEROW *row;
    LibmsiView *tv;

    if (!table_view_exists(db, table->name))
    {
        r = msi_add_table_to_db(db, table->columns, table->types,
               table->labels, table->numlabels, table->numcolumns);
        if (r != LIBMSI_RESULT_SUCCESS)
           return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    LIST_FOR_EACH_ENTRY(row, &table->rows, MERGEROW, entry)
    {
        r = table_view_create(db, table->name, &tv);
        if (r != LIBMSI_RESULT_SUCCESS)
            return r;

        r = tv->ops->insert_row(tv, row->data, -1, false);
        tv->ops->delete(tv);

        if (r != LIBMSI_RESULT_SUCCESS)
            return r;
    }

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned update_merge_errors(LibmsiDatabase *db, const char *error,
                                char *table, unsigned numconflicts)
{
    unsigned r;
    LibmsiQuery *view;

    static const char create[] =
        "CREATE TABLE `%s` (`Table` CHAR(255) NOT NULL, "
	"`NumRowMergeConflicts` SHORT NOT NULL PRIMARY KEY `Table`)";
    static const char insert[] =
        "INSERT INTO `%s` (`Table`, `NumRowMergeConflicts`) VALUES ('%s', %d)";

    if (!table_view_exists(db, error))
    {
        r = _libmsi_query_open(db, &view, create, error);
        if (r != LIBMSI_RESULT_SUCCESS)
            return r;

        r = _libmsi_query_execute(view, NULL);
        g_object_unref(view);
        if (r != LIBMSI_RESULT_SUCCESS)
            return r;
    }

    r = _libmsi_query_open(db, &view, insert, error, table, numconflicts);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    r = _libmsi_query_execute(view, NULL);
    g_object_unref(view);
    return r;
}

/**
 * libmsi_database_merge:
 * @db: a %LibmsiDatabase to merge into
 * @merge: a %LibmsiDatabase to merge
 * @table: (allow-none): an optionnal table name
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Returns: %TRUE on success
 **/
gboolean
libmsi_database_merge (LibmsiDatabase *db,
                       LibmsiDatabase *merge,
                       const char *tablename,
                       GError **error)
{
    struct list tabledata = LIST_INIT(tabledata);
    struct list *item, *cursor;
    MERGETABLE *table;
    bool conflicts;
    unsigned r;

    TRACE("(%p, %p, %s)\n", db, merge,
          debugstr_a(tablename));

    g_return_val_if_fail (LIBMSI_IS_DATABASE (db), FALSE);
    g_return_val_if_fail (LIBMSI_IS_DATABASE (merge), FALSE);
    g_return_val_if_fail (!tablename || *tablename, FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    g_object_ref(db);
    g_object_ref(merge);
    r = gather_merge_data(db, merge, &tabledata);
    if (r != LIBMSI_RESULT_SUCCESS)
        goto done;

    conflicts = false;
    LIST_FOR_EACH_ENTRY(table, &tabledata, MERGETABLE, entry)
    {
        if (table->numconflicts)
        {
            conflicts = true;

            r = update_merge_errors(db, tablename, table->name,
                                    table->numconflicts);
            if (r != LIBMSI_RESULT_SUCCESS)
                break;
        }
        else
        {
            r = merge_table(db, table);
            if (r != LIBMSI_RESULT_SUCCESS)
                break;
        }
    }

    LIST_FOR_EACH_SAFE(item, cursor, &tabledata)
    {
        table = LIST_ENTRY(item, MERGETABLE, entry);
        list_remove(&table->entry);
        free_merge_table(table);
    }

    if (conflicts)
        r = LIBMSI_RESULT_FUNCTION_FAILED;

done:
    if (r != LIBMSI_RESULT_SUCCESS)
        g_set_error (error, LIBMSI_RESULT_ERROR, r, G_STRFUNC);
    g_object_unref(db);
    g_object_unref(merge);
    return r == LIBMSI_RESULT_SUCCESS;
}

/**
 * libmsi_database_is_readonly:
 * @db: a %LibmsiDatabase
 *
 * Returns: %TRUE if the database is read-only.
 **/
gboolean
libmsi_database_is_readonly (LibmsiDatabase *db)
{
    TRACE("%p\n", db);

    g_return_val_if_fail (LIBMSI_IS_DATABASE (db), TRUE);

    return db->flags & LIBMSI_DB_FLAGS_READONLY;
}

static void cache_infile_structure( LibmsiDatabase *db )
{
    int i, n;
    unsigned r;

    n = gsf_infile_num_children(db->infile);

    /* TODO: error handling */

    for (i = 0; i < n; i++)
    {
        GsfInput *in = gsf_infile_child_by_index(db->infile, i);
        const char* name = gsf_input_name(in);
        const uint8_t *name8 = (const uint8_t *)name;

        if (!name) {
            g_warn_if_reached();
            continue;
        }
        /* table streams are not in the _Streams table */
        if (!GSF_IS_INFILE(in) || gsf_infile_num_children(GSF_INFILE(in)) == -1) {
            /* UTF-8 encoding of 0x4840.  */
            if (name8[0] == 0xe4 && name8[1] == 0xa1 && name8[2] == 0x80)
            {
                g_autofree char *decname = NULL;

                decname = decode_streamname(name + 3);
                if ( !strcmp( decname, szStringPool ) ||
                     !strcmp( decname, szStringData ) )
                    continue;

                r = _libmsi_open_table( db, decname, false );
                g_warn_if_fail (r == LIBMSI_RESULT_SUCCESS);
            }
            else
            {
                r = msi_alloc_stream(db, name, GSF_INPUT(in));
                g_object_unref(G_OBJECT(in));
            }
        } else {
            msi_open_storage(db, name);
        }

    }
}

LibmsiResult _libmsi_database_open(LibmsiDatabase *db)
{
    GsfInput *in;
    GsfInfile *stg;
    uint8_t uuid[16];
    unsigned ret = LIBMSI_RESULT_OPEN_FAILED;

    TRACE("%p %s\n", db, db->path);

    in = gsf_input_stdio_new(db->path, NULL);
    if (!in)
    {
        g_warning("open file failed for %s\n", debugstr_a(db->path));
        return LIBMSI_RESULT_OPEN_FAILED;
    }
    stg = gsf_infile_msole_new( in, NULL );
    g_object_unref(G_OBJECT(in));
    if( !stg )
    {
        g_warning("open failed for %s\n", debugstr_a(db->path));
        return LIBMSI_RESULT_OPEN_FAILED;
    }

    if( !gsf_infile_msole_get_class_id (GSF_INFILE_MSOLE(stg), uuid))
    {
        g_warning("FIXME: Failed to stat storage\n");
        goto end;
    }

    if ( memcmp( uuid, clsid_msi_database, 16 ) != 0 &&
         memcmp( uuid, clsid_msi_patch, 16 ) != 0 &&
         memcmp( uuid, clsid_msi_transform, 16 ) != 0 )
    {
        g_critical("storage GUID is not a MSI database GUID %s\n",
             debugstr_guid(uuid) );
        goto end;
    }

    if ( db->flags & LIBMSI_DB_FLAGS_PATCH && memcmp( uuid, clsid_msi_patch, 16 ) != 0 )
    {
        g_critical("storage GUID is not the MSI patch GUID %s\n",
             debugstr_guid(uuid) );
        goto end;
    }

    db->infile = stg;
    g_object_ref(G_OBJECT(db->infile));

    cache_infile_structure( db );

    db->strings = msi_load_string_table( db->infile, &db->bytes_per_strref );
    if( !db->strings )
        goto end;

    ret = LIBMSI_RESULT_SUCCESS;
end:
    if (ret) {
        if (db->infile)
            g_object_unref(G_OBJECT(db->infile));
        db->infile = NULL;
    }
    g_object_unref(G_OBJECT(stg));
    return ret;
}

unsigned _libmsi_database_apply_transform( LibmsiDatabase *db,
                 const char *szTransformFile )
{
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    GsfInput *in;
    GsfInfile *stg;
    uint8_t uuid[16];

    TRACE("%p %s\n", db, debugstr_a(szTransformFile));
    in = gsf_input_stdio_new(szTransformFile, NULL);
    if (!in)
    {
        g_warning("open file failed for transform %s\n", debugstr_a(szTransformFile));
        return LIBMSI_RESULT_OPEN_FAILED;
    }
    stg = gsf_infile_msole_new( in, NULL );
    g_object_unref(G_OBJECT(in));

    if( !gsf_infile_msole_get_class_id (GSF_INFILE_MSOLE(stg), uuid))
    {
        g_warning("FIXME: Failed to stat storage\n");
        goto end;
    }

    if ( memcmp( uuid, clsid_msi_transform, 16 ) != 0 )
        goto end;

    if (TRACE_ON && stg)
        enum_stream_names( stg );

    ret = msi_table_apply_transform( db, stg );

end:
    g_object_unref(G_OBJECT(stg));

    return ret;
}

/**
 * libmsi_database_apply_transform:
 * @db: a %LibmsiDatabase
 * @file: an MST transform file path
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * FIXME
 *
 * Returns: %TRUE on success
 **/
gboolean
libmsi_database_apply_transform (LibmsiDatabase *db,
                                 const char *file,
                                 GError **error)
{
    unsigned r;

    g_return_val_if_fail (LIBMSI_IS_DATABASE (db), FALSE);
    g_return_val_if_fail (file, FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    g_object_ref(db);
    r = _libmsi_database_apply_transform (db, file);
    g_object_unref(db);

    if (r != LIBMSI_RESULT_SUCCESS)
        g_set_error_literal (error, LIBMSI_RESULT_ERROR, r, G_STRFUNC);

    return r == LIBMSI_RESULT_SUCCESS;
}

static int gsf_infile_copy(GsfInfile *inf, GsfOutfile *outf)
{
    int n = gsf_infile_num_children(inf);
    int i;

    for (i = 0; i < n; i++) {
        const char *name = gsf_infile_name_by_index(inf, i);
        GsfInput *child = gsf_infile_child_by_index(inf, i);
        GsfInfile *childf = GSF_IS_INFILE (child) ? GSF_INFILE (child) : NULL;
        gboolean is_dir = childf && gsf_infile_num_children (childf) > 0;
        GsfOutput *dest = gsf_outfile_new_child(outf, name, is_dir);
        gboolean ok;

        if (is_dir)
            ok = gsf_infile_copy(childf, GSF_OUTFILE(dest));
        else
            ok = gsf_input_copy(child, dest);

        g_object_unref(G_OBJECT(child));
        g_object_unref(G_OBJECT(dest));
        if (!ok)
            return false;
    }
    return true;
}

static unsigned commit_storage( const char *name, GsfInfile *stg, void *opaque)
{
    LibmsiDatabase *db = opaque;
    GsfOutfile *outstg;
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;

    TRACE("%s %p %p\n", debugstr_a(name), stg, opaque);

    outstg = GSF_OUTFILE(gsf_outfile_new_child( db->outfile, name, true ));
    if ( !outstg )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    if ( !gsf_infile_copy( stg, outstg ) )
        goto end;

    ret = LIBMSI_RESULT_SUCCESS;

end:
    gsf_output_close(GSF_OUTPUT(outstg));
    g_object_unref(G_OBJECT(outstg));
    return ret;
}

static unsigned commit_stream( const char *name, GsfInput *stm, void *opaque)
{
    LibmsiDatabase *db = opaque;
    GsfOutput *outstm;
    unsigned ret = LIBMSI_RESULT_FUNCTION_FAILED;
    g_autofree char *decname = NULL;

    decname = decode_streamname(name);
    TRACE("%s(%s) %p %p\n", debugstr_a(name), debugstr_a(decname), stm, opaque);

    outstm = gsf_outfile_new_child( db->outfile, name, false );
    if ( !outstm )
        return LIBMSI_RESULT_FUNCTION_FAILED;

    gsf_input_seek (stm, 0, G_SEEK_SET);
    gsf_output_seek (outstm, 0, G_SEEK_SET);
    if ( !gsf_input_copy( stm, outstm ))
        goto end;

    ret = LIBMSI_RESULT_SUCCESS;

end:
    gsf_output_close(GSF_OUTPUT(outstm));
    g_object_unref(G_OBJECT(outstm));
    return ret;
}

/**
 * libmsi_database_commit:
 * @db: a #LibmsiDatabase
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Returns: %TRUE on success.
 **/
gboolean
libmsi_database_commit (LibmsiDatabase *db, GError **error)
{
    unsigned r = LIBMSI_RESULT_SUCCESS;
    unsigned bytes_per_strref;

    TRACE ("%p\n", db);

    g_return_val_if_fail (LIBMSI_IS_DATABASE (db), FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    g_object_ref(db);
    if (db->flags & LIBMSI_DB_FLAGS_READONLY)
        goto end;

    /* FIXME: lock the database */

    r = msi_save_string_table (db->strings, db, &bytes_per_strref);
    if (r != LIBMSI_RESULT_SUCCESS) {
        g_set_error (error, LIBMSI_RESULT_ERROR, r,
                     "failed to save string table r=%08x\n", r);
        goto end;
    }

    r = msi_enum_db_storages (db, commit_storage, db);
    if (r != LIBMSI_RESULT_SUCCESS) {
        g_set_error (error, LIBMSI_RESULT_ERROR, r,
                     "failed to save storages r=%08x\n", r);
        goto end;
    }

    r = msi_enum_db_streams (db, commit_stream, db);
    if (r != LIBMSI_RESULT_SUCCESS) {
        g_set_error (error, LIBMSI_RESULT_ERROR, r,
                     "failed to save streams r=%08x\n", r);
        goto end;
    }

    r = _libmsi_database_commit_tables (db, bytes_per_strref);
    if (r != LIBMSI_RESULT_SUCCESS) {
        g_set_error (error, LIBMSI_RESULT_ERROR, r,
                     "failed to save tables r=%08x\n", r);
        goto end;
    }

    db->bytes_per_strref = bytes_per_strref;

    /* FIXME: unlock the database */

    _libmsi_database_close(db, true);
    db->flags &= ~LIBMSI_DB_FLAGS_CREATE;
    db->flags |= LIBMSI_DB_FLAGS_TRANSACT;
    _libmsi_database_open(db);
    _libmsi_database_start_transaction(db);

end:
    g_object_unref(db);

    return r == LIBMSI_RESULT_SUCCESS;
}

struct msi_primary_key_record_info
{
    unsigned n;
    LibmsiRecord *rec;
};

static unsigned msi_primary_key_iterator( LibmsiRecord *rec, void *param )
{
    struct msi_primary_key_record_info *info = param;
    const char *name;
    const char *table;
    unsigned type;

    type = libmsi_record_get_int( rec, 4 );
    if( type & MSITYPE_KEY )
    {
        info->n++;
        if( info->rec )
        {
            if ( info->n == 1 )
            {
                table = _libmsi_record_get_string_raw( rec, 1 );
                libmsi_record_set_string( info->rec, 0, table);
            }

            name = _libmsi_record_get_string_raw( rec, 3 );
            libmsi_record_set_string( info->rec, info->n, name );
        }
    }

    return LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_database_get_primary_keys( LibmsiDatabase *db,
                const char *table, LibmsiRecord **prec )
{
    static const char sql[] = "select * from `_Columns` where `Table` = '%s'";
    struct msi_primary_key_record_info info;
    LibmsiQuery *query = NULL;
    unsigned r;

    if (!table_view_exists( db, table ))
        return LIBMSI_RESULT_INVALID_TABLE;

    r = _libmsi_query_open( db, &query, sql, table );
    if( r != LIBMSI_RESULT_SUCCESS )
        return r;

    /* count the number of primary key records */
    info.n = 0;
    info.rec = 0;
    r = _libmsi_query_iterate_records( query, 0, msi_primary_key_iterator, &info );
    if( r == LIBMSI_RESULT_SUCCESS )
    {
        TRACE("Found %d primary keys\n", info.n );

        /* allocate a record and fill in the names of the tables */
        info.rec = libmsi_record_new( info.n );
        info.n = 0;
        r = _libmsi_query_iterate_records( query, 0, msi_primary_key_iterator, &info );
        if( r == LIBMSI_RESULT_SUCCESS )
            *prec = info.rec;
        else
            g_object_unref(info.rec);
    }
    g_object_unref(query);

    return r;
}

/**
 * libmsi_database_get_primary_keys:
 * @db: a %LibmsiDatabase
 * @table: an exisiting table name
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Returns: (transfer full): a %LibmsiRecord containing the names of all the primary
 * key columns.
 **/
LibmsiRecord *
libmsi_database_get_primary_keys (LibmsiDatabase *db,
                                  const char *table,
                                  GError **error)
{
    LibmsiRecord *rec;
    unsigned r;

    TRACE("%p %s\n", db, debugstr_a(table));

    g_return_val_if_fail (LIBMSI_IS_DATABASE (db), NULL);
    g_return_val_if_fail (table != NULL, NULL);
    g_return_val_if_fail (!error || *error == NULL, NULL);

    g_object_ref(db);
    r = _libmsi_database_get_primary_keys(db, table, &rec);
    g_object_unref(db);

    if (r != LIBMSI_RESULT_SUCCESS)
        g_set_error_literal (error, LIBMSI_RESULT_ERROR, r, G_STRFUNC);

    return rec;
}

/**
 * libmsi_database_is_table_persistent:
 * @db: a %LibmsiDatabase
 * @table: an exisiting table name
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Returns: %TRUE if the @table is persistent, %FALSE if it's temporary
 **/
gboolean
libmsi_database_is_table_persistent (LibmsiDatabase *db, const char *table,
                                     GError **error)
{
    LibmsiCondition r;

    TRACE("%p %s\n", db, debugstr_a(table));

    g_return_val_if_fail (LIBMSI_IS_DATABASE (db), FALSE);
    g_return_val_if_fail (table != NULL, FALSE);
    g_return_val_if_fail (!error || *error == NULL, FALSE);

    g_object_ref(db);
    r = _libmsi_database_is_table_persistent( db, table );
    g_object_unref(db);

    if (r == LIBMSI_CONDITION_NONE)
        g_set_error (error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_INVALID_TABLE,
                     "The table is unknown");
    else if (r == LIBMSI_CONDITION_ERROR)
        g_set_error (error, LIBMSI_RESULT_ERROR, LIBMSI_RESULT_FUNCTION_FAILED,
                     "Error");

    return r == LIBMSI_CONDITION_TRUE;
}

/* TODO: use GInitable */
static gboolean
init (LibmsiDatabase *self, GError **error)
{
    LibmsiResult ret;

    if (self->flags & LIBMSI_DB_FLAGS_CREATE) {
        self->strings = msi_init_string_table (&self->bytes_per_strref);
    } else {
        if (_libmsi_database_open(self))
            return FALSE;
    }

    self->media_transform_offset = MSI_INITIAL_MEDIA_TRANSFORM_OFFSET;
    self->media_transform_disk_id = MSI_INITIAL_MEDIA_TRANSFORM_DISKID;

    if (TRACE_ON && self->infile)
        enum_stream_names (self->infile);

    ret = _libmsi_database_start_transaction (self);

    return !ret;
}

/**
 * libmsi_database_new:
 * @path: path to a MSI file
 * @flags: #LibmsiDbFlags opening flags
 * @persist: (allow-none): path to output MSI file
 * @error: (allow-none): #GError to set on error, or %NULL
 *
 * Create a MSI database or open from @path.
 *
 * Returns: a new #LibmsiDatabase on success, %NULL if fail.
 **/
LibmsiDatabase *
libmsi_database_new (const gchar *path,
                     guint flags,
                     const char *persist,
                     GError **error)
{
    LibmsiDatabase *self;

    g_return_val_if_fail (path != NULL, NULL);
    g_return_val_if_fail (!error || *error == NULL, NULL);

    self = g_object_new (LIBMSI_TYPE_DATABASE,
                         "path", path,
                         "outpath", persist,
                         "flags", flags,
                         NULL);

    if (!init (self, error)) {
        g_object_unref (self);
        return NULL;
    }

    return self;
}
