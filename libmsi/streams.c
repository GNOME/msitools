/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2007 James Hawkins
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

#include "libmsi.h"
#include "msipriv.h"
#include "query.h"

#include "debug.h"


#define NUM_STREAMS_COLS    2

typedef struct tabSTREAM
{
    unsigned str_index;
    GsfInput *stream;
} STREAM;

typedef struct _LibmsiStreamsView
{
    LibmsiView view;
    LibmsiDatabase *db;
    STREAM **streams;
    unsigned max_streams;
    unsigned num_rows;
    unsigned row_size;
} LibmsiStreamsView;

static bool streams_set_table_size(LibmsiStreamsView *sv, unsigned size)
{
    if (size >= sv->max_streams)
    {
        sv->streams = msi_realloc_zero(sv->streams, sv->max_streams * sizeof(STREAM *),
                                       sv->max_streams * 2 * sizeof(STREAM *));
        sv->max_streams *= 2;
        if (!sv->streams)
            return false;
    }

    return true;
}

static STREAM *create_stream(LibmsiStreamsView *sv, const char *name, bool encoded, GsfInput *stm)
{
    STREAM *stream;
    g_autofree char *decoded = NULL;

    stream = msi_alloc(sizeof(STREAM));
    if (!stream)
        return NULL;

    if (encoded)
    {
        decoded = decode_streamname(name);
        TRACE("stream -> %s %s\n", debugstr_a(name), debugstr_a(decoded));
        name = decoded;
    }

    stream->str_index = _libmsi_add_string(sv->db->strings, name, -1, 1, StringNonPersistent);
    stream->stream = stm;
    if (stream->stream)
        g_object_ref(G_OBJECT(stm));

    return stream;
}

static unsigned streams_view_fetch_int(LibmsiView *view, unsigned row, unsigned col, unsigned *val)
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)view;

    TRACE("(%p, %d, %d, %p)\n", view, row, col, val);

    if (col != 1)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if (row >= sv->num_rows)
        return NO_MORE_ITEMS;

    *val = sv->streams[row]->str_index;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned streams_view_fetch_stream(LibmsiView *view, unsigned row, unsigned col, GsfInput **stm)
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)view;

    TRACE("(%p, %d, %d, %p)\n", view, row, col, stm);

    if (row >= sv->num_rows)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    g_object_ref(G_OBJECT(sv->streams[row]->stream));
    *stm = sv->streams[row]->stream;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned streams_view_get_row( LibmsiView *view, unsigned row, LibmsiRecord **rec )
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)view;

    TRACE("%p %d %p\n", sv, row, rec);

    return msi_view_get_row( sv->db, view, row, rec );
}

static unsigned streams_view_set_row(LibmsiView *view, unsigned row, LibmsiRecord *rec, unsigned mask)
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)view;
    STREAM *stream = NULL;
    GsfInput *stm;
    char *name = NULL;
    unsigned r;

    TRACE("(%p, %d, %p, %08x)\n", view, row, rec, mask);

    if (row > sv->num_rows)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    r = _libmsi_record_get_gsf_input(rec, 2, &stm);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    if (sv->streams[row]) {
        if (mask & 1) {
            g_warning("FIXME: renaming stream via UPDATE on _Streams table");
            goto done;
        }

        stream = sv->streams[row];
        name = strdup(msi_string_lookup_id(sv->db->strings, stream->str_index));
    } else {
        name = strdup(_libmsi_record_get_string_raw(rec, 1));
        if (!name)
        {
            g_warning("failed to retrieve stream name\n");
            goto done;
        }

        stream = create_stream(sv, name, false, stm);
    }

    if (!stream)
        goto done;

    r = msi_create_stream(sv->db, name, stm);
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        g_warning("failed to create stream: %08x\n", r);
        g_object_unref(G_OBJECT(stream->stream));
        msi_free(stream);
        goto done;
    }

    sv->streams[row] = stream;

done:
    msi_free(name);

    g_object_unref(G_OBJECT(stm));

    return r;
}

static unsigned streams_view_insert_row(LibmsiView *view, LibmsiRecord *rec, unsigned row, bool temporary)
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)view;
    unsigned i;

    TRACE("(%p, %p, %d, %d)\n", view, rec, row, temporary);

    if (!streams_set_table_size(sv, ++sv->num_rows))
        return LIBMSI_RESULT_FUNCTION_FAILED;

    if (row == -1)
        row = sv->num_rows - 1;

    /* shift the rows to make room for the new row */
    for (i = sv->num_rows - 1; i > row; i--)
    {
        sv->streams[i] = sv->streams[i - 1];
    }

    return streams_view_set_row(view, row, rec, 0);
}

static unsigned streams_view_delete_row(LibmsiView *view, unsigned row)
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)view;
    const char *name;
    char *encname;
    unsigned i;

    if (row > sv->num_rows)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    name = msi_string_lookup_id(sv->db->strings, sv->streams[row]->str_index);
    if (!name)
    {
        g_warning("failed to retrieve stream name\n");
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }

    encname = encode_streamname(false, name);
    msi_destroy_stream(sv->db, encname);

    /* shift the remaining rows */
    for (i = row + 1; i < sv->num_rows; i++)
    {
        sv->streams[i - 1] = sv->streams[i];
    }
    sv->num_rows--;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned streams_view_execute(LibmsiView *view, LibmsiRecord *record)
{
    TRACE("(%p, %p)\n", view, record);
    return LIBMSI_RESULT_SUCCESS;
}

static unsigned streams_view_close(LibmsiView *view)
{
    TRACE("(%p)\n", view);
    return LIBMSI_RESULT_SUCCESS;
}

static unsigned streams_view_get_dimensions(LibmsiView *view, unsigned *rows, unsigned *cols)
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)view;

    TRACE("(%p, %p, %p)\n", view, rows, cols);

    if (cols) *cols = NUM_STREAMS_COLS;
    if (rows) *rows = sv->num_rows;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned streams_view_get_column_info( LibmsiView *view, unsigned n, const char **name,
                                     unsigned *type, bool *temporary, const char **table_name )
{
    TRACE("(%p, %d, %p, %p, %p, %p)\n", view, n, name, type, temporary,
          table_name);

    if (n == 0 || n > NUM_STREAMS_COLS)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    switch (n)
    {
    case 1:
        if (name) *name = szName;
        if (type) *type = MSITYPE_STRING | MSITYPE_VALID | MAX_STREAM_NAME_LEN;
        break;

    case 2:
        if (name) *name = szData;
        if (type) *type = MSITYPE_STRING | MSITYPE_VALID | MSITYPE_NULLABLE;
        break;

    default:
        g_warn_if_reached ();
    }
    if (table_name) *table_name = szStreams;
    if (temporary) *temporary = false;
    return LIBMSI_RESULT_SUCCESS;
}

static unsigned streams_view_delete(LibmsiView *view)
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)view;
    unsigned i;

    TRACE("(%p)\n", view);

    for (i = 0; i < sv->num_rows; i++)
    {
        if (sv->streams[i])
        {
            if (sv->streams[i]->stream)
                g_object_unref(G_OBJECT(sv->streams[i]->stream));
            msi_free(sv->streams[i]);
        }
    }

    msi_free(sv->streams);
    msi_free(sv);

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned streams_view_find_matching_rows(LibmsiView *view, unsigned col,
                                       unsigned val, unsigned *row, MSIITERHANDLE *handle)
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)view;
    unsigned index = (uintptr_t)(*handle);

    TRACE("(%p, %d, %d, %p, %p)\n", view, col, val, row, handle);

    if (col == 0 || col > NUM_STREAMS_COLS)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    while (index < sv->num_rows)
    {
        if (sv->streams[index]->str_index == val)
        {
            *row = index;
            break;
        }

        index++;
    }

    *handle = (MSIITERHANDLE)(uintptr_t)++index;

    if (index > sv->num_rows)
        return NO_MORE_ITEMS;

    return LIBMSI_RESULT_SUCCESS;
}

static const LibmsiViewOps streams_ops =
{
    streams_view_fetch_int,
    streams_view_fetch_stream,
    streams_view_get_row,
    streams_view_set_row,
    streams_view_insert_row,
    streams_view_delete_row,
    streams_view_execute,
    streams_view_close,
    streams_view_get_dimensions,
    streams_view_get_column_info,
    streams_view_delete,
    streams_view_find_matching_rows,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static unsigned add_stream_to_table(const char *name, GsfInput *stm, void *opaque)
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)opaque;
    STREAM *stream;

    stream = create_stream(sv, name, true, stm);
    if (!stream)
        return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;

    if (!streams_set_table_size(sv, ++sv->num_rows))
    {
        msi_free(stream);
        return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;
    }

    sv->streams[sv->num_rows - 1] = stream;
    return LIBMSI_RESULT_SUCCESS;
}

static unsigned add_streams_to_table(LibmsiStreamsView *sv)
{
    sv->max_streams = 1;
    sv->streams = msi_alloc_zero(sizeof(STREAM *));
    if (!sv->streams)
        return LIBMSI_RESULT_NOT_ENOUGH_MEMORY;

    return msi_enum_db_streams(sv->db, add_stream_to_table, sv);
}

unsigned streams_view_create(LibmsiDatabase *db, LibmsiView **view)
{
    LibmsiStreamsView *sv;
    unsigned r;

    TRACE("(%p, %p)\n", db, view);

    sv = msi_alloc_zero( sizeof(LibmsiStreamsView) );
    if (!sv)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    sv->view.ops = &streams_ops;
    sv->db = db;
    r = add_streams_to_table(sv);
    if (r)
    {
        msi_free( sv );
        return r;
    }

    *view = (LibmsiView *)sv;

    return LIBMSI_RESULT_SUCCESS;
}
