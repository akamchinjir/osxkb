#include "error.h"
#include <errno.h>
#include <string.h>

/**
 * Public procedures
 */

bool
make_error (GError **error, const char *format, ...)
{
  g_assert (error != NULL);
  
  va_list args;
  va_start (args, format);

  char *message = g_strdup_vprintf (format, args);

  va_end (args);

  if (*error)
    g_critical ("Multiple errors: %s", message);
  else
    *error = g_error_new (g_quark_from_static_string ("error"), 1, "%s", message);

  g_free (message);

  return false;
}

bool
make_system_error (GError **error, const char *format, ...)
{
  g_assert (error != NULL);

  int code = errno;

  va_list args;
  va_start (args, format);

  char *message = g_strdup_vprintf (format, args);

  va_end (args);

  if (*error)
    g_critical ("Multiple errors: %s: %s", message, strerror (code));
  else
    *error = g_error_new (g_quark_from_static_string ("error"), code, "%s: %s", message, strerror (code));

  g_free (message);

  return false;
}

bool
suffix_error (GError **error, const char *format, ...)
{
  g_assert (error != NULL && *error != NULL);
  
  va_list args;
  va_start (args, format);

  char *message = g_strdup_vprintf (format, args);

  va_end (args);

  char *temp = g_strconcat ((*error)->message, ": ", message, NULL);
  g_free (message);
  g_free ((*error)->message);
  (*error)->message = temp;

  return false;
}
