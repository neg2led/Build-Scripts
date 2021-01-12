/* Test of POSIX compatible vsprintf() function.
   Copyright (C) 2007-2020 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* Written by Bruno Haible <bruno@clisp.org>, 2007.  */

#include <config.h>

#include <stdio.h>

#include "signature.h"
SIGNATURE_CHECK (vsprintf, int, (char *, char const *, va_list));

#include <float.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"

#include "test-sprintf-posix.h"

static int
my_sprintf (char *str, const char *format, ...)
{
  va_list args;
  int ret;

  va_start (args, format);
  ret = vsprintf (str, format, args);
  va_end (args);
  return ret;
}

int
main (int argc, char *argv[])
{
  test_function (my_sprintf);
  return 0;
}
