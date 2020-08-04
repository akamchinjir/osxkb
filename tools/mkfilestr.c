#include <stdio.h>
#include <stdlib.h>

static const int N_COLS = 16;

int
main (int argc, char **argv)
{
  if (argc != 3)
  {
    fprintf (stderr, "%s: bad command line\n", argv[0]);
    return 1;
  }

  const char *varname = argv[1];
  const char *path = argv[2];

  FILE *in = fopen (path, "r");
  if (in == NULL)
  {
    fprintf (stderr, "%s: could not open %s\n", argv[0], path);
    return 1;
  }

  printf ("static const char %s[] = {", varname);

  int count = 0;
  int c;
  while ((c = fgetc (in)) != EOF)
  {
    if (count % N_COLS == 0)
      printf ("\n ");

    char ch = (char)c;
    if (ch < 0)
      printf (" -0x%02x,", -ch);
    else
      printf ("  0x%02x,", ch);

    ++count;
  }

  if (count % N_COLS == 0)
    printf ("\n ");

  printf ("  0x00\n};\n\n");

  if (fclose (in) != 0)
  {
    fprintf (stderr, "%s: error closing %s\n", argv[0], path);
    return 1;
  }

  return 0;
}
