#include "common.h"
#include <stdlib.h>
#include <string.h>
#include "bundle.h"

static void print_help (const char *program) G_GNUC_NORETURN;
static void print_version (void) G_GNUC_NORETURN;

void
print_help (const char *program_path)
{
  GFile *file = g_file_new_for_commandline_arg (program_path);
  char *basename = g_file_get_basename (file);
  g_object_unref (file);

  fprintf (stderr,
           "Usage: %s CONFIG_FILE\n"
           "Options:\n"
           "  --help, -h, -?                 print this help and exit\n"
           "  --version, -v                  print version information and exit\n"
           "Creates an OSX bundle defining one or more keyboard layouts.\n"
           "All configuration must go in CONFIG_FILE, see the package documentation\n"
           "for details.\n",
           basename);

  g_free (basename);
  exit (0);
}

void
print_version ()
{
  printf (PACKAGE_STRING "\n"
          "Copyright (c) 2020 Akam Chinjir\n"
          "License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.\n"
          "This is free software: you are free to change and redistribute it.\n"
          "There is NO WARRANTY, to the extent permitted by law.\n");
  exit (0);
}

int
main (int argc, char **argv)
{
  GError *error = NULL;

#if !GLIB_CHECK_VERSION(2,35,0)
  g_type_init ();
#endif

  const char *config_file = NULL;
  for (int idx = 1; idx < argc; ++idx)
  {
    const char *arg = argv[idx];
    if (*arg == '-')
    {
      if (arg[1] == '-')
      {
        arg += 2;
        if (strcmp (arg, "help") == 0)
        {
          print_help (argv[0]);
        }
        else if (strcmp (arg, "version") == 0)
        {
          print_version ();
        }
        else
        {
          make_error (&error, "Unknown option: %s", argv[idx]);
          goto on_error;
        }
      }
      else
      {
        if (arg[1] == 'h' || arg[1] == '?')
        {
          print_help (argv[0]);
        }
        else if (arg[1] == 'v')
        {
          print_version ();
        }
        else
        {
          make_error (&error, "Unknown option: %s", argv[idx]);
          goto on_error;
        }
      }
    }
    else
    {
      if (config_file)
      {
        make_error (&error, "Multiple configuration files requested");
        goto on_error;
      }
      config_file = arg;
    }
  }

  if (config_file == NULL)
  {
    make_error (&error, "No configuration file requested");
    goto on_error;
  }

  Bundle *bundle = bundle_new (config_file, &error);
  if (bundle == NULL
      || !bundle_load_data (bundle, &error)
      || !bundle_write_bundle (bundle, &error))
  {
    goto on_error;
  }

  exit (0);

 on_error:

  g_assert (error != NULL);

  fprintf (stderr, "Error: %s\n", error->message);
  exit (error->code);
}
