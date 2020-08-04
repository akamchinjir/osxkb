#ifndef OSX_KB_OUT_H
#define OSX_KB_OUT_H

#include "common.h"

typedef struct _Out Out;

struct _Out
{
  char *path;
  char *temp_path;
  FILE *out;
};

Out *out_open (const char *path, GError **error);
bool out_close (Out *out, GError **error);

bool out_putc (Out *out, GError **error, char c);
bool out_print (Out *out, GError **error, const char *str);
bool out_printf (Out *out, GError **error, const char *format, ...) G_GNUC_PRINTF(3, 4);

#endif
