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
