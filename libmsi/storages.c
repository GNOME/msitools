/*
 * Implementation of the Microsoft Installer (msi.dll)
 *
 * Copyright 2008 James Hawkins
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
#include "ole2.h"
#include "libmsi.h"
#include "objbase.h"
#include "msipriv.h"
#include "query.h"

#include "debug.h"


#define NUM_STORAGES_COLS    2
#define MAX_STORAGES_NAME_LEN 62

typedef struct tabSTORAGE
{
    unsigned str_index;
    IStorage *storage;
} STORAGE;

typedef struct LibmsiStorageView
{
    LibmsiView view;
    LibmsiDatabase *db;
    STORAGE **storages;
    unsigned max_storages;
    unsigned num_rows;
    unsigned row_size;
} LibmsiStorageView;

static bool storages_set_table_size(LibmsiStorageView *sv, unsigned size)
{
    if (size >= sv->max_storages)
    {
        sv->max_storages *= 2;
        sv->storages = msi_realloc(sv->storages, sv->max_storages * sizeof(STORAGE *));
        if (!sv->storages)
            return false;
    }

    return true;
}

static STORAGE *create_storage(LibmsiStorageView *sv, const WCHAR *name, IStorage *stg)
{
    STORAGE *storage;

    storage = msi_alloc(sizeof(STORAGE));
    if (!storage)
        return NULL;

    storage->str_index = msi_addstringW(sv->db->strings, name, -1, 1, StringNonPersistent);
    storage->storage = stg;

    if (storage->storage)
        IStorage_AddRef(storage->storage);

    return storage;
}

static unsigned STORAGES_fetch_int(LibmsiView *view, unsigned row, unsigned col, unsigned *val)
{
    LibmsiStorageView *sv = (LibmsiStorageView *)view;

    TRACE("(%p, %d, %d, %p)\n", view, row, col, val);

    if (col != 1)
        return ERROR_INVALID_PARAMETER;

    if (row >= sv->num_rows)
        return ERROR_NO_MORE_ITEMS;

    *val = sv->storages[row]->str_index;

    return ERROR_SUCCESS;
}

static unsigned STORAGES_fetch_stream(LibmsiView *view, unsigned row, unsigned col, IStream **stm)
{
    LibmsiStorageView *sv = (LibmsiStorageView *)view;

    TRACE("(%p, %d, %d, %p)\n", view, row, col, stm);

    if (row >= sv->num_rows)
        return ERROR_FUNCTION_FAILED;

    return ERROR_INVALID_DATA;
}

static unsigned STORAGES_get_row( LibmsiView *view, unsigned row, LibmsiRecord **rec )
{
    LibmsiStorageView *sv = (LibmsiStorageView *)view;

    FIXME("%p %d %p\n", sv, row, rec);

    return ERROR_CALL_NOT_IMPLEMENTED;
}

static HRESULT stream_to_storage(IStream *stm, IStorage **stg)
{
    ILockBytes *lockbytes = NULL;
    STATSTG stat;
    void *data;
    HRESULT hr;
    unsigned size, read;
    ULARGE_INTEGER offset;

    hr = IStream_Stat(stm, &stat, STATFLAG_NONAME);
    if (FAILED(hr))
        return hr;

    if (stat.cbSize.QuadPart >> 32)
    {
        ERR("Storage is too large\n");
        return E_FAIL;
    }

    size = stat.cbSize.QuadPart;
    data = msi_alloc(size);
    if (!data)
        return E_OUTOFMEMORY;

    hr = IStream_Read(stm, data, size, &read);
    if (FAILED(hr) || read != size)
        goto done;

    hr = CreateILockBytesOnHGlobal(NULL, true, &lockbytes);
    if (FAILED(hr))
        goto done;

    ZeroMemory(&offset, sizeof(ULARGE_INTEGER));
    hr = ILockBytes_WriteAt(lockbytes, offset, data, size, &read);
    if (FAILED(hr) || read != size)
        goto done;

    hr = StgOpenStorageOnILockBytes(lockbytes, NULL,
                                    STGM_READWRITE | STGM_SHARE_DENY_NONE,
                                    NULL, 0, stg);
    if (FAILED(hr))
        goto done;

done:
    msi_free(data);
    if (lockbytes) ILockBytes_Release(lockbytes);
    return hr;
}

static unsigned STORAGES_set_row(LibmsiView *view, unsigned row, LibmsiRecord *rec, unsigned mask)
{
    LibmsiStorageView *sv = (LibmsiStorageView *)view;
    IStorage *stg, *substg = NULL;
    IStream *stm;
    WCHAR *name = NULL;
    HRESULT hr;
    unsigned r = ERROR_FUNCTION_FAILED;

    TRACE("(%p, %p)\n", view, rec);

    if (row > sv->num_rows)
        return ERROR_FUNCTION_FAILED;

    r = MSI_RecordGetIStream(rec, 2, &stm);
    if (r != ERROR_SUCCESS)
        return r;

    r = stream_to_storage(stm, &stg);
    if (r != ERROR_SUCCESS)
    {
        IStream_Release(stm);
        return r;
    }

    name = strdupW(MSI_RecordGetStringRaw(rec, 1));
    if (!name)
    {
        r = ERROR_OUTOFMEMORY;
        goto done;
    }

    hr = IStorage_CreateStorage(sv->db->storage, name,
                                STGM_WRITE | STGM_SHARE_EXCLUSIVE,
                                0, 0, &substg);
    if (FAILED(hr))
    {
        r = ERROR_FUNCTION_FAILED;
        goto done;
    }

    hr = IStorage_CopyTo(stg, 0, NULL, NULL, substg);
    if (FAILED(hr))
    {
        r = ERROR_FUNCTION_FAILED;
        goto done;
    }

    sv->storages[row] = create_storage(sv, name, stg);
    if (!sv->storages[row])
        r = ERROR_FUNCTION_FAILED;

done:
    msi_free(name);

    if (substg) IStorage_Release(substg);
    IStorage_Release(stg);
    IStream_Release(stm);

    return r;
}

static unsigned STORAGES_insert_row(LibmsiView *view, LibmsiRecord *rec, unsigned row, bool temporary)
{
    LibmsiStorageView *sv = (LibmsiStorageView *)view;

    if (!storages_set_table_size(sv, ++sv->num_rows))
        return ERROR_FUNCTION_FAILED;

    if (row == -1)
        row = sv->num_rows - 1;

    /* FIXME have to readjust rows */

    return STORAGES_set_row(view, row, rec, 0);
}

static unsigned STORAGES_delete_row(LibmsiView *view, unsigned row)
{
    FIXME("(%p %d): stub!\n", view, row);
    return ERROR_SUCCESS;
}

static unsigned STORAGES_execute(LibmsiView *view, LibmsiRecord *record)
{
    TRACE("(%p, %p)\n", view, record);
    return ERROR_SUCCESS;
}

static unsigned STORAGES_close(LibmsiView *view)
{
    TRACE("(%p)\n", view);
    return ERROR_SUCCESS;
}

static unsigned STORAGES_get_dimensions(LibmsiView *view, unsigned *rows, unsigned *cols)
{
    LibmsiStorageView *sv = (LibmsiStorageView *)view;

    TRACE("(%p, %p, %p)\n", view, rows, cols);

    if (cols) *cols = NUM_STORAGES_COLS;
    if (rows) *rows = sv->num_rows;

    return ERROR_SUCCESS;
}

static unsigned STORAGES_get_column_info( LibmsiView *view, unsigned n, const WCHAR **name,
                                      unsigned *type, bool *temporary, const WCHAR **table_name )
{
    TRACE("(%p, %d, %p, %p, %p, %p)\n", view, n, name, type, temporary,
          table_name);

    if (n == 0 || n > NUM_STORAGES_COLS)
        return ERROR_INVALID_PARAMETER;

    switch (n)
    {
    case 1:
        if (name) *name = szName;
        if (type) *type = MSITYPE_STRING | MSITYPE_VALID | MAX_STORAGES_NAME_LEN;
        break;

    case 2:
        if (name) *name = szData;
        if (type) *type = MSITYPE_STRING | MSITYPE_VALID | MSITYPE_NULLABLE;
        break;
    }
    if (table_name) *table_name = szStorages;
    if (temporary) *temporary = false;
    return ERROR_SUCCESS;
}

static unsigned storages_find_row(LibmsiStorageView *sv, LibmsiRecord *rec, unsigned *row)
{
    const WCHAR *str;
    unsigned r, i, id, data;

    str = MSI_RecordGetStringRaw(rec, 1);
    r = msi_string2idW(sv->db->strings, str, &id);
    if (r != ERROR_SUCCESS)
        return r;

    for (i = 0; i < sv->num_rows; i++)
    {
        STORAGES_fetch_int(&sv->view, i, 1, &data);

        if (data == id)
        {
            *row = i;
            return ERROR_SUCCESS;
        }
    }

    return ERROR_FUNCTION_FAILED;
}

static unsigned storages_modify_update(LibmsiView *view, LibmsiRecord *rec)
{
    LibmsiStorageView *sv = (LibmsiStorageView *)view;
    unsigned r, row;

    r = storages_find_row(sv, rec, &row);
    if (r != ERROR_SUCCESS)
        return ERROR_FUNCTION_FAILED;

    return STORAGES_set_row(view, row, rec, 0);
}

static unsigned storages_modify_assign(LibmsiView *view, LibmsiRecord *rec)
{
    LibmsiStorageView *sv = (LibmsiStorageView *)view;
    unsigned r, row;

    r = storages_find_row(sv, rec, &row);
    if (r == ERROR_SUCCESS)
        return storages_modify_update(view, rec);

    return STORAGES_insert_row(view, rec, -1, false);
}

static unsigned STORAGES_modify(LibmsiView *view, LibmsiModify eModifyMode, LibmsiRecord *rec, unsigned row)
{
    unsigned r;

    TRACE("%p %d %p\n", view, eModifyMode, rec);

    switch (eModifyMode)
    {
    case LIBMSI_MODIFY_ASSIGN:
        r = storages_modify_assign(view, rec);
        break;

    case LIBMSI_MODIFY_INSERT:
        r = STORAGES_insert_row(view, rec, -1, false);
        break;

    case LIBMSI_MODIFY_UPDATE:
        r = storages_modify_update(view, rec);
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

static unsigned STORAGES_delete(LibmsiView *view)
{
    LibmsiStorageView *sv = (LibmsiStorageView *)view;
    unsigned i;

    TRACE("(%p)\n", view);

    for (i = 0; i < sv->num_rows; i++)
    {
        if (sv->storages[i]->storage)
            IStorage_Release(sv->storages[i]->storage);
        msi_free(sv->storages[i]);
    }

    msi_free(sv->storages);
    sv->storages = NULL;
    msi_free(sv);

    return ERROR_SUCCESS;
}

static unsigned STORAGES_find_matching_rows(LibmsiView *view, unsigned col,
                                       unsigned val, unsigned *row, MSIITERHANDLE *handle)
{
    LibmsiStorageView *sv = (LibmsiStorageView *)view;
    unsigned index = PtrToUlong(*handle);

    TRACE("(%d, %d): %d\n", *row, col, val);

    if (col == 0 || col > NUM_STORAGES_COLS)
        return ERROR_INVALID_PARAMETER;

    while (index < sv->num_rows)
    {
        if (sv->storages[index]->str_index == val)
        {
            *row = index;
            break;
        }

        index++;
    }

    *handle = UlongToPtr(++index);
    if (index >= sv->num_rows)
        return ERROR_NO_MORE_ITEMS;

    return ERROR_SUCCESS;
}

static const LibmsiViewOPS storages_ops =
{
    STORAGES_fetch_int,
    STORAGES_fetch_stream,
    STORAGES_get_row,
    STORAGES_set_row,
    STORAGES_insert_row,
    STORAGES_delete_row,
    STORAGES_execute,
    STORAGES_close,
    STORAGES_get_dimensions,
    STORAGES_get_column_info,
    STORAGES_modify,
    STORAGES_delete,
    STORAGES_find_matching_rows,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static int add_storages_to_table(LibmsiStorageView *sv)
{
    STORAGE *storage = NULL;
    IEnumSTATSTG *stgenum = NULL;
    STATSTG stat;
    HRESULT hr;
    unsigned count = 0, size;

    hr = IStorage_EnumElements(sv->db->storage, 0, NULL, 0, &stgenum);
    if (FAILED(hr))
        return -1;

    sv->max_storages = 1;
    sv->storages = msi_alloc(sizeof(STORAGE *));
    if (!sv->storages)
        return -1;

    while (true)
    {
        size = 0;
        hr = IEnumSTATSTG_Next(stgenum, 1, &stat, &size);
        if (FAILED(hr) || !size)
            break;

        if (stat.type != STGTY_STORAGE)
        {
            CoTaskMemFree(stat.pwcsName);
            continue;
        }

        TRACE("enumerated storage %s\n", debugstr_w(stat.pwcsName));

        storage = create_storage(sv, stat.pwcsName, NULL);
        if (!storage)
        {
            count = -1;
            CoTaskMemFree(stat.pwcsName);
            break;
        }

        IStorage_OpenStorage(sv->db->storage, stat.pwcsName, NULL,
                             STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0,
                             &storage->storage);
        CoTaskMemFree(stat.pwcsName);

        if (!storages_set_table_size(sv, ++count))
        {
            count = -1;
            break;
        }

        sv->storages[count - 1] = storage;
    }

    IEnumSTATSTG_Release(stgenum);
    return count;
}

unsigned STORAGES_CreateView(LibmsiDatabase *db, LibmsiView **view)
{
    LibmsiStorageView *sv;
    int rows;

    TRACE("(%p, %p)\n", db, view);

    sv = alloc_msiobject( sizeof(LibmsiStorageView), NULL );
    if (!sv)
        return ERROR_FUNCTION_FAILED;

    sv->view.ops = &storages_ops;
    sv->db = db;

    rows = add_storages_to_table(sv);
    if (rows < 0)
    {
        msi_free( sv );
        return ERROR_FUNCTION_FAILED;
    }
    sv->num_rows = rows;

    *view = (LibmsiView *)sv;

    return ERROR_SUCCESS;
}
