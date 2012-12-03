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

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "libmsi.h"
#include "objbase.h"
#include "msipriv.h"
#include "query.h"

#include "debug.h"
#include "unicode.h"


#define NUM_STREAMS_COLS    2

typedef struct tabSTREAM
{
    unsigned str_index;
    IStream *stream;
} STREAM;

typedef struct LibmsiStreamsView
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

static STREAM *create_stream(LibmsiStreamsView *sv, const WCHAR *name, bool encoded, IStream *stm)
{
    STREAM *stream;
    WCHAR decoded[MAX_STREAM_NAME_LEN];

    stream = msi_alloc(sizeof(STREAM));
    if (!stream)
        return NULL;

    if (encoded)
    {
        decode_streamname(name, decoded);
        TRACE("stream -> %s %s\n", debugstr_w(name), debugstr_w(decoded));
        name = decoded;
    }

    stream->str_index = _libmsi_add_string(sv->db->strings, name, -1, 1, StringNonPersistent);
    stream->stream = stm;
    return stream;
}

static unsigned streams_view_fetch_int(LibmsiView *view, unsigned row, unsigned col, unsigned *val)
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)view;

    TRACE("(%p, %d, %d, %p)\n", view, row, col, val);

    if (col != 1)
        return LIBMSI_RESULT_INVALID_PARAMETER;

    if (row >= sv->num_rows)
        return LIBMSI_RESULT_NO_MORE_ITEMS;

    *val = sv->streams[row]->str_index;

    return LIBMSI_RESULT_SUCCESS;
}

static unsigned streams_view_fetch_stream(LibmsiView *view, unsigned row, unsigned col, IStream **stm)
{
    LibmsiStreamsView *sv = (LibmsiStreamsView *)view;

    TRACE("(%p, %d, %d, %p)\n", view, row, col, stm);

    if (row >= sv->num_rows)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    IStream_AddRef(sv->streams[row]->stream);
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
    IStream *stm;
    WCHAR *name = NULL;
    unsigned r;

    TRACE("(%p, %d, %p, %08x)\n", view, row, rec, mask);

    if (row > sv->num_rows)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    r = _libmsi_record_get_IStream(rec, 2, &stm);
    if (r != LIBMSI_RESULT_SUCCESS)
        return r;

    name = strdupW(_libmsi_record_get_string_raw(rec, 1));
    if (!name)
    {
        WARN("failed to retrieve stream name\n");
        goto done;
    }

    stream = create_stream(sv, name, false, NULL);
    if (!stream)
        goto done;

    r = msi_create_stream(sv->db, name, stm, &stream->stream);
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        WARN("failed to create stream: %08x\n", r);
        goto done;
    }

    sv->streams[row] = stream;

done:
    if (r != LIBMSI_RESULT_SUCCESS)
        msi_free(stream);

    msi_free(name);

    IStream_Release(stm);

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
    const WCHAR *name;
    WCHAR *encname;
    unsigned i;

    if (row > sv->num_rows)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    name = msi_string_lookup_id(sv->db->strings, sv->streams[row]->str_index);
    if (!name)
    {
        WARN("failed to retrieve stream name\n");
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

static unsigned streams_view_get_column_info( LibmsiView *view, unsigned n, const WCHAR **name,
                                     unsigned *type, bool *temporary, const WCHAR **table_name )
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
                IStream_Release(sv->streams[i]->stream);
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
    unsigned index = PtrToUlong(*handle);

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

    *handle = UlongToPtr(++index);

    if (index > sv->num_rows)
        return LIBMSI_RESULT_NO_MORE_ITEMS;

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

static int add_streams_to_table(LibmsiStreamsView *sv)
{
    IEnumSTATSTG *stgenum = NULL;
    STATSTG stat;
    STREAM *stream = NULL;
    HRESULT hr;
    unsigned r, count = 0, size;

    hr = IStorage_EnumElements(sv->db->infile, 0, NULL, 0, &stgenum);
    if (FAILED(hr))
        return -1;

    sv->max_streams = 1;
    sv->streams = msi_alloc_zero(sizeof(STREAM *));
    if (!sv->streams)
        return -1;

    while (true)
    {
        size = 0;
        hr = IEnumSTATSTG_Next(stgenum, 1, &stat, &size);
        if (FAILED(hr) || !size)
            break;

        if (stat.type != STGTY_STREAM)
        {
            CoTaskMemFree(stat.pwcsName);
            continue;
        }

        /* table streams are not in the _Streams table */
        if (*stat.pwcsName == 0x4840)
        {
            CoTaskMemFree(stat.pwcsName);
            continue;
        }

        stream = create_stream(sv, stat.pwcsName, true, NULL);
        if (!stream)
        {
            count = -1;
            CoTaskMemFree(stat.pwcsName);
            break;
        }

        r = msi_get_raw_stream(sv->db, stat.pwcsName, &stream->stream);
        CoTaskMemFree(stat.pwcsName);

        if (r != LIBMSI_RESULT_SUCCESS)
        {
            WARN("unable to get stream %u\n", r);
            count = -1;
            break;
        }

        if (!streams_set_table_size(sv, ++count))
        {
            count = -1;
            break;
        }

        sv->streams[count - 1] = stream;
    }

    IEnumSTATSTG_Release(stgenum);
    return count;
}

unsigned streams_view_create(LibmsiDatabase *db, LibmsiView **view)
{
    LibmsiStreamsView *sv;
    int rows;

    TRACE("(%p, %p)\n", db, view);

    sv = alloc_msiobject( sizeof(LibmsiStreamsView), NULL );
    if (!sv)
        return LIBMSI_RESULT_FUNCTION_FAILED;

    sv->view.ops = &streams_ops;
    sv->db = db;
    rows = add_streams_to_table(sv);
    if (rows < 0)
    {
        msi_free( sv );
        return LIBMSI_RESULT_FUNCTION_FAILED;
    }
    sv->num_rows = rows;

    *view = (LibmsiView *)sv;

    return LIBMSI_RESULT_SUCCESS;
}

unsigned _libmsi_database_commit_streams( LibmsiDatabase *db )
{
    return LIBMSI_RESULT_SUCCESS;
}
