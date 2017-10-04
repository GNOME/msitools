/*
 * msiinfo - MSI inspection tool
 *
 * Copyright 2012 Red Hat, Inc.
 *
 * Author: Paolo Bonzini <pbonzini@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "config.h"
#include "libmsi.h"

#include <glib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

struct Command {
    const char *cmd;
    const char *desc;
    const char *usage;
    const char *opts;
    const char *help;
    int (*func)(struct Command *cmd, int argc, char **argv, GError **error);
};

static struct Command cmds[];

static void usage(FILE *out)
{
    int i;

    fprintf(out, "Usage: %s SUBCOMMAND COMMAND-OPTIONS...\n\n", g_get_prgname ());
    fprintf(out, "Options:\n");
    fprintf(out, "  -h, --help        Show program usage\n");
    fprintf(out, "  -v, --version     Display program version\n\n");
    fprintf(out, "Available subcommands:\n");
    for (i = 0; cmds[i].cmd; i++) {
        if (cmds[i].desc) {
            fprintf(out, "  %-18s%s\n", cmds[i].cmd, cmds[i].desc);
        }
    }
    exit (out == stderr);
}

static void cmd_usage(FILE *out, struct Command *cmd)
{
    fprintf(out, "%s %s %s\n\n%s.\n", g_get_prgname (), cmd->cmd, cmd->opts,
            cmd->desc);

    if (cmd->help) {
        fprintf(out, "\n%s\n", cmd->help);
    }
    exit (out == stderr);
}

static void print_libmsi_error(LibmsiResultError r)
{
    switch (r)
    {
    case LIBMSI_RESULT_SUCCESS:
        abort();
    case LIBMSI_RESULT_CONTINUE:
        fprintf(stderr, "%s: internal error (continue)\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_MORE_DATA:
        fprintf(stderr, "%s: internal error (more data)\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_INVALID_HANDLE:
        fprintf(stderr, "%s: internal error (invalid handle)\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_NOT_ENOUGH_MEMORY:
    case LIBMSI_RESULT_OUTOFMEMORY:
        fprintf(stderr, "%s: out of memory\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_INVALID_DATA:
        fprintf(stderr, "%s: invalid data\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_INVALID_PARAMETER:
        fprintf(stderr, "%s: invalid parameter\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_OPEN_FAILED:
        fprintf(stderr, "%s: open failed\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_CALL_NOT_IMPLEMENTED:
        fprintf(stderr, "%s: not implemented\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_NOT_FOUND:
        fprintf(stderr, "%s: not found\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_UNKNOWN_PROPERTY:
        fprintf(stderr, "%s: unknown property\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_BAD_QUERY_SYNTAX:
        fprintf(stderr, "%s: bad query syntax\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_INVALID_FIELD:
        fprintf(stderr, "%s: invalid field\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_FUNCTION_FAILED:
        fprintf(stderr, "%s: internal error (function failed)\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_INVALID_TABLE:
        fprintf(stderr, "%s: invalid table\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_DATATYPE_MISMATCH:
        fprintf(stderr, "%s: datatype mismatch\n", g_get_prgname ());
        exit(1);
    case LIBMSI_RESULT_INVALID_DATATYPE:
        fprintf(stderr, "%s: invalid datatype\n", g_get_prgname ());
        exit(1);
    default:
        g_warn_if_reached ();
    }
}

static struct Command *find_cmd(const char *s)
{
    int i;

    for (i = 0; cmds[i].cmd; i++) {
        if (!strcmp(s, cmds[i].cmd)) {
            return &cmds[i];
        }
    }

    fprintf(stderr, "%s: Unrecognized command '%s'\n", g_get_prgname (), s);
    return NULL;
}

static void print_strings_from_query(LibmsiQuery *query, GError **error)
{
    GError *err = NULL;
    LibmsiRecord *rec = NULL;
    gchar *name;

    while ((rec = libmsi_query_fetch(query, &err))) {
        name = libmsi_record_get_string(rec, 1);
        g_return_if_fail(name != NULL);

        puts(name);

        g_free(name);
        g_object_unref(rec);
    }

    if (err)
        g_propagate_error(error, err);

    g_clear_error(&err);
}

static int cmd_streams(struct Command *cmd, int argc, char **argv, GError **error)
{
    LibmsiDatabase *db = NULL;
    LibmsiQuery *query = NULL;
    int r = 1;

    if (argc != 2) {
        cmd_usage(stderr, cmd);
    }

    db = libmsi_database_new(argv[1], LIBMSI_DB_FLAGS_READONLY, NULL, error);
    if (!db)
        goto end;

    query = libmsi_query_new(db, "SELECT `Name` FROM `_Streams`", error);
    if (!query)
        goto end;

    libmsi_query_execute(query, NULL, error);
    if (*error)
        goto end;

    print_strings_from_query(query, error);
    r = 0;

end:
    if (query)
        g_object_unref(query);
    if (db)
        g_object_unref(db);

    return r;
}

static int cmd_tables(struct Command *cmd, int argc, char **argv, GError **error)
{
    LibmsiDatabase *db = NULL;
    LibmsiQuery *query = NULL;
    int r = 1;

    if (argc != 2) {
        cmd_usage(stderr, cmd);
    }

    db = libmsi_database_new(argv[1], LIBMSI_DB_FLAGS_READONLY, NULL, error);
    if (!db)
        goto end;

    query = libmsi_query_new(db, "SELECT `Name` FROM `_Tables`", error);
    if (!query)
        goto end;

    r = libmsi_query_execute(query, NULL, error);
    if (*error)
        goto end;

    puts("_SummaryInformation");
    puts("_ForceCodepage");
    print_strings_from_query(query, error);

    r = 0;

end:
    if (query)
        g_object_unref(query);
    if (db)
        g_object_unref(db);

    return r;
}

static void print_suminfo(LibmsiSummaryInfo *si, int prop, const char *name)
{
    GError *error = NULL;
    unsigned type;
    const gchar* str;
    int val;
    uint64_t valtime;
    time_t t;

    type = libmsi_summary_info_get_property_type(si, prop, &error);
    if (error)
        goto end;

    switch (type) {
    case LIBMSI_PROPERTY_TYPE_INT:
        val = libmsi_summary_info_get_int(si, prop, &error);
        if (error)
            goto end;
        printf ("%s: %d (%x)\n", name, val, val);
        break;

    case LIBMSI_PROPERTY_TYPE_STRING:
        str = libmsi_summary_info_get_string(si, prop, &error);
        if (error)
            goto end;
        printf ("%s: %s\n", name, str);
        break;

    case LIBMSI_PROPERTY_TYPE_FILETIME:
        valtime = libmsi_summary_info_get_filetime(si, prop, &error);
        if (error)
            goto end;
        /* Convert nanoseconds since 1601 to seconds since Unix epoch.  */
        t = (valtime / 10000000) - (uint64_t) 134774 * 86400;
        printf ("%s: %s", name, ctime(&t));
        break;

    case LIBMSI_PROPERTY_TYPE_EMPTY:
	break;

    default:
	abort();
    }

end:
    if (error)
        g_warning("Can't print summary info: %s", error->message);
    g_clear_error(&error);
}

static int cmd_suminfo(struct Command *cmd, int argc, char **argv, GError **error)
{
    LibmsiDatabase *db = NULL;
    LibmsiSummaryInfo *si = NULL;

    if (argc != 2) {
        cmd_usage(stderr, cmd);
    }

    db = libmsi_database_new(argv[1], LIBMSI_DB_FLAGS_READONLY, NULL, error);
    if (!db)
        goto end;

    si = libmsi_summary_info_new(db, 0, error);
    if (!si)
        goto end;

    print_suminfo(si, LIBMSI_PROPERTY_TITLE, "Title");
    print_suminfo(si, LIBMSI_PROPERTY_SUBJECT, "Subject");
    print_suminfo(si, LIBMSI_PROPERTY_AUTHOR, "Author");
    print_suminfo(si, LIBMSI_PROPERTY_KEYWORDS, "Keywords");
    print_suminfo(si, LIBMSI_PROPERTY_COMMENTS, "Comments");
    print_suminfo(si, LIBMSI_PROPERTY_TEMPLATE, "Template");
    print_suminfo(si, LIBMSI_PROPERTY_LASTAUTHOR, "Last author");
    print_suminfo(si, LIBMSI_PROPERTY_UUID, "Revision number (UUID)");
    print_suminfo(si, LIBMSI_PROPERTY_EDITTIME, "Edittime");
    print_suminfo(si, LIBMSI_PROPERTY_LASTPRINTED, "Last printed");
    print_suminfo(si, LIBMSI_PROPERTY_CREATED_TM, "Created");
    print_suminfo(si, LIBMSI_PROPERTY_LASTSAVED_TM, "Last saved");
    print_suminfo(si, LIBMSI_PROPERTY_VERSION, "Version");
    print_suminfo(si, LIBMSI_PROPERTY_SOURCE, "Source");
    print_suminfo(si, LIBMSI_PROPERTY_RESTRICT, "Restrict");
    print_suminfo(si, LIBMSI_PROPERTY_THUMBNAIL, "Thumbnail");
    print_suminfo(si, LIBMSI_PROPERTY_APPNAME, "Application");
    print_suminfo(si, LIBMSI_PROPERTY_SECURITY, "Security");

 end:
    if (db)
        g_object_unref(db);
    if (si)
        g_object_unref(si);

    return *error ? 1 : 0;
}

static void full_write(int fd, char *buf, size_t sz)
{
    while (sz > 0) {
        ssize_t rc = write(fd, buf, sz);
        if (rc < 0) {
            perror("write");
            exit(1);
        }
        buf += rc;
        sz -= rc;
    }
}

static int cmd_extract(struct Command *cmd, int argc, char **argv, GError **error)
{
    LibmsiDatabase *db = NULL;
    LibmsiQuery *query = NULL;
    LibmsiRecord *rec = NULL;
    GInputStream *in = NULL;
    int r = 1;
    char buffer[4096];
    size_t n_read;

    if (argc != 3) {
        cmd_usage(stderr, cmd);
    }

    db = libmsi_database_new(argv[1], LIBMSI_DB_FLAGS_READONLY, NULL, error);
    if (!db)
        goto end;

    query = libmsi_query_new(db, "SELECT `Data` FROM `_Streams` WHERE `Name` = ?", error);
    if (*error)
        goto end;

    rec = libmsi_record_new(1);
    libmsi_record_set_string(rec, 1, argv[2]);
    r = libmsi_query_execute(query, rec, error);
    if (*error)
        goto end;
    g_object_unref(rec);

    rec = libmsi_query_fetch(query, error);
    if (*error)
        goto end;

#if O_BINARY
    _setmode(STDOUT_FILENO, O_BINARY);
#endif

    in = G_INPUT_STREAM (libmsi_record_get_stream(rec, 1));
    for (;;) {
        n_read = g_input_stream_read (in, buffer, sizeof (buffer), NULL, error);
        if (n_read == -1)
            goto end;
        if (n_read == 0)
            break;

	full_write (STDOUT_FILENO, buffer, n_read);
    }

    if (!*error)
        r = 0;

end:
    if (in)
        g_object_unref(in);
    if (rec)
        g_object_unref(rec);
    if (query)
        g_object_unref(query);
    if (db)
        g_object_unref(db);

    return r;
}

static gboolean export_create_table(const char *table,
                                    LibmsiRecord *names,
                                    LibmsiRecord *types,
                                    LibmsiRecord *keys)
{
    guint num_columns = libmsi_record_get_field_count(names);
    guint num_keys = libmsi_record_get_field_count(keys);
    guint i, len;
    char extra[30];
    gchar *name, *type, *typesql;

    if (!strcmp(table, "_Tables") ||
        !strcmp(table, "_Columns") ||
        !strcmp(table, "_Streams") ||
        !strcmp(table, "_Storages")) {
        return 0;
    }

    printf("CREATE TABLE `%s` (", table);
    for (i = 1; i <= num_columns; i++)
    {
        name = libmsi_record_get_string(names, i);
        g_return_val_if_fail(name != NULL, FALSE);

        type = libmsi_record_get_string(types, i);
        g_return_val_if_fail(type != NULL, FALSE);

        if (i > 1) {
            printf(", ");
        }

        extra[0] = '\0';
        if (g_ascii_islower(type[0])) {
            strcat(extra, " NOT NULL");
        }

        switch (type[0])
        {
            case 'l': case 'L':
                strcat(extra, " LOCALIZABLE");
                /* fall through */
            case 's': case 'S':
                typesql = g_strdup_printf("CHAR(%s)", type+1);
                break;
            case 'i': case 'I':
                len = atol(type + 1);
                if (len <= 2)
                    typesql = g_strdup("INT");
                else if (len == 4)
                    typesql = g_strdup("LONG");
                else
                    abort();
                break;
            case 'v': case 'V':
                typesql = g_strdup("OBJECT");
                break;
            default:
                abort();
        }

        printf("`%s` %s%s", name, typesql, extra);
        g_free(typesql);
        g_free(name);
        g_free(type);
    }

    for (i = 1; i <= num_keys; i++)
    {
        name = libmsi_record_get_string(names, i);
        g_return_val_if_fail(name != NULL, FALSE);

        printf("%s `%s`", (i > 1 ? "," : " PRIMARY KEY"), name);
        g_free(name);
    }
    printf(")\n");
    return TRUE;
}

static void print_quoted_string(const char *s)
{
    putchar('\'');
    for (; *s; s++) {
        switch (*s) {
        case '\n': putchar('\\'); putchar('n'); break;
        case '\r': putchar('\\'); putchar('r'); break;
        case '\\': putchar('\\'); putchar('\\'); break;
        case '\'': putchar('\\'); putchar('\''); break;
        default: putchar(*s);
        }
    }
    putchar('\'');
}

static gboolean export_insert(const char *table,
                              LibmsiRecord *names,
                              LibmsiRecord *types,
                              LibmsiRecord *vals)
{
    guint num_columns = libmsi_record_get_field_count(names);
    gchar *name, *type;
    guint i;
    char *s;

    printf("INSERT INTO `%s` (", table);
    for (i = 1; i <= num_columns; i++)
    {
        if (libmsi_record_is_null(vals, i)) {
            continue;
        }

        name = libmsi_record_get_string(names, i);
        g_return_val_if_fail(name != NULL, FALSE);

        if (i > 1) {
            printf(", ");
        }

        printf("`%s`", name);
        g_free(name);
    }
    printf(") VALUES (");
    for (i = 1; i <= num_columns; i++)
    {
        if (libmsi_record_is_null(vals, i)) {
            continue;
        }

        if (i > 1) {
            printf(", ");
        }

        type = libmsi_record_get_string(types, i);
        g_return_val_if_fail(type != NULL, FALSE);

        switch (type[0])
        {
            case 'l': case 'L':
            case 's': case 'S':
                s = libmsi_record_get_string(vals, i);
                print_quoted_string(s);
                g_free(s);
                break;

            case 'i': case 'I':
                printf("%d", libmsi_record_get_int(vals, i));
                break;
            case 'v': case 'V':
                printf("''");
                break;
            default:
                abort();
        }

        g_free(type);
    }
    printf(")\n");
    return TRUE;
}

static gboolean export_sql( LibmsiDatabase *db, const char *table, GError **error)
{
    GError *err = NULL;
    LibmsiRecord *name = NULL;
    LibmsiRecord *type = NULL;
    LibmsiRecord *keys = NULL;
    LibmsiRecord *rec = NULL;
    LibmsiQuery *query = NULL;
    gboolean success = FALSE;
    char *sql;

    sql = g_strdup_printf("select * from `%s`", table);
    query = libmsi_query_new(db, sql, error);
    if (*error) {
        goto done;
    }

    /* write out row 1, the column names */
    name = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_NAMES, error);
    if (*error) {
        goto done;
    }

    /* write out row 2, the column types */
    type = libmsi_query_get_column_info(query, LIBMSI_COL_INFO_TYPES, error);
    if (*error) {
        goto done;
    }

    /* write out row 3, the table name + keys */
    keys = libmsi_database_get_primary_keys(db, table, error);
    if (!keys) {
        goto done;
    }

    if (!export_create_table(table, name, type, keys))
        goto done;

    /* write out row 4 onwards, the data */
    while ((rec = libmsi_query_fetch(query, &err))) {
        success = export_insert(table, name, type, rec);
        g_object_unref(rec);
        if (!success) {
            break;
        }
    }

    if (err) {
        g_propagate_error(error, err);
        success = FALSE;
    }

    g_clear_error(&err);

done:
    g_object_unref(name);
    g_object_unref(type);
    g_object_unref(keys);
    g_object_unref(query);
    return success;
}

static int cmd_export(struct Command *cmd, int argc, char **argv, GError **error)
{
    LibmsiDatabase *db = NULL;
    gboolean sql = FALSE;

    if (argc > 1 && !strcmp(argv[1], "-s")) {
        sql = TRUE;
        argc--;
        argv++;
    }

    if (argc != 3) {
        cmd_usage(stderr, cmd);
    }

    db = libmsi_database_new(argv[1], LIBMSI_DB_FLAGS_READONLY, NULL, error);
    if (!db)
        return 1;

    if (sql) {
        if (!export_sql(db, argv[2], error))
            return 1;
    } else {
#if O_BINARY
        _setmode(STDOUT_FILENO, O_BINARY);
#endif
        if (!libmsi_database_export(db, argv[2], STDOUT_FILENO, error))
            goto end;
    }

end:
    if (db)
        g_object_unref(db);

    return *error ? 1 : 0;
}

static int cmd_version(struct Command *cmd, int argc, char **argv, GError **error)
{
    printf("%s (%s) version %s\n", g_get_prgname (), PACKAGE_NAME, PACKAGE_VERSION);
    return 0;
}

static int cmd_help(struct Command *cmd, int argc, char **argv, GError **error)
{
    if (argc > 1) {
        cmd = find_cmd(argv[1]);
        if (cmd && cmd->desc) {
            cmd_usage(stdout, cmd);
        }
    }

    usage(stdout);

    return 0;
}

static struct Command cmds[] = {
    {
        .cmd = "help",
        .opts = "[SUBCOMMAND]",
        .desc = "Show program or subcommand usage",
        .func = cmd_help,
    },
    {
        .cmd = "streams",
        .opts = "FILE",
        .desc = "List streams in a .msi file",
        .func = cmd_streams,
    },
    {
        .cmd = "tables",
        .opts = "FILE",
        .desc = "List tables in a .msi file",
        .func = cmd_tables,
    },
    {
        .cmd = "extract",
        .opts = "FILE STREAM",
        .desc = "Extract a binary stream from an .msi file",
        .func = cmd_extract,
    },
    {
        .cmd = "export",
        .opts = "[-s] FILE TABLE\n\nOptions:\n"
                "  -s                Format output as an SQL query",
        .desc = "Export a table in text form from an .msi file",
        .func = cmd_export,
    },
    {
        .cmd = "suminfo",
        .opts = "FILE",
        .desc = "Print summary information",
        .func = cmd_suminfo,
    },
    {
        .cmd = "-h",
        .func = cmd_help,
    },
    {
        .cmd = "--help",
        .func = cmd_help,
    },
    {
        .cmd = "-v",
        .func = cmd_version
    },
    {
        .cmd = "--version",
        .func = cmd_version
    },
    { NULL },
};

int main(int argc, char **argv)
{
    GError *error = NULL;
    struct Command *cmd = NULL;

#if !GLIB_CHECK_VERSION(2,35,1)
    g_type_init ();
#endif
    g_set_prgname ("msiinfo");

    if (argc == 1) {
        usage(stderr);
    }

    cmd = find_cmd(argv[1]);
    if (!cmd) {
        fprintf(stderr, "%s: Unrecognized command\n", g_get_prgname ());
        usage(stderr);
    }

    int result = cmd->func(cmd, argc - 1, argv + 1, &error);
    if (error != NULL) {
        g_printerr("error: %s\n", error->message);
        print_libmsi_error(error->code);
        g_clear_error(&error);
    }

    return result;
}
