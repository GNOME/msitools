/*
 * winemsibuilder - tool to build MSI packages
 *
 * Copyright 2010 Hans Leidekker for CodeWeavers
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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libmsi.h>
#include <limits.h>

#include "sqldelim.h"

static gboolean init_suminfo(LibmsiSummaryInfo *si, GError **error)
{
    g_autofree char *uustr = NULL;
    g_autoptr(GString) str = g_string_new("");

    if (!libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_TITLE,
                                        "Installation Database", error))
        return FALSE;

    if (!libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_KEYWORDS,
                                        "Installer, MSI", error))
        return FALSE;

    if (!libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_TEMPLATE,
                                        ";1033", error))
        return FALSE;

    if (!libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_APPNAME,
                                        "libmsi msibuild", error))
        return FALSE;

    if (!libmsi_summary_info_set_int(si, LIBMSI_PROPERTY_VERSION,
                                     200, error))
        return FALSE;

    if (!libmsi_summary_info_set_int(si, LIBMSI_PROPERTY_SOURCE,
                                     0, error))
        return FALSE;

    if (!libmsi_summary_info_set_int(si, LIBMSI_PROPERTY_RESTRICT,
                                     0, error))
        return FALSE;

    uustr = g_uuid_string_random();
    g_string_printf(str, "{%s}", uustr);
    g_string_ascii_up(str);

    if (!libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_UUID,
                                        str->str, error))
        return FALSE;

    return TRUE;
}

static gboolean open_database(const char *msifile, LibmsiDatabase **db,
                              GError **error)
{
    LibmsiSummaryInfo *si = NULL;
    gboolean success = FALSE;
    struct stat st;

    if (stat(msifile, &st) == -1)
    {
        *db = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_CREATE, NULL, error);
        if (!*db)
            goto end;

        si = libmsi_summary_info_new(*db, INT_MAX, error);
        if (!si)
        {
            fprintf(stderr, "failed to open summary info\n");
            goto end;
        }

        if (!init_suminfo(si, error))
            goto end;

        if (!libmsi_summary_info_persist(si, error))
            goto end;

        if (!libmsi_database_commit(*db, error))
        {
            fprintf(stderr, "failed to commit database\n");
            g_object_unref(*db);
            goto end;
        }
    }
    else
    {
        *db = libmsi_database_new(msifile, LIBMSI_DB_FLAGS_TRANSACT, NULL, error);
        if (!*db)
            goto end;
    }

    success = TRUE;

end:
    if (si)
        g_object_unref(si);

    return success;
}

static LibmsiDatabase *db;

static gboolean import_table(char *table, GError **error)
{
    gboolean success = TRUE;

    if (!libmsi_database_import(db, table, error))
    {
        fprintf(stderr, "failed to import table %s\n", table);
        success = FALSE;
    }

    return success;
}

static gboolean add_summary_info(const char *name, const char *author,
                                 const char *template, const char *uuid,
                                 GError **error)
{
    LibmsiSummaryInfo *si = libmsi_summary_info_new(db, INT_MAX, error);
    gboolean success = FALSE;

    if (!si)
        return FALSE;

    if (!libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_SUBJECT, name, error))
        goto end;

    if (author &&
        !libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_AUTHOR, author, error))
        goto end;

    if (template &&
        !libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_TEMPLATE, template, error))
        goto end;

    if (uuid &&
        !libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_UUID, uuid, error))
        goto end;

    if (!libmsi_summary_info_persist(si, error))
        goto end;

    success = TRUE;

end:
    if (si)
        g_object_unref(si);

    return success;
}

static gboolean add_stream(const char *stream, const char *file, GError **error)
{
    gboolean r = FALSE;
    LibmsiRecord *rec = NULL;
    LibmsiQuery *query = NULL;

    rec = libmsi_record_new(2);
    libmsi_record_set_string(rec, 1, stream);
    if (!libmsi_record_load_stream(rec, 2, file)) {
        fprintf(stderr, "failed to load stream (%u)\n", r);
        goto end;
    }

    query = libmsi_query_new(db,
            "INSERT INTO `_Streams` (`Name`, `Data`) VALUES (?, ?)", error);
    if (!query)
        goto end;

    if (!libmsi_query_execute(query, rec, error)) {
        fprintf(stderr, "failed to execute query\n");
        goto end;
    }

    r = TRUE;

end:
    if (rec)
        g_object_unref(rec);
    if (query) {
        libmsi_query_close(query, error);
        g_object_unref(query);
    }

    return r;
}

static int do_query(const char *sql, void *opaque)
{
    GError **error = opaque;
    gboolean success = FALSE;
    LibmsiQuery *query;

    query = libmsi_query_new(db, sql, error);
    if (!query) {
        fprintf(stderr, "failed to open query\n");
        goto end;
    }

    if (!libmsi_query_execute(query, NULL, error)) {
        fprintf(stderr, "failed to execute query\n");
        goto end;
    }

    success = TRUE;

end:
    if (query) {
        libmsi_query_close(query, error);
        g_object_unref(query);
    }

    return !success;
}

static void show_usage(void)
{
    printf(
        "Usage: msibuild MSIFILE [OPTION]...\n"
        "Options:\n"
        "  -s name [author] [template] [uuid] Set summary information.\n"
        "  -q query         Execute SQL query/queries.\n"
        "  -i table1.idt    Import one table into the database.\n"
        "  -a stream file   Add 'stream' to storage with contents of 'file'.\n"
        "\nExisting tables or streams will be overwritten. If package.msi does not exist a new file\n"
        "will be created with an empty database.\n"
  );
}

int main(int argc, char *argv[])
{
    GError *error = NULL;
    gboolean success = FALSE;
    int n;

#if !GLIB_CHECK_VERSION(2,35,1)
    g_type_init ();
#endif

    if (argc <= 2 )
    {
        show_usage();
        return 1;
    }

    /* Accept package after first option for winemsibuilder compatibility.  */
    if (argc >= 3 && argv[1][0] == '-') {
        success = open_database(argv[2], &db, &error);
        argv[2] = argv[1];
    } else {
        success = open_database(argv[1], &db, &error);
    }
    if (!success) return 1;

    argc -= 2, argv += 2;
    while (argc > 0) {
        int ret;
        if (argc < 2 || argv[0][0] != '-' || argv[0][2])
        {
            show_usage();
            return 1;
        }

        switch (argv[0][1])
        {
        case 's':
            n = 2;
            if (argv[2] && argv[2][0] != '-') n++;
            if (n > 2 && argv[3] && argv[3][0] != '-') n++;
            if (n > 3 && argv[4] && argv[4][0] != '-') n++;
            if (!add_summary_info(argv[1],
                                  n > 2 ? argv[2] : NULL,
                                  n > 3 ? argv[3] : NULL,
                                  n > 4 ? argv[4] : NULL, &error))
                goto end;
            argc -= n + 1, argv += n + 1;
            break;
        case 'i':
            do {
                if (!import_table(argv[1], &error))
                    break;

                argc--, argv++;
            } while (argv[1] && argv[1][0] != '-');
            argc--, argv++;
            break;
        case 'q':
            do {
                ret = sql_get_statement(argv[1], do_query, &error);
                if (ret == 0) {
                    ret = sql_get_statement("", do_query, &error);
                }
                if (ret) {
                    break;
                }
                argc--, argv++;
            } while (argv[1] && argv[1][0] != '-');
            argc--, argv++;
            break;
        case 'a':
            if (argc < 3) break;
            if (!add_stream(argv[1], argv[2], &error))
                goto end;
            argc -= 3, argv += 3;
            break;
        default:
            fprintf(stdout, "unknown option\n");
            show_usage();
            break;
        }
    }

    if (!libmsi_database_commit(db, &error))
        goto end;

end:
    g_object_unref(db);

    if (error != NULL) {
        g_printerr("error: %s\n", error->message);
        g_clear_error(&error);
        exit(1);
    }

    return 0;
}
