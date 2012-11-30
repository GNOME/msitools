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

typedef struct LibmsiStreamSVIEW
{
    LibmsiView view;
    LibmsiDatabase *db;
    STREAM **streams;
    unsigned max_streams;
    unsigned num_rows;
    unsigned row_size;
} LibmsiStreamSVIEW;

static bool streams_set_table_size(LibmsiStreamSVIEW *sv, unsigned size)
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

static STREAM *create_stream(LibmsiStreamSVIEW *sv, const WCHAR *name, bool encoded, IStream *stm)
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

    stream->str_index = msi_addstringW(sv->db->strings, name, -1, 1, StringNonPersistent);
    stream->stream = stm;
    return stream;
}

static unsigned STREAMS_fetch_int(LibmsiView *view, unsigned row, unsigned col, unsigned *val)
{
    LibmsiStreamSVIEW *sv = (LibmsiStreamSVIEW *)view;

    TRACE("(%p, %d, %d, %p)\n", view, row, col, val);

    if (col != 1)
        return ERROR_INVALID_PARAMETER;

    if (row >= sv->num_rows)
        return ERROR_NO_MORE_ITEMS;

    *val = sv->streams[row]->str_index;

    return ERROR_SUCCESS;
}

static unsigned STREAMS_fetch_stream(LibmsiView *view, unsigned row, unsigned col, IStream **stm)
{
    LibmsiStreamSVIEW *sv = (LibmsiStreamSVIEW *)view;

    TRACE("(%p, %d, %d, %p)\n", view, row, col, stm);

    if (row >= sv->num_rows)
        return ERROR_FUNCTION_FAILED;

    IStream_AddRef(sv->streams[row]->stream);
    *stm = sv->streams[row]->stream;

    return ERROR_SUCCESS;
}

static unsigned STREAMS_get_row( LibmsiView *view, unsigned row, LibmsiRecord **rec )
{
    LibmsiStreamSVIEW *sv = (LibmsiStreamSVIEW *)view;

    TRACE("%p %d %p\n", sv, row, rec);

    return msi_view_get_row( sv->db, view, row, rec );
}

static unsigned STREAMS_set_row(LibmsiView *view, unsigned row, LibmsiRecord *rec, unsigned mask)
{
    LibmsiStreamSVIEW *sv = (LibmsiStreamSVIEW *)view;
    STREAM *stream;
    IStream *stm;
    STATSTG stat;
    WCHAR *encname = NULL;
    WCHAR *name = NULL;
    uint16_t *data = NULL;
    HRESULT hr;
    unsigned count;
    unsigned r = ERROR_FUNCTION_FAILED;

    TRACE("(%p, %d, %p, %08x)\n", view, row, rec, mask);

    if (row > sv->num_rows)
        return ERROR_FUNCTION_FAILED;

    r = MSI_RecordGetIStream(rec, 2, &stm);
    if (r != ERROR_SUCCESS)
        return r;

    hr = IStream_Stat(stm, &stat, STATFLAG_NONAME);
    if (FAILED(hr))
    {
        WARN("failed to stat stream: %08x\n", hr);
        goto done;
    }

    if (stat.cbSize.QuadPart >> 32)
    {
        WARN("stream too large\n");
        goto done;
    }

    data = msi_alloc(stat.cbSize.QuadPart);
    if (!data)
        goto done;

    hr = IStream_Read(stm, data, stat.cbSize.QuadPart, &count);
    if (FAILED(hr) || count != stat.cbSize.QuadPart)
    {
        WARN("failed to read stream: %08x\n", hr);
        goto done;
    }

    name = strdupW(MSI_RecordGetStringRaw(rec, 1));
    if (!name)
    {
        WARN("failed to retrieve stream name\n");
        goto done;
    }

    encname = encode_streamname(false, name);
    msi_destroy_stream(sv->db, encname);

    r = write_stream_data(sv->db->storage, name, data, count, false);
    if (r != ERROR_SUCCESS)
    {
        WARN("failed to write stream data: %d\n", r);
        goto done;
    }

    stream = create_stream(sv, name, false, NULL);
    if (!stream)
        goto done;

    hr = IStorage_OpenStream(sv->db->storage, encname, 0,
                             STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stream->stream);
    if (FAILED(hr))
    {
        WARN("failed to open stream: %08x\n", hr);
        goto done;
    }

    sv->streams[row] = stream;

done:
    msi_free(name);
    msi_free(data);
    msi_free(encname);

    IStream_Release(stm);

    return r;
}

static unsigned STREAMS_insert_row(LibmsiView *view, LibmsiRecord *rec, unsigned row, bool temporary)
{
    LibmsiStreamSVIEW *sv = (LibmsiStreamSVIEW *)view;
    unsigned i;

    TRACE("(%p, %p, %d, %d)\n", view, rec, row, temporary);

    if (!streams_set_table_size(sv, ++sv->num_rows))
        return ERROR_FUNCTION_FAILED;

    if (row == -1)
        row = sv->num_rows - 1;

    /* shift the rows to make room for the new row */
    for (i = sv->num_rows - 1; i > row; i--)
    {
        sv->streams[i] = sv->streams[i - 1];
    }

    return STREAMS_set_row(view, row, rec, 0);
}

static unsigned STREAMS_delete_row(LibmsiView *view, unsigned row)
{
    FIXME("(%p %d): stub!\n", view, row);
    return ERROR_SUCCESS;
}

static unsigned STREAMS_execute(LibmsiView *view, LibmsiRecord *record)
{
    TRACE("(%p, %p)\n", view, record);
    return ERROR_SUCCESS;
}

static unsigned STREAMS_close(LibmsiView *view)
{
    TRACE("(%p)\n", view);
    return ERROR_SUCCESS;
}

static unsigned STREAMS_get_dimensions(LibmsiView *view, unsigned *rows, unsigned *cols)
{
    LibmsiStreamSVIEW *sv = (LibmsiStreamSVIEW *)view;

    TRACE("(%p, %p, %p)\n", view, rows, cols);

    if (cols) *cols = NUM_STREAMS_COLS;
    if (rows) *rows = sv->num_rows;

    return ERROR_SUCCESS;
}

static unsigned STREAMS_get_column_info( LibmsiView *view, unsigned n, const WCHAR **name,
                                     unsigned *type, bool *temporary, const WCHAR **table_name )
{
    TRACE("(%p, %d, %p, %p, %p, %p)\n", view, n, name, type, temporary,
          table_name);

    if (n == 0 || n > NUM_STREAMS_COLS)
        return ERROR_INVALID_PARAMETER;

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
    return ERROR_SUCCESS;
}

static unsigned streams_find_row(LibmsiStreamSVIEW *sv, LibmsiRecord *rec, unsigned *row)
{
    const WCHAR *str;
    unsigned r, i, id, data;

    str = MSI_RecordGetStringRaw(rec, 1);
    r = msi_string2idW(sv->db->strings, str, &id);
    if (r != ERROR_SUCCESS)
        return r;

    for (i = 0; i < sv->num_rows; i++)
    {
        STREAMS_fetch_int(&sv->view, i, 1, &data);

        if (data == id)
        {
            *row = i;
            return ERROR_SUCCESS;
        }
    }

    return ERROR_FUNCTION_FAILED;
}

static unsigned streams_modify_update(LibmsiView *view, LibmsiRecord *rec)
{
    LibmsiStreamSVIEW *sv = (LibmsiStreamSVIEW *)view;
    unsigned r, row;

    r = streams_find_row(sv, rec, &row);
    if (r != ERROR_SUCCESS)
        return ERROR_FUNCTION_FAILED;

    return STREAMS_set_row(view, row, rec, 0);
}

static unsigned streams_modify_assign(LibmsiView *view, LibmsiRecord *rec)
{
    LibmsiStreamSVIEW *sv = (LibmsiStreamSVIEW *)view;
    unsigned r, row;

    r = streams_find_row(sv, rec, &row);
    if (r == ERROR_SUCCESS)
        return streams_modify_update(view, rec);

    return STREAMS_insert_row(view, rec, -1, false);
}

static unsigned STREAMS_modify(LibmsiView *view, LibmsiModify eModifyMode, LibmsiRecord *rec, unsigned row)
{
    unsigned r;

    TRACE("%p %d %p\n", view, eModifyMode, rec);

    switch (eModifyMode)
    {
    case LIBMSI_MODIFY_ASSIGN:
        r = streams_modify_assign(view, rec);
        break;

    case LIBMSI_MODIFY_INSERT:
        r = STREAMS_insert_row(view, rec, -1, false);
        break;

    case LIBMSI_MODIFY_UPDATE:
        r = streams_modify_update(view, rec);
        break;

    case LIBMSI_MODIFY_VALIDATE_NEW:
    case LIBMSI_MODIFY_INSERT_TEMPORARY:
    case LIBMSI_MODIFY_REFRESH:
    case LIBMSI_MODIFY_REPLACE:
    case LIBMSI_MODIFY_MERGE:
    case LIBMSI_MODIFY_DELETE:
    case LIBMSI_MODIFY_VALIDATE:
    case LIBMSI_MODIFY_VALIDATE_FIELD:
    case LIBMSI_MODIFY_VALIDATE_DELETE:
        FIXME("%p %d %p - mode not implemented\n", view, eModifyMode, rec );
        r = ERROR_CALL_NOT_IMPLEMENTED;
        break;

    default:
        r = ERROR_INVALID_DATA;
    }

    return r;
}

static unsigned STREAMS_delete(LibmsiView *view)
{
    LibmsiStreamSVIEW *sv = (LibmsiStreamSVIEW *)view;
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

    return ERROR_SUCCESS;
}

static unsigned STREAMS_find_matching_rows(LibmsiView *view, unsigned col,
                                       unsigned val, unsigned *row, MSIITERHANDLE *handle)
{
    LibmsiStreamSVIEW *sv = (LibmsiStreamSVIEW *)view;
    unsigned index = PtrToUlong(*handle);

    TRACE("(%p, %d, %d, %p, %p)\n", view, col, val, row, handle);

    if (col == 0 || col > NUM_STREAMS_COLS)
        return ERROR_INVALID_PARAMETER;

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
        return ERROR_NO_MORE_ITEMS;

    return ERROR_SUCCESS;
}

static const LibmsiViewOPS streams_ops =
{
    STREAMS_fetch_int,
    STREAMS_fetch_stream,
    STREAMS_get_row,
    STREAMS_set_row,
    STREAMS_insert_row,
    STREAMS_delete_row,
    STREAMS_execute,
    STREAMS_close,
    STREAMS_get_dimensions,
    STREAMS_get_column_info,
    STREAMS_modify,
    STREAMS_delete,
    STREAMS_find_matching_rows,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static int add_streams_to_table(LibmsiStreamSVIEW *sv)
{
    IEnumSTATSTG *stgenum = NULL;
    STATSTG stat;
    STREAM *stream = NULL;
    HRESULT hr;
    unsigned r, count = 0, size;

    hr = IStorage_EnumElements(sv->db->storage, 0, NULL, 0, &stgenum);
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

        if (r != ERROR_SUCCESS)
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

unsigned STREAMS_CreateView(LibmsiDatabase *db, LibmsiView **view)
{
    LibmsiStreamSVIEW *sv;
    int rows;

    TRACE("(%p, %p)\n", db, view);

    sv = alloc_msiobject( sizeof(LibmsiStreamSVIEW), NULL );
    if (!sv)
        return ERROR_FUNCTION_FAILED;

    sv->view.ops = &streams_ops;
    sv->db = db;
    rows = add_streams_to_table(sv);
    if (rows < 0)
    {
        msi_free( sv );
        return ERROR_FUNCTION_FAILED;
    }
    sv->num_rows = rows;

    *view = (LibmsiView *)sv;

    return ERROR_SUCCESS;
}
