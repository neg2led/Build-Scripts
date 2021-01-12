/* Copyright (C) 1991-2020 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifdef _LIBC
# define USE_IN_EXTENDED_LOCALE_MODEL 1
# define HAVE_STRUCT_ERA_ENTRY 1
# define HAVE_TM_GMTOFF 1
# define HAVE_TM_ZONE 1
# define HAVE_TZNAME 1
# define HAVE_TZSET 1
# include "../locale/localeinfo.h"
#else
# include <config.h>
# if FPRINTFTIME
#  include "fprintftime.h"
# else
#  include "strftime.h"
# endif
# include "time-internal.h"
#endif

#include <ctype.h>
#include <time.h>

#if HAVE_TZNAME && !HAVE_DECL_TZNAME
extern char *tzname[];
#endif

/* Do multibyte processing if multibyte encodings are supported, unless
   multibyte sequences are safe in formats.  Multibyte sequences are
   safe if they cannot contain byte sequences that look like format
   conversion specifications.  The multibyte encodings used by the
   C library on the various platforms (UTF-8, GB2312, GBK, CP936,
   GB18030, EUC-TW, BIG5, BIG5-HKSCS, CP950, EUC-JP, EUC-KR, CP949,
   SHIFT_JIS, CP932, JOHAB) are safe for formats, because the byte '%'
   cannot occur in a multibyte character except in the first byte.

   The DEC-HANYU encoding used on OSF/1 is not safe for formats, but
   this encoding has never been seen in real-life use, so we ignore
   it.  */
#if !(defined __osf__ && 0)
# define MULTIBYTE_IS_FORMAT_SAFE 1
#endif
#define DO_MULTIBYTE (! MULTIBYTE_IS_FORMAT_SAFE)

#if DO_MULTIBYTE
# include <wchar.h>
  static const mbstate_t mbstate_zero;
#endif

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "attribute.h"
#include <intprops.h>

#ifdef COMPILE_WIDE
# include <endian.h>
# define CHAR_T wchar_t
# define UCHAR_T unsigned int
# define L_(Str) L##Str
# define NLW(Sym) _NL_W##Sym

# define MEMCPY(d, s, n) __wmemcpy (d, s, n)
# define STRLEN(s) __wcslen (s)

#else
# define CHAR_T char
# define UCHAR_T unsigned char
# define L_(Str) Str
# define NLW(Sym) Sym
# define ABALTMON_1 _NL_ABALTMON_1

# define MEMCPY(d, s, n) memcpy (d, s, n)
# define STRLEN(s) strlen (s)

#endif

/* Shift A right by B bits portably, by dividing A by 2**B and
   truncating towards minus infinity.  A and B should be free of side
   effects, and B should be in the range 0 <= B <= INT_BITS - 2, where
   INT_BITS is the number of useful bits in an int.  GNU code can
   assume that INT_BITS is at least 32.

   ISO C99 says that A >> B is implementation-defined if A < 0.  Some
   implementations (e.g., UNICOS 9.0 on a Cray Y-MP EL) don't shift
   right in the usual way when A < 0, so SHR falls back on division if
   ordinary A >> B doesn't seem to be the usual signed shift.  */
#define SHR(a, b)       \
  (-1 >> 1 == -1        \
   ? (a) >> (b)         \
   : ((a) + ((a) < 0)) / (1 << (b)) - ((a) < 0))

#define TM_YEAR_BASE 1900

#ifndef __isleap
/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is).  */
# define __isleap(year) \
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))
#endif


#ifdef _LIBC
# define mktime_z(tz, tm) mktime (tm)
# define tzname __tzname
# define tzset __tzset
#endif

#ifndef FPRINTFTIME
# define FPRINTFTIME 0
#endif

#if FPRINTFTIME
# define STREAM_OR_CHAR_T FILE
# define STRFTIME_ARG(x) /* empty */
#else
# define STREAM_OR_CHAR_T CHAR_T
# define STRFTIME_ARG(x) x,
#endif

#if FPRINTFTIME
# define memset_byte(P, Len, Byte) \
  do { size_t _i; for (_i = 0; _i < Len; _i++) fputc (Byte, P); } while (0)
# define memset_space(P, Len) memset_byte (P, Len, ' ')
# define memset_zero(P, Len) memset_byte (P, Len, '0')
#elif defined COMPILE_WIDE
# define memset_space(P, Len) (wmemset (P, L' ', Len), (P) += (Len))
# define memset_zero(P, Len) (wmemset (P, L'0', Len), (P) += (Len))
#else
# define memset_space(P, Len) (memset (P, ' ', Len), (P) += (Len))
# define memset_zero(P, Len) (memset (P, '0', Len), (P) += (Len))
#endif

#if FPRINTFTIME
# define advance(P, N)
#else
# define advance(P, N) ((P) += (N))
#endif

#define add(n, f) width_add (width, n, f)
#define width_add(width, n, f)                                                \
  do                                                                          \
    {                                                                         \
      size_t _n = (n);                                                        \
      size_t _w = pad == L_('-') || width < 0 ? 0 : width;                    \
      size_t _incr = _n < _w ? _w : _n;                                       \
      if (_incr >= maxsize - i)                                               \
        return 0;                                                             \
      if (p)                                                                  \
        {                                                                     \
          if (_n < _w)                                                        \
            {                                                                 \
              size_t _delta = _w - _n;                                        \
              if (pad == L_('0') || pad == L_('+'))                           \
                memset_zero (p, _delta);                                      \
              else                                                            \
                memset_space (p, _delta);                                     \
            }                                                                 \
          f;                                                                  \
          advance (p, _n);                                                    \
        }                                                                     \
      i += _incr;                                                             \
    } while (0)

#define add1(c) width_add1 (width, c)
#if FPRINTFTIME
# define width_add1(width, c) width_add (width, 1, fputc (c, p))
#else
# define width_add1(width, c) width_add (width, 1, *p = c)
#endif

#define cpy(n, s) width_cpy (width, n, s)
#if FPRINTFTIME
# define width_cpy(width, n, s)                                               \
    width_add (width, n,                                                      \
     do                                                                       \
       {                                                                      \
         if (to_lowcase)                                                      \
           fwrite_lowcase (p, (s), _n);                                       \
         else if (to_uppcase)                                                 \
           fwrite_uppcase (p, (s), _n);                                       \
         else                                                                 \
           {                                                                  \
             /* Ignore the value of fwrite.  The caller can determine whether \
                an error occurred by inspecting ferror (P).  All known fwrite \
                implementations set the stream's error indicator when they    \
                fail due to ENOMEM etc., even though C11 and POSIX.1-2008 do  \
                not require this.  */                                         \
             fwrite (s, _n, 1, p);                                            \
           }                                                                  \
       }                                                                      \
     while (0)                                                                \
    )
#else
# define width_cpy(width, n, s)                                               \
    width_add (width, n,                                                      \
         if (to_lowcase)                                                      \
           memcpy_lowcase (p, (s), _n LOCALE_ARG);                            \
         else if (to_uppcase)                                                 \
           memcpy_uppcase (p, (s), _n LOCALE_ARG);                            \
         else                                                                 \
           MEMCPY ((void *) p, (void const *) (s), _n))
#endif

#ifdef COMPILE_WIDE
# ifndef USE_IN_EXTENDED_LOCALE_MODEL
#  undef __mbsrtowcs_l
#  define __mbsrtowcs_l(d, s, l, st, loc) __mbsrtowcs (d, s, l, st)
# endif
# define widen(os, ws, l) \
  {                                                                           \
    mbstate_t __st;                                                           \
    const char *__s = os;                                                     \
    memset (&__st, '\0', sizeof (__st));                                      \
    l = __mbsrtowcs_l (NULL, &__s, 0, &__st, loc);                            \
    ws = (wchar_t *) alloca ((l + 1) * sizeof (wchar_t));                     \
    (void) __mbsrtowcs_l (ws, &__s, l, &__st, loc);                           \
  }
#endif


#if defined _LIBC && defined USE_IN_EXTENDED_LOCALE_MODEL
/* We use this code also for the extended locale handling where the
   function gets as an additional argument the locale which has to be
   used.  To access the values we have to redefine the _NL_CURRENT
   macro.  */
# define strftime               __strftime_l
# define wcsftime               __wcsftime_l
# undef _NL_CURRENT
# define _NL_CURRENT(category, item) \
  (current->values[_NL_ITEM_INDEX (item)].string)
# define LOCALE_PARAM , locale_t loc
# define LOCALE_ARG , loc
# define HELPER_LOCALE_ARG  , current
#else
# define LOCALE_PARAM
# define LOCALE_ARG
# ifdef _LIBC
#  define HELPER_LOCALE_ARG , _NL_CURRENT_DATA (LC_TIME)
# else
#  define HELPER_LOCALE_ARG
# endif
#endif

#ifdef COMPILE_WIDE
# ifdef USE_IN_EXTENDED_LOCALE_MODEL
#  define TOUPPER(Ch, L) __towupper_l (Ch, L)
#  define TOLOWER(Ch, L) __towlower_l (Ch, L)
# else
#  define TOUPPER(Ch, L) towupper (Ch)
#  define TOLOWER(Ch, L) towlower (Ch)
# endif
#else
# ifdef USE_IN_EXTENDED_LOCALE_MODEL
#  define TOUPPER(Ch, L) __toupper_l (Ch, L)
#  define TOLOWER(Ch, L) __tolower_l (Ch, L)
# else
#  define TOUPPER(Ch, L) toupper (Ch)
#  define TOLOWER(Ch, L) tolower (Ch)
# endif
#endif
/* We don't use 'isdigit' here since the locale dependent
   interpretation is not what we want here.  We only need to accept
   the arabic digits in the ASCII range.  One day there is perhaps a
   more reliable way to accept other sets of digits.  */
#define ISDIGIT(Ch) ((unsigned int) (Ch) - L_('0') <= 9)

#if FPRINTFTIME
static void
fwrite_lowcase (FILE *fp, const CHAR_T *src, size_t len)
{
  while (len-- > 0)
    {
      fputc (TOLOWER ((UCHAR_T) *src, loc), fp);
      ++src;
    }
}

static void
fwrite_uppcase (FILE *fp, const CHAR_T *src, size_t len)
{
  while (len-- > 0)
    {
      fputc (TOUPPER ((UCHAR_T) *src, loc), fp);
      ++src;
    }
}
#else
static CHAR_T *memcpy_lowcase (CHAR_T *dest, const CHAR_T *src,
                               size_t len LOCALE_PARAM);

static CHAR_T *
memcpy_lowcase (CHAR_T *dest, const CHAR_T *src, size_t len LOCALE_PARAM)
{
  while (len-- > 0)
    dest[len] = TOLOWER ((UCHAR_T) src[len], loc);
  return dest;
}

static CHAR_T *memcpy_uppcase (CHAR_T *dest, const CHAR_T *src,
                               size_t len LOCALE_PARAM);

static CHAR_T *
memcpy_uppcase (CHAR_T *dest, const CHAR_T *src, size_t len LOCALE_PARAM)
{
  while (len-- > 0)
    dest[len] = TOUPPER ((UCHAR_T) src[len], loc);
  return dest;
}
#endif


#if ! HAVE_TM_GMTOFF
/* Yield the difference between *A and *B,
   measured in seconds, ignoring leap seconds.  */
# define tm_diff ftime_tm_diff
static int tm_diff (const struct tm *, const struct tm *);
static int
tm_diff (const struct tm *a, const struct tm *b)
{
  /* Compute intervening leap days correctly even if year is negative.
     Take care to avoid int overflow in leap day calculations,
     but it's OK to assume that A and B are close to each other.  */
  int a4 = SHR (a->tm_year, 2) + SHR (TM_YEAR_BASE, 2) - ! (a->tm_year & 3);
  int b4 = SHR (b->tm_year, 2) + SHR (TM_YEAR_BASE, 2) - ! (b->tm_year & 3);
  int a100 = (a4 + (a4 < 0)) / 25 - (a4 < 0);
  int b100 = (b4 + (b4 < 0)) / 25 - (b4 < 0);
  int a400 = SHR (a100, 2);
  int b400 = SHR (b100, 2);
  int intervening_leap_days = (a4 - b4) - (a100 - b100) + (a400 - b400);
  int years = a->tm_year - b->tm_year;
  int days = (365 * years + intervening_leap_days
              + (a->tm_yday - b->tm_yday));
  return (60 * (60 * (24 * days + (a->tm_hour - b->tm_hour))
                + (a->tm_min - b->tm_min))
          + (a->tm_sec - b->tm_sec));
}
#endif /* ! HAVE_TM_GMTOFF */



/* The number of days from the first day of the first ISO week of this
   year to the year day YDAY with week day WDAY.  ISO weeks start on
   Monday; the first ISO week has the year's first Thursday.  YDAY may
   be as small as YDAY_MINIMUM.  */
#define ISO_WEEK_START_WDAY 1 /* Monday */
#define ISO_WEEK1_WDAY 4 /* Thursday */
#define YDAY_MINIMUM (-366)
static int iso_week_days (int, int);
#ifdef __GNUC__
__inline__
#endif
static int
iso_week_days (int yday, int wday)
{
  /* Add enough to the first operand of % to make it nonnegative.  */
  int big_enough_multiple_of_7 = (-YDAY_MINIMUM / 7 + 2) * 7;
  return (yday
          - (yday - wday + ISO_WEEK1_WDAY + big_enough_multiple_of_7) % 7
          + ISO_WEEK1_WDAY - ISO_WEEK_START_WDAY);
}


/* When compiling this file, GNU applications can #define my_strftime
   to a symbol (typically nstrftime) to get an extended strftime with
   extra arguments TZ and NS.  */

#if FPRINTFTIME
# undef my_strftime
# define my_strftime fprintftime
#endif

#ifdef my_strftime
# undef HAVE_TZSET
# define extra_args , tz, ns
# define extra_args_spec , timezone_t tz, int ns
#else
# if defined COMPILE_WIDE
#  define my_strftime wcsftime
#  define nl_get_alt_digit _nl_get_walt_digit
# else
#  define my_strftime strftime
#  define nl_get_alt_digit _nl_get_alt_digit
# endif
# define extra_args
# define extra_args_spec
/* We don't have this information in general.  */
# define tz 1
# define ns 0
#endif

static size_t __strftime_internal (STREAM_OR_CHAR_T *, STRFTIME_ARG (size_t)
                                   const CHAR_T *, const struct tm *,
                                   bool, int, int, bool *
                                   extra_args_spec LOCALE_PARAM);

/* Write information from TP into S according to the format
   string FORMAT, writing no more that MAXSIZE characters
   (including the terminating '\0') and returning number of
   characters written.  If S is NULL, nothing will be written
   anywhere, so to determine how many characters would be
   written, use NULL for S and (size_t) -1 for MAXSIZE.  */
size_t
my_strftime (STREAM_OR_CHAR_T *s, STRFTIME_ARG (size_t maxsize)
             const CHAR_T *format,
             const struct tm *tp extra_args_spec LOCALE_PARAM)
{
  bool tzset_called = false;
  return __strftime_internal (s, STRFTIME_ARG (maxsize) format, tp, false,
                              0, -1, &tzset_called extra_args LOCALE_ARG);
}
#if defined _LIBC && ! FPRINTFTIME
libc_hidden_def (my_strftime)
#endif

/* Just like my_strftime, above, but with more parameters.
   UPCASE indicates that the result should be converted to upper case.
   YR_SPEC and WIDTH specify the padding and width for the year.
   *TZSET_CALLED indicates whether tzset has been called here.  */
static size_t
__strftime_internal (STREAM_OR_CHAR_T *s, STRFTIME_ARG (size_t maxsize)
                     const CHAR_T *format,
                     const struct tm *tp, bool upcase,
                     int yr_spec, int width, bool *tzset_called
                     extra_args_spec LOCALE_PARAM)
{
#if defined _LIBC && defined USE_IN_EXTENDED_LOCALE_MODEL
  struct __locale_data *const current = loc->__locales[LC_TIME];
#endif
#if FPRINTFTIME
  size_t maxsize = (size_t) -1;
#endif

  int hour12 = tp->tm_hour;
#ifdef _NL_CURRENT
  /* We cannot make the following values variables since we must delay
     the evaluation of these values until really needed since some
     expressions might not be valid in every situation.  The 'struct tm'
     might be generated by a strptime() call that initialized
     only a few elements.  Dereference the pointers only if the format
     requires this.  Then it is ok to fail if the pointers are invalid.  */
# define a_wkday \
  ((const CHAR_T *) (tp->tm_wday < 0 || tp->tm_wday > 6                      \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(ABDAY_1) + tp->tm_wday)))
# define f_wkday \
  ((const CHAR_T *) (tp->tm_wday < 0 || tp->tm_wday > 6                      \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(DAY_1) + tp->tm_wday)))
# define a_month \
  ((const CHAR_T *) (tp->tm_mon < 0 || tp->tm_mon > 11                       \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(ABMON_1) + tp->tm_mon)))
# define f_month \
  ((const CHAR_T *) (tp->tm_mon < 0 || tp->tm_mon > 11                       \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(MON_1) + tp->tm_mon)))
# define a_altmonth \
  ((const CHAR_T *) (tp->tm_mon < 0 || tp->tm_mon > 11                       \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(ABALTMON_1) + tp->tm_mon)))
# define f_altmonth \
  ((const CHAR_T *) (tp->tm_mon < 0 || tp->tm_mon > 11                       \
                     ? "?" : _NL_CURRENT (LC_TIME, NLW(ALTMON_1) + tp->tm_mon)))
# define ampm \
  ((const CHAR_T *) _NL_CURRENT (LC_TIME, tp->tm_hour > 11                    \
                                 ? NLW(PM_STR) : NLW(AM_STR)))

# define aw_len STRLEN (a_wkday)
# define am_len STRLEN (a_month)
# define aam_len STRLEN (a_altmonth)
# define ap_len STRLEN (ampm)
#endif
#if HAVE_TZNAME
  char **tzname_vec = tzname;
#endif
  const char *zone;
  size_t i = 0;
  STREAM_OR_CHAR_T *p = s;
  const CHAR_T *f;
#if DO_MULTIBYTE && !defined COMPILE_WIDE
  const char *format_end = NULL;
#endif

#if ! defined _LIBC && ! HAVE_RUN_TZSET_TEST
  /* Solaris 2.5.x and 2.6 tzset sometimes modify the storage returned
     by localtime.  On such systems, we must either use the tzset and
     localtime wrappers to work around the bug (which sets
     HAVE_RUN_TZSET_TEST) or make a copy of the structure.  */
  struct tm copy = *tp;
  tp = &copy;
#endif

  zone = NULL;
#if HAVE_TM_ZONE
  /* The POSIX test suite assumes that setting
     the environment variable TZ to a new value before calling strftime()
     will influence the result (the %Z format) even if the information in
     TP is computed with a totally different time zone.
     This is bogus: though POSIX allows bad behavior like this,
     POSIX does not require it.  Do the right thing instead.  */
  zone = (const char *) tp->tm_zone;
#endif
#if HAVE_TZNAME
  if (!tz)
    {
      if (! (zone && *zone))
        zone = "GMT";
    }
  else
    {
# if !HAVE_TM_ZONE
      /* Infer the zone name from *TZ instead of from TZNAME.  */
      tzname_vec = tz->tzname_copy;
# endif
    }
  /* The tzset() call might have changed the value.  */
  if (!(zone && *zone) && tp->tm_isdst >= 0)
    {
      /* POSIX.1 requires that local time zone information be used as
         though strftime called tzset.  */
# if HAVE_TZSET
      if (!*tzset_called)
        {
          tzset ();
          *tzset_called = true;
        }
# endif
      zone = tzname_vec[tp->tm_isdst != 0];
    }
#endif
  if (! zone)
    zone = "";

  if (hour12 > 12)
    hour12 -= 12;
  else
    if (hour12 == 0)
      hour12 = 12;

  for (f = format; *f != '\0'; width = -1, f++)
    {
      int pad = 0;  /* Padding for number ('_', '-', '+', '0', or 0).  */
      int modifier;             /* Field modifier ('E', 'O', or 0).  */
      int digits = 0;           /* Max digits for numeric format.  */
      int number_value;         /* Numeric value to be printed.  */
      unsigned int u_number_value; /* (unsigned int) number_value.  */
      bool negative_number;     /* The number is negative.  */
      bool always_output_a_sign; /* +/- should always be output.  */
      int tz_colon_mask;        /* Bitmask of where ':' should appear.  */
      const CHAR_T *subfmt;
      CHAR_T *bufp;
      CHAR_T buf[1
                 + 2 /* for the two colons in a %::z or %:::z time zone */
                 + (sizeof (int) < sizeof (time_t)
                    ? INT_STRLEN_BOUND (time_t)
                    : INT_STRLEN_BOUND (int))];
      bool to_lowcase = false;
      bool to_uppcase = upcase;
      size_t colons;
      bool change_case = false;
      int format_char;
      int subwidth;

#if DO_MULTIBYTE && !defined COMPILE_WIDE
      switch (*f)
        {
        case L_('%'):
          break;

        case L_('\b'): case L_('\t'): case L_('\n'):
        case L_('\v'): case L_('\f'): case L_('\r'):
        case L_(' '): case L_('!'): case L_('"'): case L_('#'): case L_('&'):
        case L_('\''): case L_('('): case L_(')'): case L_('*'): case L_('+'):
        case L_(','): case L_('-'): case L_('.'): case L_('/'): case L_('0'):
        case L_('1'): case L_('2'): case L_('3'): case L_('4'): case L_('5'):
        case L_('6'): case L_('7'): case L_('8'): case L_('9'): case L_(':'):
        case L_(';'): case L_('<'): case L_('='): case L_('>'): case L_('?'):
        case L_('A'): case L_('B'): case L_('C'): case L_('D'): case L_('E'):
        case L_('F'): case L_('G'): case L_('H'): case L_('I'): case L_('J'):
        case L_('K'): case L_('L'): case L_('M'): case L_('N'): case L_('O'):
        case L_('P'): case L_('Q'): case L_('R'): case L_('S'): case L_('T'):
        case L_('U'): case L_('V'): case L_('W'): case L_('X'): case L_('Y'):
        case L_('Z'): case L_('['): case L_('\\'): case L_(']'): case L_('^'):
        case L_('_'): case L_('a'): case L_('b'): case L_('c'): case L_('d'):
        case L_('e'): case L_('f'): case L_('g'): case L_('h'): case L_('i'):
        case L_('j'): case L_('k'): case L_('l'): case L_('m'): case L_('n'):
        case L_('o'): case L_('p'): case L_('q'): case L_('r'): case L_('s'):
        case L_('t'): case L_('u'): case L_('v'): case L_('w'): case L_('x'):
        case L_('y'): case L_('z'): case L_('{'): case L_('|'): case L_('}'):
        case L_('~'):
          /* The C Standard requires these 98 characters (plus '%') to
             be in the basic execution character set.  None of these
             characters can start a multibyte sequence, so they need
             not be analyzed further.  */
          add1 (*f);
          continue;

        default:
          /* Copy this multibyte sequence until we reach its end, find
             an error, or come back to the initial shift state.  */
          {
            mbstate_t mbstate = mbstate_zero;
            size_t len = 0;
            size_t fsize;

            if (! format_end)
              format_end = f + strlen (f) + 1;
            fsize = format_end - f;

            do
              {
                size_t bytes = mbrlen (f + len, fsize - len, &mbstate);

                if (bytes == 0)
                  break;

                if (bytes == (size_t) -2)
                  {
                    len += strlen (f + len);
                    break;
                  }

                if (bytes == (size_t) -1)
                  {
                    len++;
                    break;
                  }

                len += bytes;
              }
            while (! mbsinit (&mbstate));

            cpy (len, f);
            f += len - 1;
            continue;
          }
        }

#else /* ! DO_MULTIBYTE */

      /* Either multibyte encodings are not supported, they are
         safe for formats, so any non-'%' byte can be copied through,
         or this is the wide character version.  */
      if (*f != L_('%'))
        {
          add1 (*f);
          continue;
        }

#endif /* ! DO_MULTIBYTE */

      /* Check for flags that can modify a format.  */
      while (1)
        {
          switch (*++f)
            {
              /* This influences the number formats.  */
            case L_('_'):
            case L_('-'):
            case L_('+'):
            case L_('0'):
              pad = *f;
              continue;

              /* This changes textual output.  */
            case L_('^'):
              to_uppcase = true;
              continue;
            case L_('#'):
              change_case = true;
              continue;

            default:
              break;
            }
          break;
        }

      if (ISDIGIT (*f))
        {
          width = 0;
          do
            {
              if (INT_MULTIPLY_WRAPV (width, 10, &width)
                  || INT_ADD_WRAPV (width, *f - L_('0'), &width))
                width = INT_MAX;
              ++f;
            }
          while (ISDIGIT (*f));
        }

      /* Check for modifiers.  */
      switch (*f)
        {
        case L_('E'):
        case L_('O'):
          modifier = *f++;
          break;

        default:
          modifier = 0;
          break;
        }

      /* Now do the specified format.  */
      format_char = *f;
      switch (format_char)
        {
#define DO_NUMBER(d, v) \
          do                                                                  \
            {                                                                 \
              digits = d;                                                     \
              number_value = v;                                               \
              goto do_number;                                                 \
            }                                                                 \
          while (0)
#define DO_SIGNED_NUMBER(d, negative, v) \
          DO_MAYBE_SIGNED_NUMBER (d, negative, v, do_signed_number)
#define DO_YEARISH(d, negative, v) \
          DO_MAYBE_SIGNED_NUMBER (d, negative, v, do_yearish)
#define DO_MAYBE_SIGNED_NUMBER(d, negative, v, label) \
          do                                                                  \
            {                                                                 \
              digits = d;                                                     \
              negative_number = negative;                                     \
              u_number_value = v;                                             \
              goto label;                                                     \
            }                                                                 \
          while (0)

          /* The mask is not what you might think.
             When the ordinal i'th bit is set, insert a colon
             before the i'th digit of the time zone representation.  */
#define DO_TZ_OFFSET(d, mask, v) \
          do                                                                  \
            {                                                                 \
              digits = d;                                                     \
              tz_colon_mask = mask;                                           \
              u_number_value = v;                                             \
              goto do_tz_offset;                                              \
            }                                                                 \
          while (0)
#define DO_NUMBER_SPACEPAD(d, v) \
          do                                                                  \
            {                                                                 \
              digits = d;                                                     \
              number_value = v;                                               \
              goto do_number_spacepad;                                        \
            }                                                                 \
          while (0)

        case L_('%'):
          if (modifier != 0)
            goto bad_format;
          add1 (*f);
          break;

        case L_('a'):
          if (modifier != 0)
            goto bad_format;
          if (change_case)
            {
              to_uppcase = true;
              to_lowcase = false;
            }
#ifdef _NL_CURRENT
          cpy (aw_len, a_wkday);
          break;
#else
          goto underlying_strftime;
#endif

        case 'A':
          if (modifier != 0)
            goto bad_format;
          if (change_case)
            {
              to_uppcase = true;
              to_lowcase = false;
            }
#ifdef _NL_CURRENT
          cpy (STRLEN (f_wkday), f_wkday);
          break;
#else
          goto underlying_strftime;
#endif

        case L_('b'):
        case L_('h'):
          if (change_case)
            {
              to_uppcase = true;
              to_lowcase = false;
            }
          if (modifier == L_('E'))
            goto bad_format;
#ifdef _NL_CURRENT
          if (modifier == L_('O'))
            cpy (aam_len, a_altmonth);
          else
            cpy (am_len, a_month);
          break;
#else
          goto underlying_strftime;
#endif

        case L_('B'):
          if (modifier == L_('E'))
            goto bad_format;
          if (change_case)
            {
              to_uppcase = true;
              to_lowcase = false;
            }
#ifdef _NL_CURRENT
          if (modifier == L_('O'))
            cpy (STRLEN (f_altmonth), f_altmonth);
          else
            cpy (STRLEN (f_month), f_month);
          break;
#else
          goto underlying_strftime;
#endif

        case L_('c'):
          if (modifier == L_('O'))
            goto bad_format;
#ifdef _NL_CURRENT
          if (! (modifier == L_('E')
                 && (*(subfmt =
                       (const CHAR_T *) _NL_CURRENT (LC_TIME,
                                                     NLW(ERA_D_T_FMT)))
                     != '\0')))
            subfmt = (const CHAR_T *) _NL_CURRENT (LC_TIME, NLW(D_T_FMT));
#else
          goto underlying_strftime;
#endif

        subformat:
          subwidth = -1;
        subformat_width:
          {
            size_t len = __strftime_internal (NULL, STRFTIME_ARG ((size_t) -1)
                                              subfmt, tp, to_uppcase,
                                              pad, subwidth, tzset_called
                                              extra_args LOCALE_ARG);
            add (len, __strftime_internal (p,
                                           STRFTIME_ARG (maxsize - i)
                                           subfmt, tp, to_uppcase,
                                           pad, subwidth, tzset_called
                                           extra_args LOCALE_ARG));
          }
          break;

#if !(defined _NL_CURRENT && HAVE_STRUCT_ERA_ENTRY)
        underlying_strftime:
          {
            /* The relevant information is available only via the
               underlying strftime implementation, so use that.  */
            char ufmt[5];
            char *u = ufmt;
            char ubuf[1024]; /* enough for any single format in practice */
            size_t len;
            /* Make sure we're calling the actual underlying strftime.
               In some cases, config.h contains something like
               "#define strftime rpl_strftime".  */
# ifdef strftime
#  undef strftime
            size_t strftime ();
# endif

            /* The space helps distinguish strftime failure from empty
               output.  */
            *u++ = ' ';
            *u++ = '%';
            if (modifier != 0)
              *u++ = modifier;
            *u++ = format_char;
            *u = '\0';
            len = strftime (ubuf, sizeof ubuf, ufmt, tp);
            if (len != 0)
              cpy (len - 1, ubuf + 1);
          }
          break;
#endif

        case L_('C'):
          if (modifier == L_('E'))
            {
#if HAVE_STRUCT_ERA_ENTRY
              struct era_entry *era = _nl_get_era_entry (tp HELPER_LOCALE_ARG);
              if (era)
                {
# ifdef COMPILE_WIDE
                  size_t len = __wcslen (era->era_wname);
                  cpy (len, era->era_wname);
# else
                  size_t len = strlen (era->era_name);
                  cpy (len, era->era_name);
# endif
                  break;
                }
#else
              goto underlying_strftime;
#endif
            }

          {
            bool negative_year = tp->tm_year < - TM_YEAR_BASE;
            bool zero_thru_1899 = !negative_year & (tp->tm_year < 0);
            int century = ((tp->tm_year - 99 * zero_thru_1899) / 100
                           + TM_YEAR_BASE / 100);
            DO_YEARISH (2, negative_year, century);
          }

        case L_('x'):
          if (modifier == L_('O'))
            goto bad_format;
#ifdef _NL_CURRENT
          if (! (modifier == L_('E')
                 && (*(subfmt =
                       (const CHAR_T *) _NL_CURRENT (LC_TIME, NLW(ERA_D_FMT)))
                     != L_('\0'))))
            subfmt = (const CHAR_T *) _NL_CURRENT (LC_TIME, NLW(D_FMT));
          goto subformat;
#else
          goto underlying_strftime;
#endif
        case L_('D'):
          if (modifier != 0)
            goto bad_format;
          subfmt = L_("%m/%d/%y");
          goto subformat;

        case L_('d'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, tp->tm_mday);

        case L_('e'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER_SPACEPAD (2, tp->tm_mday);

          /* All numeric formats set DIGITS and NUMBER_VALUE (or U_NUMBER_VALUE)
             and then jump to one of these labels.  */

        do_tz_offset:
          always_output_a_sign = true;
          goto do_number_body;

        do_yearish:
          if (pad == 0)
            pad = yr_spec;
          always_output_a_sign
            = (pad == L_('+')
               && ((digits == 2 ? 99 : 9999) < u_number_value
                   || digits < width));
          goto do_maybe_signed_number;

        do_number_spacepad:
          if (pad == 0)
            pad = L_('_');

        do_number:
          /* Format NUMBER_VALUE according to the MODIFIER flag.  */
          negative_number = number_value < 0;
          u_number_value = number_value;

        do_signed_number:
          always_output_a_sign = false;

        do_maybe_signed_number:
          tz_colon_mask = 0;

        do_number_body:
          /* Format U_NUMBER_VALUE according to the MODIFIER flag.
             NEGATIVE_NUMBER is nonzero if the original number was
             negative; in this case it was converted directly to
             unsigned int (i.e., modulo (UINT_MAX + 1)) without
             negating it.  */
          if (modifier == L_('O') && !negative_number)
            {
#ifdef _NL_CURRENT
              /* Get the locale specific alternate representation of
                 the number.  If none exist NULL is returned.  */
              const CHAR_T *cp = nl_get_alt_digit (u_number_value
                                                   HELPER_LOCALE_ARG);

              if (cp != NULL)
                {
                  size_t digitlen = STRLEN (cp);
                  if (digitlen != 0)
                    {
                      cpy (digitlen, cp);
                      break;
                    }
                }
#else
              goto underlying_strftime;
#endif
            }

          bufp = buf + sizeof (buf) / sizeof (buf[0]);

          if (negative_number)
            u_number_value = - u_number_value;

          do
            {
              if (tz_colon_mask & 1)
                *--bufp = ':';
              tz_colon_mask >>= 1;
              *--bufp = u_number_value % 10 + L_('0');
              u_number_value /= 10;
            }
          while (u_number_value != 0 || tz_colon_mask != 0);

        do_number_sign_and_padding:
          if (pad == 0)
            pad = L_('0');
          if (width < 0)
            width = digits;

          {
            CHAR_T sign_char = (negative_number ? L_('-')
                                : always_output_a_sign ? L_('+')
                                : 0);
            int numlen = buf + sizeof buf / sizeof buf[0] - bufp;
            int shortage = width - !!sign_char - numlen;
            int padding = pad == L_('-') || shortage <= 0 ? 0 : shortage;

            if (sign_char)
              {
                if (pad == L_('_'))
                  {
                    if (p)
                      memset_space (p, padding);
                    i += padding;
                    width -= padding;
                  }
                width_add1 (0, sign_char);
                width--;
              }

            cpy (numlen, bufp);
          }
          break;

        case L_('F'):
          if (modifier != 0)
            goto bad_format;
          if (pad == 0 && width < 0)
            {
              pad = L_('+');
              subwidth = 4;
            }
          else
            {
              subwidth = width - 6;
              if (subwidth < 0)
                subwidth = 0;
            }
          subfmt = L_("%Y-%m-%d");
          goto subformat_width;

        case L_('H'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, tp->tm_hour);

        case L_('I'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, hour12);

        case L_('k'):           /* GNU extension.  */
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER_SPACEPAD (2, tp->tm_hour);

        case L_('l'):           /* GNU extension.  */
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER_SPACEPAD (2, hour12);

        case L_('j'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_SIGNED_NUMBER (3, tp->tm_yday < -1, tp->tm_yday + 1U);

        case L_('M'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, tp->tm_min);

        case L_('m'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_SIGNED_NUMBER (2, tp->tm_mon < -1, tp->tm_mon + 1U);

#ifndef _LIBC
        case L_('N'):           /* GNU extension.  */
          if (modifier == L_('E'))
            goto bad_format;
          {
            int n = ns, ns_digits = 9;
            if (width <= 0)
              width = ns_digits;
            int ndigs = ns_digits;
            while (width < ndigs || (1 < ndigs && n % 10 == 0))
              ndigs--, n /= 10;
            for (int j = ndigs; 0 < j; j--)
              buf[j - 1] = n % 10 + L_('0'), n /= 10;
            if (!pad)
              pad = L_('0');
            width_cpy (0, ndigs, buf);
            width_add (width - ndigs, 0, (void) 0);
          }
          break;
#endif

        case L_('n'):
          add1 (L_('\n'));
          break;

        case L_('P'):
          to_lowcase = true;
#ifndef _NL_CURRENT
          format_char = L_('p');
#endif
          FALLTHROUGH;
        case L_('p'):
          if (change_case)
            {
              to_uppcase = false;
              to_lowcase = true;
            }
#ifdef _NL_CURRENT
          cpy (ap_len, ampm);
          break;
#else
          goto underlying_strftime;
#endif

        case L_('q'):           /* GNU extension.  */
          DO_SIGNED_NUMBER (1, false, ((tp->tm_mon * 11) >> 5) + 1);
          break;

        case L_('R'):
          subfmt = L_("%H:%M");
          goto subformat;

        case L_('r'):
#ifdef _NL_CURRENT
          if (*(subfmt = (const CHAR_T *) _NL_CURRENT (LC_TIME,
                                                       NLW(T_FMT_AMPM)))
              == L_('\0'))
            subfmt = L_("%I:%M:%S %p");
          goto subformat;
#else
          goto underlying_strftime;
#endif

        case L_('S'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, tp->tm_sec);

        case L_('s'):           /* GNU extension.  */
          {
            struct tm ltm;
            time_t t;

            ltm = *tp;
            t = mktime_z (tz, &ltm);

            /* Generate string value for T using time_t arithmetic;
               this works even if sizeof (long) < sizeof (time_t).  */

            bufp = buf + sizeof (buf) / sizeof (buf[0]);
            negative_number = t < 0;

            do
              {
                int d = t % 10;
                t /= 10;
                *--bufp = (negative_number ? -d : d) + L_('0');
              }
            while (t != 0);

            digits = 1;
            always_output_a_sign = false;
            goto do_number_sign_and_padding;
          }

        case L_('X'):
          if (modifier == L_('O'))
            goto bad_format;
#ifdef _NL_CURRENT
          if (! (modifier == L_('E')
                 && (*(subfmt =
                       (const CHAR_T *) _NL_CURRENT (LC_TIME, NLW(ERA_T_FMT)))
                     != L_('\0'))))
            subfmt = (const CHAR_T *) _NL_CURRENT (LC_TIME, NLW(T_FMT));
          goto subformat;
#else
          goto underlying_strftime;
#endif
        case L_('T'):
          subfmt = L_("%H:%M:%S");
          goto subformat;

        case L_('t'):
          add1 (L_('\t'));
          break;

        case L_('u'):
          DO_NUMBER (1, (tp->tm_wday - 1 + 7) % 7 + 1);

        case L_('U'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, (tp->tm_yday - tp->tm_wday + 7) / 7);

        case L_('V'):
        case L_('g'):
        case L_('G'):
          if (modifier == L_('E'))
            goto bad_format;
          {
            /* YEAR is a leap year if and only if (tp->tm_year + TM_YEAR_BASE)
               is a leap year, except that YEAR and YEAR - 1 both work
               correctly even when (tp->tm_year + TM_YEAR_BASE) would
               overflow.  */
            int year = (tp->tm_year
                        + (tp->tm_year < 0
                           ? TM_YEAR_BASE % 400
                           : TM_YEAR_BASE % 400 - 400));
            int year_adjust = 0;
            int days = iso_week_days (tp->tm_yday, tp->tm_wday);

            if (days < 0)
              {
                /* This ISO week belongs to the previous year.  */
                year_adjust = -1;
                days = iso_week_days (tp->tm_yday + (365 + __isleap (year - 1)),
                                      tp->tm_wday);
              }
            else
              {
                int d = iso_week_days (tp->tm_yday - (365 + __isleap (year)),
                                       tp->tm_wday);
                if (0 <= d)
                  {
                    /* This ISO week belongs to the next year.  */
                    year_adjust = 1;
                    days = d;
                  }
              }

            switch (*f)
              {
              case L_('g'):
                {
                  int yy = (tp->tm_year % 100 + year_adjust) % 100;
                  DO_YEARISH (2, false,
                              (0 <= yy
                               ? yy
                               : tp->tm_year < -TM_YEAR_BASE - year_adjust
                               ? -yy
                               : yy + 100));
                }

              case L_('G'):
                DO_YEARISH (4, tp->tm_year < -TM_YEAR_BASE - year_adjust,
                            (tp->tm_year + (unsigned int) TM_YEAR_BASE
                             + year_adjust));

              default:
                DO_NUMBER (2, days / 7 + 1);
              }
          }

        case L_('W'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (2, (tp->tm_yday - (tp->tm_wday - 1 + 7) % 7 + 7) / 7);

        case L_('w'):
          if (modifier == L_('E'))
            goto bad_format;

          DO_NUMBER (1, tp->tm_wday);

        case L_('Y'):
          if (modifier == L_('E'))
            {
#if HAVE_STRUCT_ERA_ENTRY
              struct era_entry *era = _nl_get_era_entry (tp HELPER_LOCALE_ARG);
              if (era)
                {
# ifdef COMPILE_WIDE
                  subfmt = era->era_wformat;
# else
                  subfmt = era->era_format;
# endif
                  if (pad == 0)
                    pad = yr_spec;
                  goto subformat;
                }
#else
              goto underlying_strftime;
#endif
            }
          if (modifier == L_('O'))
            goto bad_format;

          DO_YEARISH (4, tp->tm_year < -TM_YEAR_BASE,
                      tp->tm_year + (unsigned int) TM_YEAR_BASE);

        case L_('y'):
          if (modifier == L_('E'))
            {
#if HAVE_STRUCT_ERA_ENTRY
              struct era_entry *era = _nl_get_era_entry (tp HELPER_LOCALE_ARG);
              if (era)
                {
                  int delta = tp->tm_year - era->start_date[0];
                  if (pad == 0)
                    pad = yr_spec;
                  DO_NUMBER (2, (era->offset
                                 + delta * era->absolute_direction));
                }
#else
              goto underlying_strftime;
#endif
            }

          {
            int yy = tp->tm_year % 100;
            if (yy < 0)
              yy = tp->tm_year < - TM_YEAR_BASE ? -yy : yy + 100;
            DO_YEARISH (2, false, yy);
          }

        case L_('Z'):
          if (change_case)
            {
              to_uppcase = false;
              to_lowcase = true;
            }

#ifdef COMPILE_WIDE
          {
            /* The zone string is always given in multibyte form.  We have
               to transform it first.  */
            wchar_t *wczone;
            size_t len;
            widen (zone, wczone, len);
            cpy (len, wczone);
          }
#else
          cpy (strlen (zone), zone);
#endif
          break;

        case L_(':'):
          /* :, ::, and ::: are valid only just before 'z'.
             :::: etc. are rejected later.  */
          for (colons = 1; f[colons] == L_(':'); colons++)
            continue;
          if (f[colons] != L_('z'))
            goto bad_format;
          f += colons;
          goto do_z_conversion;

        case L_('z'):
          colons = 0;

        do_z_conversion:
          if (tp->tm_isdst < 0)
            break;

          {
            int diff;
            int hour_diff;
            int min_diff;
            int sec_diff;
#if HAVE_TM_GMTOFF
            diff = tp->tm_gmtoff;
#else
            if (!tz)
              diff = 0;
            else
              {
                struct tm gtm;
                struct tm ltm;
                time_t lt;

                /* POSIX.1 requires that local time zone information be used as
                   though strftime called tzset.  */
# if HAVE_TZSET
                if (!*tzset_called)
                  {
                    tzset ();
                    *tzset_called = true;
                  }
# endif

                ltm = *tp;
                ltm.tm_wday = -1;
                lt = mktime_z (tz, &ltm);
                if (ltm.tm_wday < 0 || ! localtime_rz (0, &lt, &gtm))
                  break;
                diff = tm_diff (&ltm, &gtm);
              }
#endif

            negative_number = diff < 0 || (diff == 0 && *zone == '-');
            hour_diff = diff / 60 / 60;
            min_diff = diff / 60 % 60;
            sec_diff = diff % 60;

            switch (colons)
              {
              case 0: /* +hhmm */
                DO_TZ_OFFSET (5, 0, hour_diff * 100 + min_diff);

              case 1: tz_hh_mm: /* +hh:mm */
                DO_TZ_OFFSET (6, 04, hour_diff * 100 + min_diff);

              case 2: tz_hh_mm_ss: /* +hh:mm:ss */
                DO_TZ_OFFSET (9, 024,
                              hour_diff * 10000 + min_diff * 100 + sec_diff);

              case 3: /* +hh if possible, else +hh:mm, else +hh:mm:ss */
                if (sec_diff != 0)
                  goto tz_hh_mm_ss;
                if (min_diff != 0)
                  goto tz_hh_mm;
                DO_TZ_OFFSET (3, 0, hour_diff);

              default:
                goto bad_format;
              }
          }

        case L_('\0'):          /* GNU extension: % at end of format.  */
            --f;
            FALLTHROUGH;
        default:
          /* Unknown format; output the format, including the '%',
             since this is most likely the right thing to do if a
             multibyte string has been misparsed.  */
        bad_format:
          {
            int flen;
            for (flen = 1; f[1 - flen] != L_('%'); flen++)
              continue;
            cpy (flen, &f[1 - flen]);
          }
          break;
        }
    }

#if ! FPRINTFTIME
  if (p && maxsize != 0)
    *p = L_('\0');
#endif

  return i;
}
