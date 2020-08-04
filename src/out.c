#include "out.h"
#include <glib/gstdio.h>
#include "util.h"

/**
 * Public procedures
 */

Out *
out_open (const char *path, GError **error)
{
  Out *out = g_slice_alloc (sizeof (Out));

  out->path = g_strdup (path);
  int count = 0;
  out->temp_path = g_strconcat (path, ".TMP", NULL);
  while (util_file_exists (out->temp_path))
  {
    ++count;
    char *temp = g_strdup_printf ("%s.TMP.%02d", path, count);
    g_free (out->temp_path);
    out->temp_path = temp;
  }

  out->out = fopen (out->temp_path, "w");
  if (out->out == NULL)
  {
    make_system_error (error, "Could not open %s for writing", out->temp_path);
    return NULL;
  }

  return out;
}

bool
out_close (Out *out, GError **error)
{
  g_assert (error != NULL);

  if (fclose (out->out) != 0)
    return make_system_error (error, "Could not close %s", out->temp_path);

  if (*error == NULL)
  {
    if (!util_move_file (out->temp_path, out->path, error))
      return false;
  }

  g_free (out->path);
  g_free (out->temp_path);
  g_slice_free1 (sizeof (Out), out);

  return true;
}

bool
out_putc (Out *out, GError **error, char c)
{
  if (fputc (c, out->out) == EOF)
    return make_system_error (error, "Write failed (%s)", out->temp_path);
  else
    return true;
}

bool
out_print (Out *out, GError **error, const char *str)
{
  if (fputs (str, out->out) == EOF)
    return make_system_error (error, "Write failed (%s)", out->temp_path);
  else
    return true;
}

bool
out_printf (Out *out, GError **error, const char *format, ...)
{
  va_list args;
  va_start (args, format);
  char *output;
  size_t len = (size_t)g_vasprintf (&output, format, args); // docs say this returns a number of bytes, must be positive
  va_end (args);

  if (fwrite (output, 1, len, out->out) != len)
    make_system_error (error, "Write failed (%s)", out->temp_path);
  
  g_free (output);
  
  return *error == NULL;
}


