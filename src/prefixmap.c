#include "prefixmap.h"

static void prefix_map_init (PrefixMap *map);

void
prefix_map_init (PrefixMap *map)
{
  map->capacity = 8;
  map->children = g_malloc (map->capacity * sizeof (PrefixNode));
  map->n_children = 0;
}

/**
 * Public procedures
 */

PrefixMap *
prefix_map_new ()
{
  PrefixMap *map = g_slice_alloc0 (sizeof (PrefixMap));

  prefix_map_init (map);

  return map;
}

void **
prefix_map_lookup (PrefixMap *map, const char *key)
{
  g_assert (*key != '\0');

  char c = *key;

  int low = 0;
  int high = map->n_children - 1;
  int mid = 0;
  int cmp = -1; // so index ends up as 0 when the map is empty
  while (low <= high)
  {
    mid = (low + high) / 2;
    cmp = c - map->children[mid].key;
    if (cmp < 0)
    {
      high = mid - 1;
    }
    else if (cmp > 0)
    {
      low = mid + 1;
    }
    else
    {
      if (key[1] == '\0')
        return &map->children[mid].data;
      
      return prefix_map_lookup (&map->children[mid].submap, key + 1);
    }
  }

  // not there, have to insert a position for it
  // it should go either right before or right after mid

  int index = cmp < 0 ? mid : mid + 1;
  if (map->n_children == map->capacity)
  {
    map->capacity *= 2;
    map->children = g_realloc (map->children, map->capacity * sizeof (PrefixNode));
  }

  for (int i = map->n_children; i > index; --i)
    map->children[i] = map->children[i-1];

  while (true)
  {
    map->n_children += 1;
    map->children[index].key = *key;
    map->children[index].data = NULL;
    prefix_map_init (&map->children[index].submap);

    if (key[1] == '\0')
      break;

    map = &map->children[index].submap; // which has just been initted, so has space for a child at index = 0
    index = 0;
    ++key;
  }

  return &map->children[index].data;  
}

void *
prefix_map_get (PrefixMap *map, const char *key)
{
  g_assert (*key != '\0');

  char c = *key;

  int low = 0;
  int high = map->n_children - 1;
  while (low <= high)
  {
    int mid = (low + high) / 2;
    int cmp = c - map->children[mid].key;
    if (cmp < 0)
    {
      high = mid - 1;
    }
    else if (cmp > 0)
    {
      low = mid + 1;
    }
    else
    {
      if (key[1] == '\0')
        return map->children[mid].data;
      
      return prefix_map_get (&map->children[mid].submap, key + 1);
    }
  }

  return NULL;
}

void *
prefix_map_get_prefix (PrefixMap *map, const char **key)
{
  g_assert (**key != '\0');

  const char *ptr = *key;
  char c = *ptr;

  int low = 0;
  int high = map->n_children - 1;
  while (low <= high)
  {
    int mid = (low + high) / 2;
    int cmp = c - map->children[mid].key;
    if (cmp < 0)
    {
      high = mid - 1;
    }
    else if (cmp > 0)
    {
      low = mid + 1;
    }
    else
    {
      ++ptr;
      if (map->children[mid].data != NULL)
        *key = ptr;
      
      if (*ptr == '\0')
        return map->children[mid].data;

      void *ret = prefix_map_get_prefix (&map->children[mid].submap, &ptr);
      if (ret)
      {
        *key = ptr;
        return ret;
      }
      else
      {
        return map->children[mid].data;
      }
    }
  }

  return NULL;
}
