/* fast binary compare for two files
 *
 * 
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

static int
try_open_read(const char *name, int direct)
{
  int	fd;

  if (!direct || (fd=tino_file_openE(name, O_RDONLY|O_DIRECT))<0)
    fd	= tino_file_openE(name, O_RDONLY);
  return fd;
}

/* Possible extensions:
 *
 * Use cryptographic checksums to protect against bit errors? (I am
 * just paranoid.)
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
  unsigned long		a_buflen, a_cmplen;
  size_t		buflen, cmplen;
  char			*cmpbuf, *buf;
  unsigned long long	pos;
  int			delta, async, direct;

  argn	= tino_getopt(argc, argv, 2, 2,
		      TINO_GETOPT_VERSION(CMPFAST_VERSION)
		      " file1 file2\n"
		      "	fast binary compare of two files, use - for stdin.\n"
		      "	returns 0 (true) if equal, 10 if differ,\n"
		      "	101/102 for EOF on file1/2, something else else",

		      TINO_GETOPT_USAGE
		      "h	This help"
		      ,

		      TINO_GETOPT_FLAG
		      "a	async, do not use O_DIRECT (default)"
		      , &async,

		      TINO_GETOPT_ULONGINT TINO_GETOPT_SUFFIX
		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_MIN_PTR TINO_GETOPT_MAX
		      "b size	Big buffer size"
		      , &a_cmplen						/* min	*/
		      , &a_buflen,
		      1024ul*1024ul,						/* default	*/
		      (unsigned long)(INT_MAX>SSIZE_MAX ? SSIZE_MAX : INT_MAX),	/* max	*/

		      TINO_GETOPT_FLAG
		      "d	direct, do use O_DIRECT"
		      , &direct,

		      TINO_GETOPT_ULONGINT TINO_GETOPT_SUFFIX
		      TINO_GETOPT_DEFAULT
		      TINO_GETOPT_MIN TINO_GETOPT_MAX_PTR
		      "s size	Small buffer size"
		      , &a_buflen		/* max	*/
		      , &a_cmplen,
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
  cmplen	= a_cmplen;
  delta	= tino_alloc_align_sizeO(&cmplen);
  if (delta)
    {
      tino_err("FTTFC103 small buffer size must be aligned %d", delta);
      return -1;
    }
  buflen= a_buflen;
  delta	= tino_alloc_align_sizeO(&buflen);
  if (delta)
    {
      tino_err("FTTFC103 big buffer size must be aligned %d", delta);
      return -1;
    }
  cmpbuf= tino_alloc_alignedO(cmplen);
  buf	= tino_alloc_alignedO(buflen);

  /* XXX TODO
   *
   * Use generic file open method to allow compare of socket data,
   * too.
   */
  for (n=0; n<2; n++)
    {
      fd[n]	= 0;
      if (strcmp(argv[argn+n],"-") && (fd[n]=try_open_read(argv[argn+n], direct && !async))<0)
	{
	  tino_err("%s open error on file %s", n ? "ETTFC102E" : "ETTFC101E", argv[argn+n]);
	  return -1;
	}
    }
  if (!fd[0] && !fd[1])
    {
      tino_err("FTTFC100 both files cannot be stdin");
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
      posix_fadvise(fd[n], (off_t)pos, (off_t)got, POSIX_FADV_DONTNEED);
      posix_fadvise(fd[n], (off_t)pos+got, (off_t)0, POSIX_FADV_SEQUENTIAL);
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
	  int		get;
	  size_t	max, cmp;

	  max	= got-off;
	  if (max>cmplen)
	    max	= cmplen;
	  cmp	= max;
	  tino_alloc_align_sizeO(&cmp);	/* Hack for O_DIRECT, read only full pages	*/
	  get	= tino_file_readE(fd[n], cmpbuf, cmp);
	  if (get<0)
	    {
	      tino_err("%s at byte %llu read error on file %s", (n ? "ETTFC112E" : "ETTFC111E"), pos, argv[argn+n]);
	      return -1;
	    }
	  if (!get)
	    {
	      tino_err("%s at byte %llu EOF on file %s", (n ? "NTTFC124A" : "NTTFC123A"), pos, argv[argn+n]);
	      return 101+n;
	    }
	  posix_fadvise(fd[n], (off_t)pos, (off_t)get, POSIX_FADV_DONTNEED);
	  posix_fadvise(fd[n], (off_t)pos+get, (off_t)0, POSIX_FADV_SEQUENTIAL);
	  if (max>get)
	    max	= get;
	  if (memcmp(cmpbuf, buf+off, max))
	    {
	      int	i, a, b;

	      /* Paranoid as I am, default output is to inform about
	       * some weird situation
	       */
	      a	= -1;
	      b	= -1;
	      /* Hunt for the byte which was different
	       */
	      for (i=0; i<max; i++)
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
	  off	+= max;
	  pos	+= max;
	  if (get>max)
	    {
	      /* Well, we have some remaining data (due to alignment).
	       * Reverse roles.  Usually this means EOF.
	       */
	      if (off!=got)
		{
		  tino_err("FTTFC105 internal error: alignment overhead in the middle of the block: %llu/%llu", off, got);
		  return -1;
		}
	      off	= 0;
	      got	= get-max;
	      memcpy(buf, cmpbuf+max, got);
	      n	= !n;
	    }
	}
      /* Now roles of the two files are reversed, so reads come from
       * the same file which was compared before.
       */
    }
}
