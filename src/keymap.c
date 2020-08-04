#include "keymap.h"
#include <string.h>

static Result *key_map_set_lookup_result (KeyMapSet *set, int mods, int shift_state, int code);

static KeyMapSubset *key_map_subset_new (bool is_control, bool is_option, bool capslock_disables);
static KeyMapSubset *key_map_subset_copy (KeyMapSubset *src);
static void key_map_subset_set_backup (KeyMapSubset *set, KeyMap *backup);
static void key_map_subset_distinguish_shift_state (KeyMapSubset *set);
static Result *key_map_subset_lookup_result (KeyMapSubset *set, int shift_state, int code);
static void key_map_subset_maybe_unshift (KeyMapSubset *set);
static void key_map_subset_assign_mods (KeyMapSubset *set, const char *required, const char *permitted, int *index);

static bool key_map_subset_write_mods (KeyMapSubset *set, Out *out, GError **error);
static bool key_map_subset_write_maps (KeyMapSubset *set, Out *out, GError **error);

static void key_map_subset_set_capslock_active (KeyMapSubset *set);

static KeyMap *key_map_new (void);
static KeyMap *key_map_copy (KeyMap *src);
static Result *key_map_lookup_result (KeyMap *map, int code);
static void key_map_assign_mods (KeyMap *map, const char *requied, const char *also_required, const char *permitted, const char *also_permitted);
static void key_map_assign_index (KeyMap *map, int *index);

static bool key_map_write_mods (KeyMap *map, Out *out, GError **error);
static bool key_map_write (KeyMap *map, Out *out, GError **error);

static bool key_maps_eq (KeyMap *lhs, KeyMap *rhs);

/**
 * Private procedures
 */

Result *
key_map_set_lookup_result (KeyMapSet *set, int mods, int shift_state, int code)
{
  if ((mods & MOD_OPTION) != 0)
  {
    if (set->opt_maps == NULL)
      set->opt_maps = key_map_subset_copy (set->backup_maps);

    return key_map_subset_lookup_result (set->opt_maps, shift_state, code);
  }
  else
  {
    return key_map_subset_lookup_result (set->plain_maps, shift_state, code);
  }
}

KeyMapSubset *
key_map_subset_new (bool is_control, bool is_option, bool capslock_disables)
{
  KeyMapSubset *set = g_slice_alloc0 (sizeof (KeyMapSubset));

  set->is_control = is_control;
  set->is_option = is_option;
  set->capslock_disables = capslock_disables;

  set->shiftless_map = key_map_new ();
  if (!set->is_control)
  {
    set->shifty_map = key_map_new ();
    if (!set->is_option && !capslock_disables)
      set->capslock_map = key_map_new ();
  }

  return set;
}

KeyMapSubset *
key_map_subset_copy (KeyMapSubset *src)
{
  KeyMapSubset *set = g_slice_alloc0 (sizeof (KeyMapSubset));

  set->is_control = src->is_control;
  set->is_option = src->is_option;
  set->capslock_disables = src->capslock_disables;
  set->active_capslock = src->active_capslock;

  set->shiftless_map = key_map_copy (src->shiftless_map);

  if (!set->is_control)
  {
    set->shifty_map = key_map_copy (src->shifty_map);
    if (!set->is_option && !set->capslock_disables && set->active_capslock)
      set->capslock_map = key_map_copy (src->capslock_map);
  }
  
  return set;
}

void
key_map_subset_set_backup (KeyMapSubset *set, KeyMap *backup)
{
  set->backup_map = backup;
}

void
key_map_subset_distinguish_shift_state (KeyMapSubset *set)
{
  g_assert (set->shifty_map == NULL && set->capslock_map == NULL);
  g_assert (set->backup_map != NULL);
  // and we know this is in the control maps

  set->shifty_map = key_map_copy (set->backup_map);
  if (set->active_capslock)
    set->capslock_map = key_map_copy (set->backup_map);
}

Result *
key_map_subset_lookup_result (KeyMapSubset *set, int shift_state, int code)
{
  if (shift_state == SHIFTLESS)
  {
    return key_map_lookup_result (set->shiftless_map, code);
  }
  else if (shift_state == SHIFTY)
  {
    if (set->shifty_map == NULL)
      key_map_subset_distinguish_shift_state (set);

    return key_map_lookup_result (set->shifty_map, code);
  }
  else
  {
    g_assert (!set->capslock_disables);

    if (set->capslock_map == NULL)
    {
      g_assert (set->active_capslock);
      key_map_subset_distinguish_shift_state (set);
    }

    return key_map_lookup_result (set->capslock_map, code);
  }
}

void
key_map_subset_maybe_unshift (KeyMapSubset *set)
{
  if (set->shifty_map)
  {
    if (key_maps_eq (set->shiftless_map, set->shifty_map)
        && (set->capslock_map == NULL || key_maps_eq (set->shiftless_map, set->capslock_map)))
    {
      set->shifty_map = NULL; // leak!
      set->capslock_map = NULL;
    }
  }
}

void
key_map_subset_assign_mods (KeyMapSubset *set, const char *required, const char *permitted, int *index)
{
  if (set->capslock_disables)
  {
    g_assert (set->capslock_map == NULL);
    if (set->shifty_map)
    {
      key_map_assign_mods (set->shiftless_map, required, "", permitted, "");
      key_map_assign_mods (set->shiftless_map, required, " command", permitted, " anyShift?");
      key_map_assign_mods (set->shifty_map, required, " anyShift", permitted, "");
    }
    else
    {
      key_map_assign_mods (set->shiftless_map, required, "", permitted, " command? anyShift?");
    }
  }
  else if (set->shifty_map)
  {
    if (set->capslock_map)
    {
      key_map_assign_mods (set->shiftless_map, required, "", permitted, "");
      key_map_assign_mods (set->shifty_map, required, " anyShift", permitted, " caps?");
      key_map_assign_mods (set->capslock_map, required, " caps", permitted, "");
    }
    else
    {
      key_map_assign_mods (set->shiftless_map, required, "", permitted, " caps?");
      key_map_assign_mods (set->shifty_map, required, " anyShift", permitted, " caps?");
    }

    key_map_assign_mods (set->shiftless_map, required, " command", permitted, " anyShift? caps?");
  }
  else
  {
    // no shifty map, but capslock doesn't disable
    key_map_assign_mods (set->shiftless_map, required, "", permitted, " command? anyShift? caps?");    
  }

  key_map_assign_index (set->shiftless_map, index);
  if (set->shifty_map)
  {
    key_map_assign_index (set->shifty_map, index);
    if (set->capslock_map)
      key_map_assign_index (set->capslock_map, index);
  }
}

bool
key_map_subset_write_mods (KeyMapSubset *set, Out *out, GError **error)
{
  if (!key_map_write_mods (set->shiftless_map, out, error)
      || (set->shifty_map && !key_map_write_mods (set->shifty_map, out, error))
      || (set->capslock_map && !key_map_write_mods (set->capslock_map, out, error)))
  {
    return false;
  }

  return true;
}

bool
key_map_subset_write_maps (KeyMapSubset *set, Out *out, GError **error)
{
  if (!key_map_write (set->shiftless_map, out, error)
      || (set->shifty_map && !key_map_write (set->shifty_map, out, error))
      || (set->capslock_map && !key_map_write (set->capslock_map, out, error)))
  {
    return false;
  }

  return true;
}

void
key_map_subset_set_capslock_active (KeyMapSubset *set)
{
  g_assert (set->capslock_disables == false);
  set->active_capslock = true;
}

KeyMap *
key_map_new ()
{
  KeyMap *map = g_slice_alloc0 (sizeof (KeyMap));

  return map;
}

KeyMap *
key_map_copy (KeyMap *src)
{
  g_assert (src->mods == NULL && src->index == 0);

  KeyMap *map = g_slice_alloc0 (sizeof (KeyMap));

  memcpy (map->keys, src->keys, 128 * sizeof (Result));

  return map;
}

Result *
key_map_lookup_result (KeyMap *map, int code)
{
  g_assert (code >= 0 && code < 128);
  return &map->keys[code];
}

void
key_map_assign_mods (KeyMap *map, const char *required, const char *also_required, const char *permitted, const char *also_permitted)
{
  char *mods = g_strconcat (required, also_required, permitted, also_permitted, NULL); // will likely start with a space
  map->mods = g_list_append (map->mods, mods);
}

void
key_map_assign_index (KeyMap *map, int *index)
{
  map->index = *index;
  *index += 1;
}

bool
key_map_write_mods (KeyMap *map, Out *out, GError **error)
{
  if (!out_printf (out, error,
                   "    <keyMapSelect mapIndex=\"%d\">\n",
                   map->index))
  {
    return false;
  }

  for (GList *iter = map->mods; iter != NULL; iter = iter->next)
  {
    const char *mods = iter->data;
    if (*mods == ' ')
      ++mods;
    
    if (!out_printf (out, error,
                     "      <modifier keys=\"%s\" />\n",
                     mods))
    {
      return false;
    }
  }

  if (!out_print (out, error,
                  "    </keyMapSelect>\n"))
  {
    return false;
  }

  return true;
}


bool
key_map_write (KeyMap *map, Out *out, GError **error)
{
  if (!out_printf (out, error,
                   "    <keyMap index=\"%d\">\n",
                   map->index))
  {
    return false;
  }

  for (int idx = 0; idx < 128; ++idx)
  {
    if (map->keys[idx].result_type == RESULT_OUTPUT)
    {
      if (!out_printf (out, error,
                       "      <key code=\"%d\" output=\"%s\" />\n",
                       idx, map->keys[idx].content))
      {
        return false;
      }
    }
    else if (map->keys[idx].result_type == RESULT_ACTION)
    {
      if (!out_printf (out, error,
                       "      <key code=\"%d\" action=\"%s\" />\n",
                       idx, map->keys[idx].content))
      {
        return false;
      }
    }
  }

  if (!out_print (out, error,
                  "    </keyMap>\n"))
  {
    return false;
  }

  return true;
}

bool
key_maps_eq (KeyMap *lhs, KeyMap *rhs)
{
  for (int idx = 0; idx < 128; ++idx)
  {
    if (lhs->keys[idx].result_type != NO_RESULT && rhs->keys[idx].result_type != NO_RESULT)
    {
      if (lhs->keys[idx].result_type != rhs->keys[idx].result_type
          || strcmp (lhs->keys[idx].content, rhs->keys[idx].content) != 0)
      {
        return false;
      }
    }
  }

  return true;
}

/**
 * Public procedures
 */

KeyMapSet *
key_map_set_new (bool is_control, bool capslock_disables)
{
  KeyMapSet *set = g_slice_alloc0 (sizeof (KeyMapSet));

  set->is_control = is_control;
  set->capslock_disables = capslock_disables;

  set->plain_maps = key_map_subset_new (is_control, false, capslock_disables);

  return set;
}

void
key_map_set_set_result (KeyMapSet *set, int mods, int shift_state, int code, int result_type, const char *content)
{
  Result *result = key_map_set_lookup_result (set, mods, shift_state, code);
  result->result_type = result_type;
  result->content = content;
  set->dirty = true;
}

const Result *
key_map_set_get_result (KeyMapSet *set, int mods, int shift_state, int code)
{
  return key_map_set_lookup_result (set, mods, shift_state, code);
}

void
key_map_set_maybe_unshift (KeyMapSet *set)
{
  key_map_subset_maybe_unshift (set->plain_maps);
  if (set->opt_maps)
    key_map_subset_maybe_unshift (set->opt_maps);
}

void
key_map_set_make_backup (KeyMapSet *set)
{
  set->backup_maps = key_map_subset_copy (set->plain_maps);

  if (set->is_control)
  {
    key_map_subset_set_backup (set->plain_maps, set->backup_maps->shiftless_map);
    g_assert (set->opt_maps == NULL);
  }

  set->dirty = false;
}

void
key_map_set_assign_mods (KeyMapSet *set, int *index)
{
  const char *required;
  const char *permitted;

  required = set->is_control ? " control" : "";
  if (set->opt_maps)
    permitted = set->capslock_disables && !set->dirty ? " caps?" : "";
  else
    permitted = set->capslock_disables && !set->dirty ? " anyOption? caps?" : " anyOption?";

  key_map_subset_assign_mods (set->plain_maps, required, permitted, index);

  permitted = set->capslock_disables && !set->dirty ? " caps?" : "";
  
  if (set->opt_maps)
  {
    required = set->is_control ? " control anyOption" : " anyOption";

    key_map_subset_assign_mods (set->opt_maps, required, permitted, index);
  }

  if (set->capslock_disables && set->dirty)
  {
    // need to output the backup
    required = set->is_control ? " caps control" : " caps";
    key_map_subset_assign_mods (set->backup_maps, required, " anyOption?", index);
  }
}

bool
key_map_set_write_mods (KeyMapSet *set, Out *out, GError **error)
{
  if (!key_map_subset_write_mods (set->plain_maps, out, error)
      || (set->opt_maps && !key_map_subset_write_mods (set->opt_maps, out, error))
      || (set->capslock_disables && set->dirty && !key_map_subset_write_mods (set->backup_maps, out, error)))
  {
    return false;
  }
  
  return true;
}

bool
key_map_set_write_maps (KeyMapSet *set, Out *out, GError **error)
{
  if (!key_map_subset_write_maps (set->plain_maps, out, error)
      || (set->opt_maps && !key_map_subset_write_maps (set->opt_maps, out, error))
      || (set->capslock_disables && set->dirty && !key_map_subset_write_maps (set->backup_maps, out, error)))
  {
    return false;
  }

  return true;
}

void
key_map_set_set_capslock_active (KeyMapSet *set)
{
  g_assert (!set->capslock_disables);

  set->active_capslock = true;
  key_map_subset_set_capslock_active (set->plain_maps);
  if (set->opt_maps)
    key_map_subset_set_capslock_active (set->opt_maps);
}
