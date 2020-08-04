#include "keyboard.h"
#include <ctype.h>
#include <string.h>
#include "data.h"
#include "out.h"
#include "util.h"

typedef struct _Point Point;
typedef struct _Literal Literal;
typedef struct _Key Key;
typedef struct _Action Action;
typedef struct _Subaction Subaction;

enum
  {
    ACTION_OUTPUT,
    ACTION_CHANGE_STATE
  };

struct _Point
{
  int shift_state;
  int code;
};

struct _Literal // badly named :(
{
  char *name; // this will be used to construct action names, it incorporates various substitutions
  char *output; // also with substitutions, not the same ones
  GList *points;
};

struct _Key
{
  int mods;
  Literal *literal;
};

struct _Action
{
  const char *name;
  GTree *subactions;
};

struct _Subaction
{
  int action_type;
  const char *target; // the output content or the next state
};

/** private procedures */

static Point *point_new (int shift_state, int code);
static Literal *literal_new (const char *literal);
static void literal_add_point (Literal *literal, int shift_state, int code);

static Key *key_new (int mods, Literal *literal);

static Action *action_new (const char *name);
static Subaction *subaction_new (int action_type, const char *target);
static void action_handle_state (Action *action, const char *state, int action_type, const char *target, Keyboard *kb);

static bool parse_input_to_literal (char *token, GError **error);

static char *parse_literal_to_name (const char *literal);
static char *parse_literal_to_output (const char *literal);

static void keyboard_set_capslock_active (Keyboard *kb);

static bool keyboard_write_actions (Keyboard *kb, Out *out, GError **error);
static bool keyboard_write_terminators (Keyboard *kb, Out *out, GError **error);

static void keyboard_set_terminator (Keyboard *kb, const char *state, const char *terminator);
static bool keyboard_parse_keys (Keyboard *kb, char *start, GList **keys, GError **error);
static bool keyboard_load_sequence (Keyboard *kb, GList *keys, const char *output, GError **error);

static bool keyboard_set_base_encoding (Keyboard *kb, const char *data_name, char *data, char *osxalt, GError **error);
static bool keyboard_load_mappings (Keyboard *kb, const char *data_name, char *data, GError **error);

/**
 * Private procedures
 */

Point *
point_new (int shift_state, int code)
{
  Point *point = g_slice_alloc (sizeof (Point));
  point->shift_state = shift_state;
  point->code = code;
  return point;
}

Literal *
literal_new (const char *literal)
{
  Literal *lit = g_slice_alloc (sizeof (Literal));
  lit->name = parse_literal_to_name (literal);
  lit->output = parse_literal_to_output (literal);
  lit->points = NULL;
  return lit;
}

void
literal_add_point (Literal *literal, int shift_state, int code)
{
  Point *point = point_new (shift_state, code);
  literal->points = g_list_append (literal->points, point);
}

Key *
key_new (int mods, Literal *literal)
{
  Key *key = g_slice_alloc (sizeof (Key));
  key->mods = mods;
  key->literal = literal;
  return key;
}

Action *
action_new (const char *name)
{
  Action *action = g_slice_alloc (sizeof (Action));
  action->name = name;
  action->subactions = g_tree_new ((GCompareFunc)strcmp);
  return action;
}

Subaction *
subaction_new (int action_type, const char *target)
{
  Subaction *sub = g_slice_alloc (sizeof (Subaction));
  sub->action_type = action_type;
  sub->target = target;
  return sub;
}

void
action_handle_state (Action *action, const char *state, int action_type, const char *target, Keyboard *kb)
{
  Subaction *subaction = g_tree_lookup (action->subactions, state);
  if (subaction)
  {
    if (subaction->action_type == action_type)
    {
      if (action_type == ACTION_OUTPUT)
        subaction->target = target;
      else
        g_assert (strcmp (subaction->target, target) == 0);
    }
    else if (action_type == ACTION_OUTPUT)
    {
      keyboard_set_terminator (kb, subaction->target, target);
    }
    else
    {
      keyboard_set_terminator (kb, target, subaction->target);
      subaction->action_type = ACTION_CHANGE_STATE;
      subaction->target = target;
    }
  }
  else
  {
    subaction = subaction_new (action_type, target);
    g_tree_insert (action->subactions, (char *)state, subaction);
  }
}

bool
parse_input_to_literal (char *token, GError **error)
{
  char *ptr = token;
  char *out = NULL;
  char *end;
  while (*ptr)
  {
    if (*ptr == '[' && (end = strchr (ptr, ']')) != NULL)
    {
      if (out == NULL)
        out = ptr;
      ++ptr;
      *end = '\0';

      char ascii = lookup_ascii (ptr);
      if (ascii == '\0')
        return make_error (error, "Unknown character name `%s'", ptr);

      *out = ascii;
      ++out;
      ptr = end + 1;
    }
    else
    {
      if (out)
      {
        *out = *ptr;
        ++out;
      }
      ++ptr;
    }
  }

  if (out)
    *out = '\0';

  return true;
}

char *
parse_literal_to_name (const char *literal)
{
  // two passes, once for length, second time for copying

  char *ret = NULL;
  size_t len = 0;
  const char *ptr = literal;
  while (*ptr)
  {
    const char *name = lookup_name (*ptr);
    if (name)
      len += (strlen (name) + 2);
    else
      ++len;
    ++ptr;
  }

  ret = g_malloc (len + 1);
  ptr = literal;
  char *out = ret;
  while (*ptr)
  {
    const char *name = lookup_name (*ptr);
    if (name)
    {
      *out++ = '[';
      const char *p = name;
      while (*p)
        *out++ = *p++;
      *out++ = ']';
      ++ptr;
    }
    else
    {
      *out++ = *ptr++;
    }
  }
  *out = '\0';
  
  return ret;
}

char *
parse_literal_to_output (const char *literal)
{
  // two passes, once for length, second time for copying

  char *ret = NULL;
  size_t len = 0;
  const char *ptr = literal;
  while (*ptr)
  {
    const char *output = lookup_output (*ptr);
    if (output)
      len += strlen (output);
    else
      ++len;
    ++ptr;
  }

  ret = g_malloc (len + 1);
  ptr = literal;
  char *out = ret;
  while (*ptr)
  {
    const char *output = lookup_output (*ptr);
    if (output)
    {
      const char *p = output;
      while (*p)
        *out++ = *p++;
      ++ptr;
    }
    else
    {
      *out++ = *ptr++;
    }
  }
  *out = '\0';
  
  return ret;
}

static gboolean
write_subaction (const char *state, Subaction *subaction, void *userdata)
{
  struct
  {
    Out *out;
    GError **error;
  } *data = userdata;

  if (strcmp (state, "none") != 0)
  {
    if (!out_printf (data->out, data->error,
                     "      <when state=\"%s\" %s=\"%s\" />\n",
                     state,
                     subaction->action_type == ACTION_OUTPUT ? "output" : "next",
                     subaction->target))
      return TRUE;
  }

  return FALSE;
}

static gboolean
write_action (const char *name, Action *action, void *userdata)
{
  struct
  {
    Out *out;
    GError **error;
  } *data = userdata;

  if (!out_printf (data->out, data->error,
                   "    <action id=\"%s\">\n",
                   name))
    return TRUE; // to stop the iteration

  Subaction *subaction = g_tree_lookup (action->subactions, "none");
  if (subaction)
  {
    if (!out_printf (data->out, data->error,
                     "      <when state=\"none\" %s=\"%s\" />\n",
                     subaction->action_type == ACTION_OUTPUT ? "output" : "next",
                     subaction->target))
      return TRUE;
  }

  g_tree_foreach (action->subactions, (GTraverseFunc)write_subaction, userdata);
  if (*data->error)
    return TRUE;

  if (!out_print (data->out, data->error, "    </action>\n"))
    return TRUE;

  return FALSE;
}

void
keyboard_set_capslock_active (Keyboard *kb)
{
  g_assert (kb->capslock_policy != CAPSLOCK_DISABLES);
  kb->active_capslock = true;
  key_map_set_set_capslock_active (kb->base_keymaps);
  key_map_set_set_capslock_active (kb->control_keymaps);
}

bool
keyboard_write_actions (Keyboard *kb, Out *out, GError **error)
{
  if (g_tree_nnodes (kb->actions) > 0)
  {
    if (!out_print (out, error, "  <actions>\n"))
      return false;

    struct
    {
      Out *out;
      GError **error;
    } data = { out, error };
    g_tree_foreach (kb->actions, (GTraverseFunc)write_action, &data);

    if (*error)
      return false;

    if (!out_print (out, error, "  </actions>\n"))
      return false;
  }

  return true;
}

static gboolean
write_terminator (const char *state, const char *output, void *userdata)
{
  struct
  {
    Out *out;
    GError **error;
  } *data = userdata;

  if (!out_printf (data->out, data->error,
                   "    <when state=\"%s\" output=\"%s\" />\n",
                   state, output))
    return TRUE;

  return FALSE;
}

bool
keyboard_write_terminators (Keyboard *kb, Out *out, GError **error)
{
  if (g_tree_nnodes (kb->terminators) > 0)
  {
    if (!out_print (out, error, "  <terminators>\n"))
      return false;

    struct
    {
      Out *out;
      GError **error;
    } data = { out, error };
    g_tree_foreach (kb->terminators, (GTraverseFunc)write_terminator, &data);

    if (*error)
      return false;

    if (!out_print (out, error, "  </terminators>\n"))
      return false;
  }

  return true;
}

void
keyboard_set_terminator (Keyboard *kb, const char *state, const char *terminator)
{
  g_tree_insert (kb->terminators, (char *)state, (char *)terminator);
}

bool
keyboard_parse_keys (Keyboard *kb, char *str, GList **keys, GError **error)
{
  /**
   * Each element can have one or two prefixed modifiers (O-, G-, C-) and
   * then the representation of a key. There can be multiple keys in
   * sequence, but the input has already been split on spaces. 
   */

  char *ptr = str;
  while (*ptr)
  {
    int mods = 0;
    // can be looking at:
    //   O- or C-
    //   a bracketed name
    //   a literal
    
    while (ptr[1] == '-'
        && (*ptr == 'O' || *ptr == 'C'))
    {
      if (*ptr == 'O')
        mods |= MOD_OPTION;
      else
        mods |= MOD_CONTROL;

      ptr += 2;
    }

    if (*ptr == '\0')
      return make_error (error, "Truncated key list");

    Literal *literal;
    char *end;
    if (*ptr == '[' && (end = strchr (ptr, ']')) != NULL)
    {
      *end = '\0';
      ++ptr;
      char c = lookup_ascii (ptr);
      if (c == '\0')
        return make_error (error, "Unknown character name `%s'", ptr);

      char s[2] = { c, '\0' };
      literal = prefix_map_get (kb->literals, s);
      if (literal == NULL)
        return make_error (error, "Unknown character `%s'", ptr);
      ptr = end + 1;
    }
    else
    {
      literal = prefix_map_get_prefix (kb->literals, (const char **)&ptr);
      if (literal == NULL)
        return make_error (error, "Unknown character `%s'", ptr);
    }

    Key *key = key_new (mods, literal);
    *keys = g_list_append (*keys, key);
  }

  return true;
}

bool
keyboard_load_sequence (Keyboard *kb, GList *keys, const char *raw_output, GError **error)
{
  static const char *MODS[4] = { NULL, "O-", "C-", "C-O-" };

  char *literal = g_strdup (raw_output);
  if (!parse_input_to_literal (literal, error))
    return false;

  char *output = parse_literal_to_output (literal);
  g_free (literal); // only so much leaking i can take, i guess

  char *inc_output = NULL;
  const char *cur_state = NULL; // leaking all memory
  const char *prev_state = "none";
  for (GList *iter = keys; iter != NULL; iter = iter->next)
  {
    Key *key = iter->data;
    const char *action_name = key->mods == 0 ? key->literal->name : g_strconcat (MODS[key->mods], key->literal->name, NULL);
    Action *action = g_tree_lookup (kb->actions, action_name);
    KeyMapSet *mapset = (key->mods & MOD_CONTROL) != 0 ? kb->control_keymaps : kb->base_keymaps;

    if (key->mods == 0)
      inc_output = inc_output ? g_strconcat (inc_output, key->literal->output, NULL) : key->literal->output;
    else
      inc_output = NULL;
    
    int new_result_type = -1;
    const char *new_result_content;

    if (iter->next == NULL && iter == keys)
    {
      if (action == NULL) // it's okay, just change the output
      {
        new_result_type = RESULT_OUTPUT;
        new_result_content = output;
      }
      else
      {
        action_handle_state (action, "none", ACTION_OUTPUT, output, kb);
      }
    }
    else
    {
      if (action == NULL)
      {
        action = action_new (action_name);
        g_tree_insert (kb->actions, (char *)action_name, action);

        Point *point = key->literal->points->data;
        g_assert (point->shift_state != CAPSLOCK);
        const Result *result = key_map_set_get_result (mapset, key->mods, point->shift_state, point->code);
        g_assert (result->result_type == RESULT_OUTPUT);

        action_handle_state (action, "none", ACTION_OUTPUT, result->content, kb);

        new_result_type = RESULT_ACTION;
        new_result_content = action_name;
      }

      if (iter->next == NULL)
      {
        action_handle_state (action, prev_state, ACTION_OUTPUT, output, kb);
      }
      else
      {
        cur_state = cur_state ? g_strconcat (cur_state, ".", action_name, NULL) : action_name;
        action_handle_state (action, prev_state, ACTION_CHANGE_STATE, cur_state, kb);
        if (inc_output)
          keyboard_set_terminator (kb, cur_state, inc_output);
        prev_state = cur_state;
      }
    }

    if (new_result_type != -1)
    {
      for (GList *i = key->literal->points; i != NULL; i = i->next)
      {
        Point *pt = i->data;

        // ignore capslock when there's a modifier, unless it actually distinguishes literal forms
        if (key->mods == 0 || pt->shift_state != CAPSLOCK || kb->active_capslock)
          key_map_set_set_result (mapset, key->mods, pt->shift_state, pt->code, new_result_type, new_result_content);
      }
    }    
  }
  return true;
}

bool
keyboard_set_base_encoding (Keyboard *kb, const char *data_name, char *data, char *osxalt, GError **error)
{
  // each line in data is: code shiftless shifty capslock
  // they must be separated by space
  // need a map for each shift state, with modifiers=0, read in from *data*
  // copy that for the anyOption state
  // if osxalt, apply those changes to the anyOption state
  // make a control map

  int lineno = 1;
  char *ptr = data;
  while (*ptr)
  {
    while (isspace (*ptr))
    {
      if (*ptr == '\n')
        ++lineno;
      ++ptr;
    }

    if (*ptr == '\0')
      break;

    bool done_line = false;
    
    char *start = ptr;
    int code = 0;
    while (isdigit (*ptr))
    {
      code *= 10;
      code += (*ptr - '0');
      ++ptr;
    }

    if (ptr == start)
      return make_error (error, "Missing numerical code: %s, line %d", data_name, lineno);
    if (code > 127)
      return make_error (error, "Key code too high: %d (max is 127): %s, line %d", code, data_name, lineno);
    if (*ptr == '\n')
      return make_error (error, "Unexpected end of line: %s, line %d", data_name, lineno);
    if (!isspace (*ptr))
      return make_error (error, "Bad delimiter (expected space): %s, line %d", *ptr, data_name, lineno);

    ++ptr;
    
    for (int shift_state = 0; shift_state < 3; ++shift_state)
    {
      while (isspace (*ptr) && *ptr != '\n')
        ++ptr;

      char *token = ptr;
      while (!isspace (*ptr) && *ptr != '\n' && *ptr != '\0')
        ++ptr;

      if (shift_state != 2)
      {
        if (*ptr == '\n')
          return make_error (error, "Unexpected end of line: %s, line %d", data_name, lineno);
        if (*ptr == '\0')
          return make_error (error, "Unexpected end of file: %s, line %d", data_name, lineno);
      }

      if (*ptr != '\0')
      {
        if (*ptr == '\n')
        {
          ++lineno;
          done_line = true;
        }
        *ptr = '\0';
        ++ptr;
      }

      if (shift_state != 2 || kb->capslock_policy != CAPSLOCK_DISABLES)
      {
        if (!parse_input_to_literal (token, error))
          return suffix_error (error, "%s, line %d", data_name, lineno);
        
        Literal **litp = (Literal **)prefix_map_lookup (kb->literals, token);
        if (*litp == NULL)
        {
          *litp = literal_new (token);
          if (shift_state == 2)
            keyboard_set_capslock_active (kb);
        }

        literal_add_point (*litp, shift_state, code);
        key_map_set_set_result (kb->base_keymaps, 0, shift_state, code, RESULT_OUTPUT, (*litp)->output);

        if (shift_state == 0)
        {
          // update control map
          const char *ctrl = lookup_ctrl_code (token);
          if (ctrl == NULL)
          {
            if (strcmp (token, "§") == 0)
              ctrl = "0"; // weird special case
            else
              ctrl = parse_literal_to_output (token);
          }

          key_map_set_set_result (kb->control_keymaps, 0, 0, code, RESULT_OUTPUT, ctrl);
        }
      }
    }

    if (!done_line)
    {
      while (*ptr != '\n' && *ptr != '\0')
        ++ptr;
    }
  }

  key_map_set_make_backup (kb->base_keymaps);
  key_map_set_make_backup (kb->control_keymaps);

  if (osxalt)
  {
    if (!keyboard_load_mappings (kb, "osxalt", osxalt, error))
      return false;
  }
  
  return true;
}

bool
keyboard_load_mappings (Keyboard *kb, const char *data_name, char *data, GError **error)
{
  int lineno = 1;
  
  char *ptr = data;
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

    char *output = ptr;
    while (!isspace (*ptr) && *ptr != '\0')
      ++ptr;

    if (ptr == output)
      return make_error (error, "Missing output (did you mean SPC?): %s, line %d", data_name, lineno);
    if (*ptr == '\n')
      return make_error (error, "Unexpected end of line: %s, line %d", data_name, lineno);
    if (*ptr == '\0')
      return make_error (error, "Unexpected end of file: %s, line %d", data_name, lineno);

    *ptr = '\0';
    ++ptr;

    while (isspace (*ptr))
    {
      if (*ptr == '\n')
        return make_error (error, "Unexpected end of line: %s, line %d", data_name, lineno);
      ++ptr;
    }

    if (*ptr == '\0')
      return make_error (error, "Unexpected end of file: %s, line %d", data_name, lineno);

    // now at the start of the key sequence to produce the output
    // and we know there's SOMETHING at least
    GList *keys = NULL;
    bool done_line = false;
    while (!done_line)
    {
      char *start = ptr;
      while (!isspace (*ptr) && *ptr != '\0')
        ++ptr;

      if (*ptr != '\0')
      {
        char *end = ptr;
        while (isspace (*ptr))
        {
          if (*ptr == '\n')
          {
            done_line = true;
            ++lineno;
            ++ptr;
            break;
          }
          ++ptr;
        }
        *end = '\0';

      }

      if (!keyboard_parse_keys (kb, start, &keys, error))
        return suffix_error (error, "%s, line %d", data_name, lineno);
    }

    if (!keyboard_load_sequence (kb, keys, output, error))
      return false;
  }
  
  return true;
}

/**
 * Public procedures
 */

Keyboard *keyboard_new (const char *name,
                        const char *url,
                        const char *language,
                        const char *icons_source,
                        GList *datafiles,
                        const char *base_encoding,
                        bool osxopt,
                        int capslock_policy)
{
  Keyboard *kb = g_slice_alloc0 (sizeof (Keyboard));

  kb->name = name;
  kb->keylayout_basename = g_strconcat (name, ".keylayout", NULL);
  kb->icons_basename = g_strconcat (name, ".icns", NULL);
  kb->url = url;
  kb->language = language;
  kb->icons_source = icons_source;
  kb->datafiles = datafiles;
  kb->base_encoding = base_encoding;
  kb->osxopt = osxopt;
  kb->capslock_policy = capslock_policy;

  kb->base_keymaps = key_map_set_new (false, kb->capslock_policy == CAPSLOCK_DISABLES);
  kb->control_keymaps = key_map_set_new (true, kb->capslock_policy == CAPSLOCK_DISABLES);

  kb->literals = prefix_map_new ();
  kb->actions = g_tree_new ((GCompareFunc)strcmp);
  kb->terminators = g_tree_new ((GCompareFunc)strcmp);

  return kb;
}

bool
keyboard_load_data (Keyboard *kb, GError **error)
{
  char *data = data_load (kb->base_encoding, error);
  if (data == NULL)
    return false;

  char *osxopt;
  if (kb->osxopt)
  {
    osxopt = data_load_internal (KB_DATA_OSXOPT, error);
    if (osxopt == NULL)
      return false;
  }
  else
  {
    osxopt = NULL;
  }

  if (!keyboard_set_base_encoding (kb, kb->base_encoding, data, osxopt, error))
    return false;

  // not freeing data, &c because it's chopped up into little strings that can still be used

  for (GList *iter = kb->datafiles; iter != NULL; iter = iter->next)
  {
    const char *data_name = iter->data;
    data = data_load (data_name, error);
    if (data == NULL || !keyboard_load_mappings (kb, data_name, data, error))
      return false;
  }

  return true;
}

bool
keyboard_write_keylayout (Keyboard *kb, GError **error)
{
  int index = 0;
  key_map_set_maybe_unshift (kb->control_keymaps);
  key_map_set_assign_mods (kb->base_keymaps, &index);
  key_map_set_assign_mods (kb->control_keymaps, &index);

  Out *out = out_open (kb->keylayout_basename, error);
  if (out == NULL)
    return false;

  if (!out_print (out, error,
                  "<?xml version=\"1.1\" encoding=\"UTF-8\"?>\n"
                  "<!DOCTYPE keyboard SYSTEM \"file://localhost/System/Library/DTDs/KeyboardLayout.dtd\">\n")

      || !out_printf (out, error,
                      "<keyboard group=\"126\" id=\"-1\" name=\"%s\">\n",
                      kb->name)

      || !out_print (out, error,
                     "  <layouts>\n"
                     "    <layout first=\"0\" last=\"0\" mapSet=\"maps\" modifiers=\"mods\" />\n"
                     "  </layouts>\n")

      || !out_print (out, error,
                     "  <modifierMap id=\"mods\" defaultIndex=\"0\">\n")

      || !key_map_set_write_mods (kb->base_keymaps, out, error)
      || !key_map_set_write_mods (kb->control_keymaps, out, error)

      || !out_print (out, error,
                     "  </modifierMap>\n"
                     "  <keyMapSet id=\"maps\">\n")

      || !key_map_set_write_maps (kb->base_keymaps, out, error)
      || !key_map_set_write_maps (kb->control_keymaps, out, error)

      || !out_print (out, error,
                     "  </keyMapSet>\n")

      || !keyboard_write_actions (kb, out, error)
      || !keyboard_write_terminators (kb, out, error)

      || !out_print (out, error, "</keyboard>\n"))
  {
    // nothing? goto close?
  }

  out_close (out, error);

  return *error == NULL;
}

bool
keyboard_write_info_plist (Keyboard *kb, Out *out, GError **error)
{
  if (!out_printf (out, error,
                   "    <key>KLInfo_%s</key>\n"
                   "    <dict>\n",
                   kb->name)

      || !out_printf (out, error,
                      "      <key>TICapsLockLanguageSwitchCapable</key>\n"
                      "      <%s/>\n",
                      kb->capslock_policy == CAPSLOCK_SWITCHES_IM ? "true" : "false")

      || !out_printf (out, error,
                      "      <key>TISInputSourceID</key>\n"
                      "      <string>%s</string>\n",
                      kb->url)

      || !out_printf (out, error,
                      "      <key>TISIntendedLanguage</key>\n"
                      "      <string>%s</string>\n",
                      kb->language)

      || !out_print (out, error,
                     "    </dict>\n"))
  {
    return false;
  }

  return true;  
}

