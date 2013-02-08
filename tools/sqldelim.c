/*
** 2001 September 15
**
** The author disclaims copyright to this source code.  In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
*************************************************************************
** A tokenizer for SQL
**
** This file contains C code that splits an SQL input string up into
** individual tokens, groups them back into statements, and passes the
** statements up to a user-defined callback.
*/

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <glib.h>

#include "sqldelim.h"

/*
** All the keywords of the SQL language are stored as in a hash
** table composed of instances of the following structure.
*/
typedef struct Keyword Keyword;
struct Keyword {
    const char *zName;      /* The keyword name */
};

#define MAX_TOKEN_LEN 11

/*
** These are the keywords that begin a new SQL statement.
** They MUST be in alphabetical order
*/
static const Keyword aKeywordTable[] = {
    { "ALTER" },
    { "CREATE" },
    { "DELETE" },
    { "DROP" },
    { "INSERT" },
    { "SELECT" },
    { "UPDATE" },
};

#define KEYWORD_COUNT (sizeof aKeywordTable / sizeof (Keyword))

/*
** Comparison function for binary search.
*/
G_GNUC_PURE
static int sql_compare_keyword(const void *m1, const void *m2){
    const uint8_t *p = m1;
    const Keyword *k = m2;
    const char *q = k->zName;

    for (; *p; p++, q++) {
        uint8_t c;
        if ((uint16_t) *p > 127)
            return 1;
        c = *p;
        if (c >= 'a' && c <= 'z')
            c ^= 'A' ^ 'a';
        if (c != *q)
            return (unsigned)c - (unsigned)*q;
    }

    return (unsigned)*p - (unsigned)*q;
}

/*
** This function looks up an identifier to determine if it is a
** keyword.  If it is a keyword, the token code of that keyword is 
** returned.  If the input is not a keyword, TK_ID is returned.
*/
static int sqlite_find_keyword(const char *z, int n)
{
    char str[MAX_TOKEN_LEN + 1];
    Keyword *r;

    if (n > MAX_TOKEN_LEN)
        return false;

    memcpy(str, z, n);
    str[n] = 0;
    r = bsearch(str, aKeywordTable, KEYWORD_COUNT, sizeof (Keyword), sql_compare_keyword);
    return r != NULL;
}


/*
** If X is a character that can be used in an identifier then
** isIdChar[X] will be 1.  Otherwise isIdChar[X] will be 0.
**
** In this implementation, an identifier can be a string of
** alphabetic characters, digits, and "_" plus any character
** with the high-order bit set.  The latter rule means that
** any sequence of UTF-8 characters or characters taken from
** an extended ISO8859 character set can form an identifier.
*/
static const uint8_t isIdChar[] = {
/* x0 x1 x2 x3 x4 x5 x6 x7 x8 x9 xA xB xC xD xE xF */
    0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 1x */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0,  /* 2x */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,  /* 3x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 4x */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1,  /* 5x */
    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 6x */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  /* 7x */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 8x */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 9x */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* Ax */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* Bx */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* Cx */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* Dx */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* Ex */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* Fx */
};


/*
** Return the length of the token that begins at z[0].  Return
** -1 if the token is (or might be) incomplete.  Store the token
** type in *tokenType before returning.
*/
static int sql_skip_token(const char **p, bool *cont)
{
    int i = 1;
    const uint8_t *z = (uint8_t *) *p;
    bool get_keyword = *cont;

    *cont = true;
    switch (*z) {
    case ' ': case '\t': case '\n': case '\f':
        while (isspace(z[i]) && z[i] != '\r') i++;
        *p += i;
        return false;
    case '-':
    case '(':
    case ')':
    case '*':
    case '=':
    case '<':
    case '>':
    case '!':
    case '?':
    case ',':
    case '.':
        *p += 1;
        return false;
    case '`': case '\'': {
        int delim = z[0];
        while (z[i])
            if (z[i++] == delim)
               break;
        *p += i;
        return false;
    }
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        while (isdigit(z[i])) i++;
        *p += i;
        return false;
    case '[':
        while (z[i] && z[i-1] != ']') i++;
        *p += i;
        return false;
    default:
        if (!isIdChar[*z]) {
            *p += 1;
            return true;
        }
        while (isIdChar[z[i]]) i++;
        if (get_keyword && sqlite_find_keyword(*p, i)) {
            return true;
        } else {
            /* Do not recognize a keyword at the beginning of the next chunk.  */
            if (!z[i]) {
                *cont = false;
            }
            *p += i;
            return false;
        }
    }
}

int sql_get_statement(const char *start,
                      int (*fn)(const char *stmt, void *opaque),
                      void *opaque)
{
    static GString str;
    static bool cont = false;

    const char *p = start;
    char *stmt;
    bool done;
    int ret = 0;

    /* Final part?  Build a statement with what's left.  */
    if (!*p) {
        goto stmt;
    }

    while (*p) {
        start = p;
        /* A semicolon is not part of the SQL syntax, skip it and conclude
         * this statement.
         */
        if (*p == ';') {
            done = true;
            p++;
        } else {
            done = sql_skip_token(&p, &cont);
            g_string_append_len(&str, start, p - start);
        }

        if (done) {
stmt:
            cont = false;
            stmt = g_strndup(str.str, str.len);
            g_string_erase(&str, 0, str.len);
            if (stmt[0]) {
                ret = fn(stmt, opaque);
            }
            free(stmt);
            if (ret) {
                return ret;
            }
        }
    }
    return 0;
}

#if 0
int main()
{
    uint8_t line[100], *stmt;
    const uint8_t *p;

    while (fgets(line, sizeof(line), stdin)) {
        sql_get_statement(line, puts);
    }
    sql_get_statement("", puts);
}
#endif
