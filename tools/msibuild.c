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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libmsi.h>

LibmsiDatabase *db;

LibmsiResult open_database(const char *msifile, LibmsiDatabase **db)
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
        r = libmsi_database_commit(*db);
        if (r != LIBMSI_RESULT_SUCCESS)
        {
            fprintf(stderr, "failed to commit database (%u)\n", r);
            libmsi_unref(*db);
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
    }

    return r;
}

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

static int add_stream(const char *stream, const char *file)
{
    LibmsiResult r;
    LibmsiRecord *rec;
    LibmsiQuery *query;

    rec = libmsi_record_create(2);
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

    libmsi_unref(rec);
    libmsi_query_close(query);
    libmsi_unref(query);
    return 0;
}

static void show_usage(void)
{
    printf(
        "Usage: msibuild MSIFILE [OPTION]...\n"
        "Options:\n"
        "  -i table1.idt    Import one table into the database.\n"
        "  -a stream file   Add 'stream' to storage with contents of 'file'.\n"
        "\nExisting tables or streams will be overwritten. If package.msi does not exist a new file\n"
        "will be created with an empty database.\n"
  );
}

int main(int argc, char *argv[])
{
    int r;

    if (argc <= 2 )
    {
        show_usage();
        return 1;
    }

    /* Accept package after first option for winemsibuilder compatibility.  */
    if (argc >= 3 && argv[1][0] == '-') {
        r = open_database(argv[2], &db);
        argv[2] = argv[1];
    } else {
        r = open_database(argv[1], &db);
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
        case 'i':
            do {
                ret = import_table(argv[1]);
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
        r = libmsi_database_commit(db);
        if (r != LIBMSI_RESULT_SUCCESS)
            fprintf(stderr, "failed to commit database (%u)\n", r);
    }
    libmsi_unref(db);
    return r != LIBMSI_RESULT_SUCCESS;
}
