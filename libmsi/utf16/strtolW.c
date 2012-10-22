/* Convert string representation of a number into an integer value.

   Copyright (C) 1991-1992, 1994-1999, 2003, 2005-2007, 2009-2012 Free Software
   Foundation, Inc.

   NOTE: The canonical source of this file is maintained with the GNU C
   Library.  Bugs can be reported to bug-glibc@gnu.org.

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 3 of the License, or any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifdef _LIBC
# define USE_NUMBER_GROUPING
#else
# include <config.h>
#endif

#include "unicode.h"
#include <errno.h>
#ifndef __set_errno
# define __set_errno(Val) errno = (Val)
#endif

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Convert NPTR to an 'unsigned long int' or 'long int' in base BASE.
   If BASE is 0 the base is determined by the presence of a leading
   zero, indicating octal or a leading "0x" or "0X", indicating hexadecimal.
   If BASE is < 2 or > 36, it is reset to 10.
   If ENDPTR is not NULL, a pointer to the character after the last
   one converted is stored in *ENDPTR.  */

long
strtolW (const WCHAR *nptr, WCHAR **endptr, int base)
{
  int negative;
  unsigned long cutoff;
  unsigned int cutlim;
  unsigned long i;
  const WCHAR *s;
  UCHAR c;
  const WCHAR *save, *end;
  int overflow;

  if (base < 0 || base == 1 || base > 36)
    {
      __set_errno (EINVAL);
      return 0;
    }

  save = s = nptr;

  /* Skip white space.  */
  while (*s == ' ' || *s == '\n' )
    ++s;
  if (*s == '\0')
    goto noconv;

  /* Check for a sign.  */
  if (*s == '-')
    {
      negative = 1;
      ++s;
    }
  else if (*s == '+')
    {
      negative = 0;
      ++s;
    }
  else
    negative = 0;

  /* Recognize number prefix and if BASE is zero, figure it out ourselves.  */
  if (*s == '0')
    {
      if ((base == 0 || base == 16) && (s[1] == 'X' || s[1] == 'x'))
        {
          s += 2;
          base = 16;
        }
      else if (base == 0)
        base = 8;
    }
  else if (base == 0)
    base = 10;

  /* Save the pointer so we can check later if anything happened.  */
  save = s;

    end = NULL;

  cutoff = ULONG_MAX / (unsigned long) base;
  cutlim = ULONG_MAX % (unsigned long) base;

  overflow = 0;
  i = 0;
  for (c = *s; c; c = *++s)
    {
      if (s == end)
        break;
      if (c >= '0' && c <= '9')
        c -= '0';
      else if (c >= 'A' && c <= 'Z')
        c = c - 'A' + 10;
      else if (c >= 'a' && c <= 'z')
        c = c - 'a' + 10;
      else
        break;
      if ((int) c >= base)
        break;
      /* Check for overflow.  */
      if (i > cutoff || (i == cutoff && c > cutlim))
        overflow = 1;
      else
        {
          i *= (unsigned long) base;
          i += c;
        }
    }

  /* Check if anything actually happened.  */
  if (s == save)
    goto noconv;

  /* Store in ENDPTR the address of one character
     past the last character we converted.  */
  if (endptr != NULL)
    *endptr = (WCHAR *) s;

  /* Check for a value that is within the range of
     'unsigned long', but outside the range of 'long'.  */
  if (overflow == 0
      && i > (negative
              ? -((unsigned long int) (LONG_MIN + 1)) + 1
              : (unsigned long int) LONG_MAX))
    overflow = 1;

  if (overflow)
    {
      __set_errno (ERANGE);
      return ULONG_MAX;
    }

  /* Return the result of the appropriate sign.  */
  return negative ? -i : i;

noconv:
  /* We must handle a special case here: the base is 0 or 16 and the
     first two characters are '0' and 'x', but the rest are no
     hexadecimal digits.  This is no error case.  We return 0 and
     ENDPTR points to the 'x'.  */
  if (endptr != NULL)
    {
      if (save - nptr >= 2 && (save[-1] == 'X' || save[-1] == 'x')
          && save[-2] == '0')
        *endptr = (WCHAR *) &save[-1];
      else
        /*  There was no number to convert.  */
        *endptr = (WCHAR *) nptr;
    }

  return 0L;
}
