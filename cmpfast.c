/* $Header$
 *
 * fast binary compare for two files
 *
 * Copyright (C)2007-2008 Valentin Hilbig <webmaster@scylla-charybdis.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 *
 * $Log$
 * Revision 1.11  2008-07-01 18:47:28  tino
 * Usage had SPC instead of TAB
 *
 * Revision 1.10  2008-05-13 15:45:02  tino
 * Current tinolib
 *
 * Revision 1.9  2008-05-13 15:43:12  tino
 * Standard return values on EOF
 *
 * Revision 1.8  2008-04-23 03:59:11  tino
 * Forgot status flushing
 *
 * Revision 1.7  2008-04-22 23:57:27  tino
 * Option -v
 *
 * Revision 1.6  2007-09-26 03:12:16  tino
 * Memory footprint decreased in most situations.
 *
 * Revision 1.5  2007/04/20 23:58:57  tino
 * Added better messages and option -s
 *
 * Revision 1.4  2007/04/20 20:53:35  tino
 * I mixed numbers in error output
 *
 * Revision 1.3  2007/04/20 20:50:52  tino
 * TYPE_ERR clarified (and used)
 *
 * Revision 1.2  2007/04/20 20:49:48  tino
 * TYPE_ERR (E) added
 */

#include "tino/file.h"
#include "tino/alloc.h"
#include "tino/getopt.h"
#include "tino/err.h"

#include "cmpfast_version.h"

#include <limits.h>
#include <time.h>

static void
show(int n, unsigned long long pos)
{
  static time_t	last;
  time_t	now;

  time(&now);
  if (last==now)
    return;
  last	= now;
  printf("%d %lluM \r", n, pos>>20);
  fflush(stdout);
}

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
  int			argn, fd[2], n, eof, verbose;
  unsigned long		buflen, cmplen;
  char			*cmpbuf, *buf;
  unsigned long long	pos;

  argn	= tino_getopt(argc, argv, 2, 2,
		      TINO_GETOPT_VERSION(CMPFAST_VERSION)
		      " file1 file2\n"
		      "	fast binary compare of two files, use - for stdin.\n"
		      "	returns 0 (true) if equal, 10 if differ,\n"
		      "	101/102 for EOF on file1/2, something else else",

		      TINO_GETOPT_USAGE
		      "h	This help"
		      ,
		      TINO_GETOPT_ULONGINT TINO_GETOPT_SUFFIX
		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_MIN_PTR TINO_GETOPT_MAX
		      "b size	Big buffer size"
		      , &cmplen							/* min	*/
		      , &buflen,
		      1024ul*1024ul,						/* default	*/
		      (unsigned long)(INT_MAX>SSIZE_MAX ? SSIZE_MAX : INT_MAX),	/* max	*/

		      TINO_GETOPT_ULONGINT TINO_GETOPT_SUFFIX
		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_MIN TINO_GETOPT_MAX_PTR
		      "s size	Small buffer size"
		      , &buflen			/* max	*/
		      , &cmplen,
		      (unsigned long)BUFSIZ*10,	/* default	*/
		      (unsigned long)BUFSIZ,	/* min	*/

		      TINO_GETOPT_FLAG
		      "v	verbose, print progress"
		      , &verbose,

		      NULL
		      );

  if (argn<=0)
    return 1;

  /* XXX TODO
   *
   * Maximum buffer size shall be not higher than the smallest file size.
   */
  cmpbuf= tino_allocO(cmplen);
  buf	= tino_allocO(buflen);

  /* XXX TODO
   *
   * Use generic file open method to allow compare of socket data,
   * too.
   */
  for (n=0; n<2; n++)
    {
      fd[n]	= 0;
      if (strcmp(argv[argn+n],"-") && (fd[n]=tino_file_openE(argv[argn+n], 0))<0)
	{
	  tino_err("%s open error on file %s", n ? "ETTFC102E" : "ETTFC101E", argv[argn+n]);
	  return -1;
	}
    }
  if (!fd[0] && !fd[1])
    {
      tino_err("ETTFC100F both files cannot be stdin");
      return -1;
    }

  /* XXX TODO
   *
   * Extend to more than 2 file handles.
   */
  pos	= 0;
  eof	= 0;
  for (n=0;; )
    {
      int	got, off;

      if (!eof && !fd[n])
	n	= !n;		/* Avoid stdin in the read buffer	*/
      if (verbose)
	show(n, pos);
      got	= tino_file_readE(fd[n], buf, buflen);
      if (got<0)
	{
	  tino_err("%s read error on file %s", (n ? "ETTFC112E" : "ETTFC111E"), argv[argn+n]);
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
	  tino_err("%s at byte %llu EOF on file %s", (n ? "NTTFC122A" : "NTTFC121A"), pos, argv[argn+n]);
	  return 101+n;
	}
      if (verbose)
	show(n, pos);
      for (off=0; off<got; )
	{
	  int	max, cmp;

	  max	= got-off;
	  if (max>cmplen)
	    max	= cmplen;
	  cmp	= tino_file_readE(fd[n], cmpbuf, max);
	  if (cmp<0)
	    {
	      tino_err("%s read error on file %s", (n ? "ETTFC112E" : "ETTFC111E"), argv[argn+n]);
	      return -1;
	    }
	  if (!cmp)
	    {
	      tino_err("%s at byte %llu EOF on file %s", (n ? "NTTFC124A" : "NTTFC123A"), pos, argv[argn+n]);
	      return 101+n;
	    }
	  if (memcmp(cmpbuf, buf+off, cmp))
	    {
	      int	i, a, b;

	      /* Paranoied as I am, default output is to inform about
	       * some weird situation
	       */
	      a	= -1;
	      b	= -1;
	      /* Hunt for the byte which was different
	       */
	      for (i=0; i<cmp; i++)
		if (cmpbuf[i]!=buf[off+i])
		  {
		    a	= (unsigned char)buf[off+i];
		    b	= (unsigned char)cmpbuf[i];
		    break;
		  }
	      pos		+= i;
	      if (!n)
		{
		  int	x;

		  /* Switch byte sides if we are reveresed
		   */
		  x	= a;
		  a	= b;
		  b	= x;
		}
	      tino_err("ITTFC130B files differ at byte %llu ($%02x $%02x)", pos, a, b);
	      return 10;
	    }
	  off	+= cmp;
	  pos	+= cmp;
	}
      /* Now roles of the two files are reversed, so reads come from
       * the same file which was compared before.
       */
    }
}
