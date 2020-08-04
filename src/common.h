#ifndef OSX_KB_COMMON_H
#define OSX_KB_COMMON_H

#if HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdbool.h>
#include <stdio.h>
#include <glib.h>
#include <gio/gio.h>

#include "error.h"

#if HAVE_STPCPY
#else
static inline char *
my_stpcpy (char *dest, const char *src)
{
  char *out = dest;
  const char *ptr = src;
  while (*ptr)
    *out++ = *ptr++;
  return out;
}
#define stpcpy my_stpcpy
#endif

#endif

