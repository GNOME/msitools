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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *program_name;

struct Command {
    const char *cmd;
    const char *desc;
    const char *usage;
    const char *opts;
    const char *help;
    int (*func)(struct Command *cmd, int argc, char **argv);
};

static struct Command cmds[];

static char *get_basename(char *path)
{
    char *p;
    if (!path || !*path) {
        return ".";
    }
    p = path + strlen(path);
    while (p > path && *p != '/' && *p != '\\') {
        p--;
    }
    if (p > path) {
        p++;
    }
    return p;
}

static void usage(FILE *out)
{
    int i;

    fprintf(out, "Usage: %s SUBCOMMAND COMMAND-OPTIONS...\n\n", program_name);
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
    fprintf(out, "%s %s %s\n\n%s.\n", program_name, cmd->cmd, cmd->opts,
            cmd->desc);

    if (cmd->help) {
        fprintf(out, "\n%s\n", cmd->help);
    }
    exit (out == stderr);
}

static void print_libmsi_error(LibmsiResult r)
{
    switch (r)
    {
    case LIBMSI_RESULT_SUCCESS:
        abort();
    case LIBMSI_RESULT_CONTINUE:
        fprintf(stderr, "%s: internal error (continue)\n", program_name);
        exit(1);
    case LIBMSI_RESULT_MORE_DATA:
        fprintf(stderr, "%s: internal error (more data)\n", program_name);
        exit(1);
    case LIBMSI_RESULT_INVALID_HANDLE:
        fprintf(stderr, "%s: internal error (invalid handle)\n", program_name);
        exit(1);
    case LIBMSI_RESULT_NOT_ENOUGH_MEMORY:
    case LIBMSI_RESULT_OUTOFMEMORY:
        fprintf(stderr, "%s: out of memory\n", program_name);
        exit(1);
    case LIBMSI_RESULT_INVALID_DATA:
        fprintf(stderr, "%s: invalid data\n", program_name);
        exit(1);
    case LIBMSI_RESULT_INVALID_PARAMETER:
        fprintf(stderr, "%s: invalid parameter\n", program_name);
        exit(1);
    case LIBMSI_RESULT_OPEN_FAILED:
        fprintf(stderr, "%s: open failed\n", program_name);
        exit(1);
    case LIBMSI_RESULT_CALL_NOT_IMPLEMENTED:
        fprintf(stderr, "%s: not implemented\n", program_name);
        exit(1);
    case LIBMSI_RESULT_NO_MORE_ITEMS:
    case LIBMSI_RESULT_NOT_FOUND:
        fprintf(stderr, "%s: not found\n", program_name);
        exit(1);
    case LIBMSI_RESULT_UNKNOWN_PROPERTY:
        fprintf(stderr, "%s: unknown property\n", program_name);
        exit(1);
    case LIBMSI_RESULT_BAD_QUERY_SYNTAX:
        fprintf(stderr, "%s: bad query syntax\n", program_name);
        exit(1);
    case LIBMSI_RESULT_INVALID_FIELD:
        fprintf(stderr, "%s: invalid field\n", program_name);
        exit(1);
    case LIBMSI_RESULT_FUNCTION_FAILED:
        fprintf(stderr, "%s: internal error (function failed)\n", program_name);
        exit(1);
    case LIBMSI_RESULT_INVALID_TABLE:
        fprintf(stderr, "%s: invalid table\n", program_name);
        exit(1);
    case LIBMSI_RESULT_DATATYPE_MISMATCH:
        fprintf(stderr, "%s: datatype mismatch\n", program_name);
        exit(1);
    case LIBMSI_RESULT_INVALID_DATATYPE:
        fprintf(stderr, "%s: invalid datatype\n", program_name);
        exit(1);
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

    fprintf(stderr, "%s: Unrecognized command '%s'\n", program_name, s);
    return NULL;
}

static LibmsiResult print_strings_from_query(LibmsiQuery *query)
{
    LibmsiRecord *rec = NULL;
    LibmsiResult r;
    char name[PATH_MAX];

    while ((r = libmsi_query_fetch(query, &rec)) == LIBMSI_RESULT_SUCCESS) {
        size_t size = PATH_MAX;
        r = libmsi_record_get_string(rec, 1, name, &size);
        if (r) {
            print_libmsi_error(r);
        }

        puts(name);
        libmsi_unref(rec);
    }

    if (r == LIBMSI_RESULT_NO_MORE_ITEMS) {
        r = LIBMSI_RESULT_SUCCESS;
    }
    return r;
}

static int cmd_streams(struct Command *cmd, int argc, char **argv)
{
    LibmsiDatabase *db = NULL;
    LibmsiQuery *query = NULL;
    LibmsiResult r;

    if (argc != 2) {
        cmd_usage(stderr, cmd);
    }

    r = libmsi_database_open(argv[1], LIBMSI_DB_OPEN_READONLY, &db);
    if (r) {
        print_libmsi_error(r);
    }

    r = libmsi_database_open_query(db, "SELECT `Name` FROM `_Streams`", &query);
    if (r) {
        print_libmsi_error(r);
    }

    r = libmsi_query_execute(query, NULL);
    if (r) {
        print_libmsi_error(r);
    }

    r = print_strings_from_query(query);
    if (r) {
        print_libmsi_error(r);
    }

    libmsi_unref(query);
    libmsi_unref(db);
}

static int cmd_tables(struct Command *cmd, int argc, char **argv)
{
    LibmsiDatabase *db = NULL;
    LibmsiQuery *query = NULL;
    LibmsiResult r;

    if (argc != 2) {
        cmd_usage(stderr, cmd);
    }

    r = libmsi_database_open(argv[1], LIBMSI_DB_OPEN_READONLY, &db);
    if (r) {
        print_libmsi_error(r);
    }

    r = libmsi_database_open_query(db, "SELECT `Name` FROM `_Tables`", &query);
    if (r) {
        print_libmsi_error(r);
    }

    r = libmsi_query_execute(query, NULL);
    if (r) {
        print_libmsi_error(r);
    }

    r = print_strings_from_query(query);
    if (r) {
        print_libmsi_error(r);
    }

    libmsi_unref(query);
    libmsi_unref(db);
}

static int cmd_version(struct Command *cmd, int argc, char **argv)
{
    printf("%s (%s) version %s\n", program_name, PACKAGE, VERSION);
    return 0;
}

static int cmd_help(struct Command *cmd, int argc, char **argv)
{
    if (argc > 1) {
        cmd = find_cmd(argv[1]);
        if (cmd && cmd->desc) {
            cmd_usage(stdout, cmd);
        }
    }

    usage(stdout);
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
    struct Command *cmd = NULL;

    program_name = get_basename(argv[0]);

    if (argc == 1) {
        usage(stderr);
    }

    cmd = find_cmd(argv[1]);
    if (!cmd) {
        fprintf(stderr, "%s: Unrecognized command\n", program_name);
        usage(stderr);
    }

    return cmd->func(cmd, argc - 1, argv + 1);
}
