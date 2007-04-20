/* $Header$
 *
 * Create files of byte runs needed for test.sh
 *
 * $Log$
 * Revision 1.1  2007-04-20 23:57:23  tino
 * Testing succeeded
 *
 */

#include <stdio.h>

int
main(int argc, char **argv)
{
  int	i;

  for (i=1; i<argc; i+=2)
    {
      int	nr, c;
  
      nr	= atoi(argv[i]);
      c		= atoi(argv[i+1]);
      while (--nr>=0)
        putchar(c);
    }
  return 0;
}
