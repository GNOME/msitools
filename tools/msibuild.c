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

#ifdef HAVE_LIBUUID
#include <uuid.h>
#endif

#include "sqldelim.h"

static void init_suminfo(LibmsiSummaryInfo *si)
{
    libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_TITLE,
                                     LIBMSI_PROPERTY_TYPE_STRING, 0, NULL,
                                     "Installation Database");
    libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_KEYWORDS,
                                     LIBMSI_PROPERTY_TYPE_STRING, 0, NULL,
                                     "Installer, MSI");
    libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_TEMPLATE,
                                     LIBMSI_PROPERTY_TYPE_STRING, 0, NULL,
                                     ";1033");
    libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_APPNAME,
                                     LIBMSI_PROPERTY_TYPE_STRING, 0, NULL,
                                     "libmsi msibuild");
    libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_VERSION,
                                     LIBMSI_PROPERTY_TYPE_INT, 200, NULL, NULL);
    libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_SOURCE,
                                     LIBMSI_PROPERTY_TYPE_INT, 0, NULL, NULL);
    libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_RESTRICT,
                                     LIBMSI_PROPERTY_TYPE_INT, 0, NULL, NULL);

#ifdef HAVE_LIBUUID
    {
        uuid_t uu;
        char uustr[40];
        uuid_generate(uu);
        uustr[0] = '{';
        uuid_unparse_upper(uu, uustr + 1);
        strcat(uustr, "}");
        libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_UUID,
                                         LIBMSI_PROPERTY_TYPE_STRING, 0, NULL, uustr);
    }
#endif
}

static LibmsiResult open_database(const char *msifile, LibmsiDatabase **db,
                                  LibmsiSummaryInfo **si)
{
    LibmsiResult r;
    struct stat st;

    if (stat(msifile, &st) == -1)
    {
        r = libmsi_database_open(msifile, LIBMSI_DB_OPEN_CREATE, db);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            fprintf(stderr, "failed to create package database %s (%u)\n", msifile, r);
            return r;
        }

        r = libmsi_database_get_summary_info(*db, INT_MAX, si);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            fprintf(stderr, "failed to open summary info (%u)\n", r);
            return r;
        }

        init_suminfo(*si);

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
        r = libmsi_database_open(msifile, LIBMSI_DB_OPEN_TRANSACT, db);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            fprintf(stderr, "failed to open package database %s (%u)\n", msifile, r);
            return r;
        }

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

static int add_summary_info(const char *name, const char *author,
                            const char *template, const char *uuid)
{
    libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_SUBJECT,
                                     LIBMSI_PROPERTY_TYPE_STRING,
                                     0, NULL, name);
    if (author) {
        libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_AUTHOR,
                                         LIBMSI_PROPERTY_TYPE_STRING,
                                         0, NULL, author);
    }
    if (template) {
        libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_TEMPLATE,
                                         LIBMSI_PROPERTY_TYPE_STRING,
                                         0, NULL, template);
    }
    if (uuid) {
        libmsi_summary_info_set_property(si, LIBMSI_PROPERTY_UUID,
                                         LIBMSI_PROPERTY_TYPE_STRING,
                                         0, NULL, uuid);
    }
}

static int add_stream(const char *stream, const char *file)
{
    LibmsiResult r;
    LibmsiRecord *rec;
    LibmsiQuery *query;

    rec = libmsi_record_new(2);
    libmsi_record_set_string(rec, 1, stream);
    r = libmsi_record_load_stream(rec, 2, file);
    if (r != LIBMSI_RESULT_SUCCESS)
        fprintf(stderr, "failed to load stream (%u)\n", r);

    r = libmsi_database_open_query(db,
            "INSERT INTO `_Streams` (`Name`, `Data`) VALUES (?, ?)", &query);
    if (r != LIBMSI_RESULT_SUCCESS)
        fprintf(stderr, "failed to open query (%u)\n", r);

    r = libmsi_query_execute(query, rec);
    if (r != LIBMSI_RESULT_SUCCESS)
        fprintf(stderr, "failed to execute query (%u)\n", r);

    g_object_unref(rec);
    libmsi_query_close(query);
    g_object_unref(query);
    return r;
}

static int do_query(const char *sql, void *opaque)
{
    LibmsiResult r;
    LibmsiQuery *query;

    r = libmsi_database_open_query(db, sql, &query);
    if (r != LIBMSI_RESULT_SUCCESS) {
        fprintf(stderr, "failed to open query (%u)\n", r);
        return r;
    }

    r = libmsi_query_execute(query, NULL);
    if (r != LIBMSI_RESULT_SUCCESS)
        fprintf(stderr, "failed to execute query (%u)\n", r);

    libmsi_query_close(query);
    g_object_unref(query);
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
        r = open_database(argv[2], &db, &si);
        argv[2] = argv[1];
    } else {
        r = open_database(argv[1], &db, &si);
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
            ret = add_summary_info(argv[1],
                                   n > 2 ? argv[2] : NULL,
                                   n > 3 ? argv[3] : NULL,
                                   n > 4 ? argv[4] : NULL);
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
                ret = sql_get_statement(argv[1], do_query, NULL);
                if (ret == 0) {
                    ret = sql_get_statement("", do_query, NULL);
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
            ret = add_stream(argv[1], argv[2]);
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
        r = libmsi_summary_info_persist(si);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            fprintf(stderr, "failed to commit summary info (%u)\n", r);
            exit(1);
        }
        r = libmsi_database_commit(db);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            fprintf(stderr, "failed to commit database (%u)\n", r);
            exit(1);
        }
    }
    g_object_unref(si);
    g_object_unref(db);
    return r != LIBMSI_RESULT_SUCCESS;
}
