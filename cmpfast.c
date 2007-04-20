/* $Header$
 *
 * fast binary compare for two files
 *
 * Copyright (C)2007 Valentin Hilbig <webmaster@scylla-charybdis.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 * $Log$
 * Revision 1.1  2007-04-20 20:44:39  tino
 * Yet untested first version
 *
 */

#include "tino/file.h"
#include "tino/alloc.h"
#include "tino/getopt.h"
#include "tino/err.h"

#include "cmpfast_version.h"

#include <limits.h>

/* Possible extensions:
 *
 * Use cryptographic checksums to protect against bit errors? (I am
 * just paranoied.)
 *
 * Note:
 *
 * Memory mapped IO is not of much help, as this does thrashing again.
 * It would help only on situations where memory is very limited, as
 * this method does not need large compare buffers.  The only
 * optimization would be with a clever timing technique, which detects
 * when paging situations become incomfortable, thus using the cache
 * instead of userspace RAM.
 */
int
main(int argc, char **argv)
{
  int		argn, fd[2], n, eof;
  unsigned long	buflen;
  char		cmpbuf[BUFSIZ*10], *buf;

  argn	= tino_getopt(argc, argv, 2, 2,
		      TINO_GETOPT_VERSION(CMPFAST_VERSION)
		      " file1 file2\n"
		      "	fast compare two files, use - for stdin.\n"
		      "	returns 0 (true) if equal, something else else",

		      TINO_GETOPT_USAGE
		      "h	This help"
		      ,
		      TINO_GETOPT_ULONGINT TINO_GETOPT_SUFFIX
		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_MIN TINO_GETOPT_MAX
		      "b size	Buffer size"
		      , &buflen,
		      1024ul*1024ul,
		      4096ul, (unsigned long)(INT_MAX>SSIZE_MAX ? SSIZE_MAX : INT_MAX),

		      NULL
		      );

  if (argn<=0)
    return 1;

  /* XXX TODO
   *
   * Maximum buffer size shall be not higher than the smallest file size.
   */
  buf	= tino_alloc(buflen);

  /* XXX TODO
   *
   * Use generic file open method to allow compare of socket data,
   * too.
   */
  for (n=0; n<2; n++)
    {
      fd[n]	= 0;
      if (strcmp(argv[argn+n],"-") && (fd[n]=tino_file_open(argv[argn+n], 0))<0)
	{
	  tino_err("%s %s not found", n ? "ETTFC101A" : "ETTFC102A", argv[argn+n]);
	  return -1;
	}
    }
  if (!fd[0] && !fd[1])
    tino_err("ETTFC100A - both files cannot be stdin");

  /* XXX TODO
   *
   * Extend to more than 2 file handles.
   */
  eof	= 0;
  for (n=0;; )
    {
      int	got, off;

      if (!eof && !fd[n])
	n	= !n;		/* Avoid stdin in the read buffer	*/
      got	= tino_file_read(fd[n], buf, buflen);
      if (got<0)
	{
	  tino_err("%s %s read error", (n ? "ETTFC112A" : "ETTFC111A"), argv[argn+n]);
	  return -1;
	}
      n		= !n;
      if (!got)
	{
	  if (eof)
	    return 0;
	  eof	= 1;
	  continue;
	}
      if (eof)
	{
	  tino_err("%s %s eof on file", (n ? "WTTFC122A" : "WTTFC121A"), argv[argn+n]);
	  return -1;
	}
      for (off=0; off<got; )
	{
	  int	max, cmp;

	  max	= got-off;
	  if (max>sizeof cmpbuf)
	    max	= sizeof cmpbuf;
	  cmp	= tino_file_read(fd[n], cmpbuf, max);
	  if (cmp<0)
	    {
	      tino_err("%s %s read error", (n ? "ETTFC112A" : "ETTFC111A"), argv[argn+n]);
	      return -1;
	    }
	  if (!cmp)
	    {
	      tino_err("%s %s eof on file", (n ? "WTTFC122A" : "WTTFC121A"), argv[argn+n]);
	      return -1;
	    }
	  if (memcmp(cmpbuf, buf+off, cmp))
	    {
	      tino_err("NTTFC130A files differ");
	      return 2;
	    }
	  off	+= cmp;
	}
      /* Now roles of the two files are reversed, so reads come from
       * the same file which was compared before.
       */
    }
}
