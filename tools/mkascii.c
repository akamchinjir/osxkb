#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>

typedef struct _Ascii Ascii;
typedef bool (*Handler) (GList *, int, GError **);

static GQuark domain = 0;

static GTree *name_to_ascii = NULL;
static GTree *ascii_to_name = NULL;
static GTree *ascii_to_output = NULL;

static const char *ctrl_codes[128] = { NULL, };

struct _Ascii
{
  char *display;
  int sort;
};

static bool bad_elements (int found, int expected, GError **error);

static int mkcode (const char *str, GError **error);
static bool careful_insert (GTree *tree, char *name, void *value, GError **error);

static Ascii *ascii_new (int code);
static int asciis_compare (Ascii *lhs, Ascii *rhs);

static bool handle_nonprinting (GList *elts, int n_elts, GError **error);
static bool handle_xmlspecial (GList *elts, int n_elts, GError **error);
static bool handle_otherspecial (GList *elts, int n_elts, GError **error);
static bool handle_control_code (GList *elts, int n_elts, GError **error);

static gboolean map_name_to_ascii (char *name, Ascii *ascii);
static gboolean map_ascii_to_name (Ascii *ascii, char *name, char **lookup);
static gboolean map_ascii_to_output (Ascii *ascii, char *output, char **lookup);

static void print_ascii_lookup (const char *name, const char **lookup);

bool
bad_elements (int found, int expected, GError **error)
{
  *error = g_error_new (domain, 0, "wrong number of elements, expected %d, found %d", expected, found);
  return false;
}

int
mkcode (const char *str, GError **error)
{
  int code = 0;
  const char *ptr = str;
  while (isdigit (*ptr))
  {
    code *= 10;
    code += (*ptr - '0');
    ++ptr;
  }

  if (ptr == str || *ptr != '\0')
  {
    *error = g_error_new (domain, 0, "expected decimal number, found `%s'", str);
    return -1;
  }

  return code;
}

bool
careful_insert (GTree *tree, char *name, void *value, GError **error)
{
  if (g_tree_lookup (tree, name) != NULL)
  {
    *error = g_error_new (domain, 0, "duplicate name: %s", name);
    return false;
  }

  g_tree_insert (tree, name, value);
  return true;
}

Ascii *
ascii_new (int code)
{
  Ascii *ascii = g_slice_alloc (sizeof (Ascii));

  ascii->sort = code;
  if (ascii->sort == '\'')
  {
    ascii->display = g_strdup ("\\\'");
  }
  else if (isprint (ascii->sort))
  {
    ascii->display = g_malloc (2);
    ascii->display[0] = (char)code;
    ascii->display[1] = '\0';
  }
  else
  {
    ascii->display = g_strdup_printf ("\\x%02x", code);
  }

  return ascii;
}

int
asciis_compare (Ascii *lhs, Ascii *rhs)
{
  return lhs->sort - rhs->sort;
}

bool
handle_nonprinting (GList *elts, int n_elts, GError **error)
{
  if (n_elts != 3)
    return bad_elements (n_elts, 3, error);

  int code = mkcode (elts->data, error);
  if (code == -1)
    return false;

  Ascii *ascii = ascii_new (code);
  char *name = elts->next->data;
  char *output = elts->next->next->data;

  if (!careful_insert (name_to_ascii, name, ascii, error))
    return false;

  g_tree_insert (ascii_to_name, ascii, name);
  g_tree_insert (ascii_to_output, ascii, output);

  return true;
}

bool
handle_xmlspecial (GList *elts, int n_elts, GError **error)
{
  return handle_nonprinting (elts, n_elts, error);
}

bool
handle_otherspecial (GList *elts, int n_elts, GError **error)
{
  if (n_elts != 2)
    return bad_elements (n_elts, 2, error);

  int code = mkcode (elts->data, error);
  if (code == -1)
    return false;

  Ascii *ascii = ascii_new (code);
  char *name = elts->next->data;

  if (!careful_insert (name_to_ascii, name, ascii, error))
    return false;

  if (g_tree_lookup (ascii_to_name, ascii) == NULL) // allow otherspecials to include duplicate
    g_tree_insert (ascii_to_name, ascii, name);
  
  return true;
}

bool
handle_control_code (GList *elts, int n_elts, GError **error)
{
  if (n_elts != 2)
    return bad_elements (n_elts, 2, error);

  char *ascii_str = elts->data;
  char *ctrl_code = elts->next->data;

  Ascii *ascii = g_tree_lookup (name_to_ascii, ctrl_code);
  if (ascii == NULL)
  {
    *error = g_error_new (domain, 0, "unknown code %s", ctrl_code);
    return false;
  }

  const char *output = g_tree_lookup (ascii_to_output, ascii);
  g_assert (output != NULL);

  ctrl_codes[(int)*ascii_str] = output;

  return true;
}

gboolean
map_name_to_ascii (char *name, Ascii *ascii)
{
  printf ("  { \"%s\", '%s' },\n", name, ascii->display);
  return FALSE;
}

gboolean
map_ascii_to_name (Ascii *ascii, char *name, char **lookup)
{
  lookup[ascii->sort] = name;
  return FALSE;
}

gboolean
map_ascii_to_output (Ascii *ascii, char *output, char **lookup)
{
  lookup[ascii->sort] = output;
  return FALSE;
}

void
print_ascii_lookup (const char *name, const char **lookup)
{
  printf ("static const char *%s[128] = { ", name);
  for (int idx = 0; idx < 128; ++idx)
  {
    const char *value = lookup[idx];
    if (value == NULL)
      printf (" NULL");
    else
      printf (" \"%s\"", value);

    if (idx < 127)
      printf (",");
  }
  printf (" };\n");
}

int
main (int argc, char **argv)
{
  domain = g_quark_from_static_string ("error");

  name_to_ascii = g_tree_new ((GCompareFunc)strcmp);
  ascii_to_name = g_tree_new ((GCompareFunc)asciis_compare);
  ascii_to_output = g_tree_new ((GCompareFunc)asciis_compare);
  
  if (argc != 2)
  {
    fprintf (stderr, "%s: bad command line\n", argv[0]);
    return 1;
  }

  GError *error = NULL;
  const char *path = argv[1];
  char *contents;
  if (!g_file_get_contents (path, &contents, NULL, &error))
  {
    fprintf (stderr, "%s: %s\n", argv[0], error->message);
    return 1;
  }

  Handler handler = NULL;
  int lineno = 1;
  char *ptr = contents;
  while (true)
  {
    while (isspace (*ptr))
    {
      if (*ptr == '\n')
        ++lineno;
      ++ptr;
    }

    if (*ptr == '\0')
      break;

    char *next;
    char *end = strchr (ptr, '\n');
    if (end)
    {
      *end = '\0';
      next = end + 1;
    }
    else
    {
      end = strchr (ptr, '\0');
      next = NULL;
    }

    if (*ptr == '[' && *(end - 1) == ']')
    {
      ++ptr;
      *(end - 1) = '\0';

      if (strcmp (ptr, "nonprinting") == 0)
      {
        handler = handle_nonprinting;
      }
      else if (strcmp (ptr, "xmlspecial") == 0)
      {
        handler = handle_xmlspecial;
      }
      else if (strcmp (ptr, "otherspecial") == 0)
      {
        handler = handle_otherspecial;
      }
      else if (strcmp (ptr, "control") == 0)
      {
        handler = handle_control_code;
      }
      else
      {
        fprintf (stderr, "%s: unknown header: %s (%s, line %d)\n", argv[0], ptr, path, lineno);
        return 1;
      }
    }
    else
    {
      if (handler == NULL)
      {
        fprintf (stderr, "%s: handler not set (%s, line %d)\n", argv[0], path, lineno);
        return 1;
      }

      GList *elts = NULL;
      int n_elts = 0;

      while (true)
      {
        elts = g_list_append (elts, ptr);
        ++n_elts;
        ptr = strchr (ptr, ' ');
        if (ptr == NULL)
          break;

        *ptr = '\0';
        ++ptr;
      }

      if (!handler (elts, n_elts, &error))
      {
        fprintf (stderr, "%s: %s (%s, line %d)\n", argv[0], error->message, path, lineno);
        return 1;
      }
    }

    if (next == NULL)
      break;

    ptr = next;
    ++lineno;
  }

  printf ("static const AsciiLookup name_to_ascii[] = {\n");
  
  g_tree_foreach (name_to_ascii, (GTraverseFunc)map_name_to_ascii, NULL);

  printf ("};\n"
          "\n"
          "static const int N_NAMES = %d;\n\n",
          g_tree_nnodes (name_to_ascii));

  const char *name_lookup[128] = { NULL, };
  g_tree_foreach (ascii_to_name, (GTraverseFunc)map_ascii_to_name, name_lookup);

  const char *output_lookup[128] = { NULL, };
  g_tree_foreach (ascii_to_output, (GTraverseFunc)map_ascii_to_output, output_lookup);

  print_ascii_lookup ("name_lookup", name_lookup);
  print_ascii_lookup ("output_lookup", output_lookup);
  print_ascii_lookup ("control_codes", ctrl_codes);
  printf ("\n");

  return 0;  
}

