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
#include <unistd.h>
#include <sys/stat.h>
#include <libmsi.h>
#include <limits.h>

#ifdef HAVE_LIBUUID
#include <uuid.h>
#endif

#include "sqldelim.h"

static gboolean init_suminfo(LibmsiSummaryInfo *si, GError **error)
{
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

#ifdef HAVE_LIBUUID
    {
        uuid_t uu;
        char uustr[40];
        uuid_generate(uu);
        uustr[0] = '{';
        uuid_unparse_upper(uu, uustr + 1);
        strcat(uustr, "}");
        if (!libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_UUID,
                                            uustr, error))
            return FALSE;
    }
#endif

    return TRUE;
}

static LibmsiResult open_database(const char *msifile, LibmsiDatabase **db,
                                  LibmsiSummaryInfo **si, GError **error)
{
    LibmsiResult r;
    struct stat st;

    if (stat(msifile, &st) == -1)
    {
        *db = libmsi_database_new(msifile, LIBMSI_DB_OPEN_CREATE, error);
        if (!*db)
            return LIBMSI_RESULT_FUNCTION_FAILED;

        r = libmsi_database_get_summary_info(*db, INT_MAX, si);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            fprintf(stderr, "failed to open summary info (%u)\n", r);
            return r;
        }

        if (!init_suminfo(*si, error))
            return LIBMSI_RESULT_FUNCTION_FAILED;

        r = libmsi_database_commit(*db);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            fprintf(stderr, "failed to commit database (%u)\n", r);
            g_object_unref(*db);
            return r;
        }
    }
    else
    {
        *db = libmsi_database_new(msifile, LIBMSI_DB_OPEN_TRANSACT, error);
        if (!*db)
            return LIBMSI_RESULT_FUNCTION_FAILED;

        r = libmsi_database_get_summary_info(*db, INT_MAX, si);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            fprintf(stderr, "failed to open summary info (%u)\n", r);
            return r;
        }
    }

    return r;
}

static LibmsiDatabase *db;
static LibmsiSummaryInfo *si;

static int import_table(char *table)
{
    LibmsiResult r;
    char dir[PATH_MAX];

    if (getcwd(dir, PATH_MAX) == NULL)
        return 1;

    r = libmsi_database_import(db, dir, table);
    if (r != LIBMSI_RESULT_SUCCESS)
    {
        fprintf(stderr, "failed to import table %s (%u)\n", table, r);
    }

    return (r != LIBMSI_RESULT_SUCCESS);
}

static gboolean add_summary_info(const char *name, const char *author,
                                 const char *template, const char *uuid,
                                 GError **error)
{
    if (!libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_SUBJECT, name, error))
        return FALSE;

    if (author &&
        !libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_AUTHOR, author, error))
        return FALSE;

    if (template &&
        !libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_TEMPLATE, template, error))
        return FALSE;

    if (uuid &&
        !libmsi_summary_info_set_string(si, LIBMSI_PROPERTY_UUID, uuid, error))
        return FALSE;

    return TRUE;
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
    LibmsiResult r = LIBMSI_RESULT_FUNCTION_FAILED;
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

    r = LIBMSI_RESULT_SUCCESS;

end:
    if (query) {
        libmsi_query_close(query, error);
        g_object_unref(query);
    }

    return r;
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
    int r;
    int n;

    g_type_init();
    if (argc <= 2 )
    {
        show_usage();
        return 1;
    }

    /* Accept package after first option for winemsibuilder compatibility.  */
    if (argc >= 3 && argv[1][0] == '-') {
        r = open_database(argv[2], &db, &si, &error);
        argv[2] = argv[1];
    } else {
        r = open_database(argv[1], &db, &si, &error);
    }
    if (r != LIBMSI_RESULT_SUCCESS) return 1;

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
            argc -= 3, argv += 3;
            break;
        case 'i':
            do {
                ret = import_table(argv[1]);
                if (ret) {
                    break;
                }
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
        if (r != LIBMSI_RESULT_SUCCESS) {
            break;
        }
    }

    if (r == LIBMSI_RESULT_SUCCESS) {
        libmsi_summary_info_persist(si, &error);
        if (error)
            goto end;

        r = libmsi_database_commit(db);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            fprintf(stderr, "failed to commit database (%u)\n", r);
            exit(1);
        }
    }

end:
    g_object_unref(si);
    g_object_unref(db);

    if (error != NULL) {
        g_printerr("error: %s\n", error->message);
        g_clear_error(&error);
        exit(1);
    }

    return r != LIBMSI_RESULT_SUCCESS;
}
