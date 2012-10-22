/*
 * Win32 definitions for Windows NT
 *
 * Copyright 1996 Alexandre Julliard
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

#ifndef _WINE_WINNT_
#define _WINE_WINNT_

#ifndef MIDL_PASS
# if defined(_MSC_VER)
#  define DECLSPEC_IMPORT __declspec(dllimport)
# elif defined(__MINGW32__) || defined(__CYGWIN__)
#  define DECLSPEC_IMPORT __attribute__((dllimport))
# else
#  define DECLSPEC_IMPORT DECLSPEC_HIDDEN
# endif
#else
# define DECLSPEC_IMPORT
#endif

#ifndef DECLSPEC_NORETURN
# if defined(_MSC_VER) && (_MSC_VER >= 1200) && !defined(MIDL_PASS)
#  define DECLSPEC_NORETURN __declspec(noreturn)
# elif defined(__GNUC__)
#  define DECLSPEC_NORETURN __attribute__((noreturn))
# else
#  define DECLSPEC_NORETURN
# endif
#endif

#ifndef DECLSPEC_ALIGN
# if defined(_MSC_VER) && (_MSC_VER >= 1300) && !defined(MIDL_PASS)
#  define DECLSPEC_ALIGN(x) __declspec(align(x))
# elif defined(__GNUC__)
#  define DECLSPEC_ALIGN(x) __attribute__((aligned(x)))
# else
#  define DECLSPEC_ALIGN(x)
# endif
#endif

#ifndef DECLSPEC_CACHEALIGN
# define DECLSPEC_CACHEALIGN DECLSPEC_ALIGN(128)
#endif

#ifndef DECLSPEC_UUID
# if defined(_MSC_VER) && (_MSC_VER >= 1100) && defined (__cplusplus)
#  define DECLSPEC_UUID(x) __declspec(uuid(x))
# else
#  define DECLSPEC_UUID(x)
# endif
#endif

#ifndef DECLSPEC_NOVTABLE
# if defined(_MSC_VER) && (_MSC_VER >= 1100) && defined(__cplusplus)
#  define DECLSPEC_NOVTABLE __declspec(novtable)
# else
#  define DECLSPEC_NOVTABLE
# endif
#endif

#ifndef DECLSPEC_SELECTANY
#if defined(_MSC_VER) && (_MSC_VER >= 1100)
#define DECLSPEC_SELECTANY __declspec(selectany)
#else
#define DECLSPEC_SELECTANY
#endif
#endif

#ifndef NOP_FUNCTION
# if defined(_MSC_VER) && (_MSC_VER >= 1210)
#  define NOP_FUNCTION __noop
# else
#  define NOP_FUNCTION (void)0
# endif
#endif

#ifndef DECLSPEC_ADDRSAFE
# if defined(_MSC_VER) && (_MSC_VER >= 1200) && (defined(_M_ALPHA) || defined(_M_AXP64))
#  define DECLSPEC_ADDRSAFE __declspec(address_safe)
# else
#  define DECLSPEC_ADDRSAFE
# endif
#endif

#ifndef FORCEINLINE
# if defined(_MSC_VER) && (_MSC_VER >= 1200)
#  define FORCEINLINE __forceinline
# elif defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 2)))
#  define FORCEINLINE inline __attribute__((always_inline))
# else
#  define FORCEINLINE inline
# endif
#endif

#ifndef DECLSPEC_DEPRECATED
# if defined(_MSC_VER) && (_MSC_VER >= 1300) && !defined(MIDL_PASS)
#  define DECLSPEC_DEPRECATED __declspec(deprecated)
#  define DEPRECATE_SUPPORTED
# elif defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 2)))
#  define DECLSPEC_DEPRECATED __attribute__((deprecated))
#  define DEPRECATE_SUPPORTED
# else
#  define DECLSPEC_DEPRECATED
#  undef  DEPRECATE_SUPPORTED
# endif
#endif

/* a couple of useful Wine extensions */

#ifdef _MSC_VER
# define DECLSPEC_EXPORT __declspec(dllexport)
#elif defined(__MINGW32__)
# define DECLSPEC_EXPORT __attribute__((dllexport))
#elif defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 3)))
# define DECLSPEC_EXPORT __attribute__((visibility ("default")))
#else
# define DECLSPEC_EXPORT
#endif

#if defined(__MSC_VER) || defined(__MINGW32__) || defined(__CYGWIN__)
# define DECLSPEC_HIDDEN
#elif defined(__GNUC__) && ((__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 3)))
# define DECLSPEC_HIDDEN __attribute__((visibility ("hidden")))
#else
# define DECLSPEC_HIDDEN
#endif

#if defined(__GNUC__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 3)))
#define __WINE_ALLOC_SIZE(x) __attribute__((__alloc_size__(x)))
#else
#define __WINE_ALLOC_SIZE(x)
#endif

#include_next <winnt.h>

#endif  /* _WINNT_ */
