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
** individual tokens and sends those tokens one-by-one over to the
** parser for analysis.
*/

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>

#include "query.h"
#include "sql-parser.h"

/*
** All the keywords of the SQL language are stored as in a hash
** table composed of instances of the following structure.
*/
typedef struct Keyword Keyword;
struct Keyword {
  const char *zName;             /* The keyword name */
  int tokenType;           /* The token value for this keyword */
};

#define MAX_TOKEN_LEN 11

/*
** These are the keywords
** They MUST be in alphabetical order
*/
static const Keyword aKeywordTable[] = {
  { "ADD", TK_ADD },
  { "ALTER", TK_ALTER },
  { "AND", TK_AND },
  { "BY", TK_BY },
  { "CHAR", TK_CHAR },
  { "CHARACTER", TK_CHAR },
  { "CREATE", TK_CREATE },
  { "DELETE", TK_DELETE },
  { "DISTINCT", TK_DISTINCT },
  { "DROP", TK_DROP },
  { "FREE", TK_FREE },
  { "FROM", TK_FROM },
  { "HOLD", TK_HOLD },
  { "INSERT", TK_INSERT },
  { "INT", TK_INT },
  { "INTEGER", TK_INT },
  { "INTO", TK_INTO },
  { "IS", TK_IS },
  { "KEY", TK_KEY },
  { "LIKE", TK_LIKE },
  { "LOCALIZABLE", TK_LOCALIZABLE },
  { "LONG", TK_LONG },
  { "LONGCHAR", TK_LONGCHAR },
  { "NOT", TK_NOT },
  { "NULL", TK_NULL },
  { "OBJECT", TK_OBJECT },
  { "OR", TK_OR },
  { "ORDER", TK_ORDER },
  { "PRIMARY", TK_PRIMARY },
  { "SELECT", TK_SELECT },
  { "SET", TK_SET },
  { "SHORT", TK_SHORT },
  { "TABLE", TK_TABLE },
  { "TEMPORARY", TK_TEMPORARY },
  { "UPDATE", TK_UPDATE },
  { "VALUES", TK_VALUES },
  { "WHERE", TK_WHERE },
};

#define KEYWORD_COUNT ( sizeof aKeywordTable/sizeof (Keyword) )

/*
** Comparison function for binary search.
*/
G_GNUC_PURE
static int sql_compare_keyword(const void *m1, const void *m2){
  const char *p = m1;
  const Keyword *k = m2;
  const char *q = k->zName;

  for (; *p; p++, q++) {
    char c;
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
static int sqlite_find_keyword(const char *z, int n){
  char str[MAX_TOKEN_LEN+1];
  Keyword *r;

  if( n>MAX_TOKEN_LEN )
    return TK_ID;

  memcpy( str, z, n*sizeof (char) );
  str[n] = 0;
  r = bsearch( str, aKeywordTable, KEYWORD_COUNT, sizeof (Keyword), sql_compare_keyword );
  if( r )
    return r->tokenType;
  return TK_ID;
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
static const char isIdChar[] = {
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
int sql_get_token(const char *zz, int *tokenType, int *skip){
  guint8 *z = (guint8*)zz;
  int i;

  *skip = 0;
  switch( *z ){
    case ' ': case '\t': case '\n': case '\f':
      for(i=1; isspace(z[i]) && z[i] != '\r'; i++){}
      *tokenType = TK_SPACE;
      return i;
    case '-':
      if( z[1]==0 ) return -1;
      *tokenType = TK_MINUS;
      return 1;
    case '(':
      *tokenType = TK_LP;
      return 1;
    case ')':
      *tokenType = TK_RP;
      return 1;
    case '*':
      *tokenType = TK_STAR;
      return 1;
    case '=':
      *tokenType = TK_EQ;
      return 1;
    case '<':
      if( z[1]=='=' ){
        *tokenType = TK_LE;
        return 2;
      }else if( z[1]=='>' ){
        *tokenType = TK_NE;
        return 2;
      }else{
        *tokenType = TK_LT;
        return 1;
      }
    case '>':
      if( z[1]=='=' ){
        *tokenType = TK_GE;
        return 2;
      }else{
        *tokenType = TK_GT;
        return 1;
      }
    case '!':
      if( z[1]!='=' ){
        *tokenType = TK_ILLEGAL;
        return 2;
      }else{
        *tokenType = TK_NE;
        return 2;
      }
    case '?':
      *tokenType = TK_WILDCARD;
      return 1;
    case ',':
      *tokenType = TK_COMMA;
      return 1;
    case '`': case '\'': {
      int delim = z[0];
      for(i=1; z[i]; i++){
        if( z[i]==delim )
          break;
      }
      if( z[i] ) i++;
      if( delim == '`' )
        *tokenType = TK_ID;
      else
        *tokenType = TK_STRING;
      return i;
    }
    case '.':
      if( !isdigit(z[1]) ){
        *tokenType = TK_DOT;
        return 1;
      }
      /* Fall through */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      *tokenType = TK_INTEGER;
      for(i=1; isdigit(z[i]); i++){}
      return i;
    case '[':
      for(i=1; z[i] && z[i-1]!=']'; i++){}
      *tokenType = TK_ID;
      return i;
    default:
      if( !isIdChar[*z] ){
        break;
      }
      for(i=1; isIdChar[z[i]]; i++){}
      *tokenType = sqlite_find_keyword(zz, i);
      if( *tokenType == TK_ID && z[i] == '`' ) *skip = 1;
      return i;
  }
  *tokenType = TK_ILLEGAL;
  return 1;
}
