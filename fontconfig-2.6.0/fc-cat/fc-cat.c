/*
 * $RCSId: xc/lib/fontconfig/fc-cache/fc-cache.c,v 1.8tsi Exp $
 *
 * Copyright © 2002 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#ifdef linux
#define HAVE_GETOPT_LONG 1
#endif
#define HAVE_GETOPT 1
#endif

#include <fontconfig/fontconfig.h>
#include "../fc-arch/fcarch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifndef HAVE_GETOPT
#define HAVE_GETOPT 0
#endif
#ifndef HAVE_GETOPT_LONG
#define HAVE_GETOPT_LONG 0
#endif

#if HAVE_GETOPT_LONG
#undef  _GNU_SOURCE
#define _GNU_SOURCE
#include <getopt.h>
const struct option longopts[] = {
    {"version", 0, 0, 'V'},
    {"verbose", 0, 0, 'v'},
    {"recurse", 0, 0, 'r'},
    {"help", 0, 0, '?'},
    {NULL,0,0,0},
};
#else
#if HAVE_GETOPT
extern char *optarg;
extern int optind, opterr, optopt;
#endif
#endif

/*
 * POSIX has broken stdio so that getc must do thread-safe locking,
 * this is a serious performance problem for applications doing large
 * amounts of IO with getc (as is done here).  If available, use
 * the getc_unlocked varient instead.
 */
 
#if defined(getc_unlocked) || defined(_IO_getc_unlocked)
#define GETC(f) getc_unlocked(f)
#define PUTC(c,f) putc_unlocked(c,f)
#else
#define GETC(f) getc(f)
#define PUTC(c,f) putc(c,f)
#endif

static FcBool
write_chars (FILE *f, const FcChar8 *chars)
{
    FcChar8    c;
    while ((c = *chars++))
    {
	switch (c) {
	case '"':
	case '\\':
	    if (PUTC ('\\', f) == EOF)
		return FcFalse;
	    /* fall through */
	default:
	    if (PUTC (c, f) == EOF)
		return FcFalse;
	}
    }
    return FcTrue;
}

static FcBool
write_ulong (FILE *f, unsigned long t)
{
    int	    pow;
    unsigned long   temp, digit;

    temp = t;
    pow = 1;
    while (temp >= 10)
    {
	temp /= 10;
	pow *= 10;
    }
    temp = t;
    while (pow)
    {
	digit = temp / pow;
	if (PUTC ((char) digit + '0', f) == EOF)
	    return FcFalse;
	temp = temp - pow * digit;
	pow = pow / 10;
    }
    return FcTrue;
}

static FcBool
write_int (FILE *f, int i)
{
    return write_ulong (f, (unsigned long) i);
}

static FcBool
write_string (FILE *f, const FcChar8 *string)
{

    if (PUTC ('"', f) == EOF)
	return FcFalse;
    if (!write_chars (f, string))
	return FcFalse;
    if (PUTC ('"', f) == EOF)
	return FcFalse;
    return FcTrue;
}

static void
usage (char *program)
{
#if HAVE_GETOPT_LONG
    fprintf (stderr, "usage: %s [-rv] [--recurse] [--verbose] [*-%s.cache-2|directory]...\n",
	     program, FC_ARCHITECTURE);
    fprintf (stderr, "       %s [-V?] [--version] [--help]\n", program);
#else
    fprintf (stderr, "usage: %s [-rvV?] [*-%s.cache-2|directory]...\n",
	     program, FC_ARCHITECTURE);
#endif
    fprintf (stderr, "Reads font information cache from:\n"); 
    fprintf (stderr, " 1) specified fontconfig cache file\n");
    fprintf (stderr, " 2) related to a particular font directory\n");
    fprintf (stderr, "\n");
#if HAVE_GETOPT_LONG
    fprintf (stderr, "  -r, --recurse        recurse into subdirectories\n");
    fprintf (stderr, "  -v, --verbose        be verbose\n");
    fprintf (stderr, "  -V, --version        display font config version and exit\n");
    fprintf (stderr, "  -?, --help           display this help and exit\n");
#else
    fprintf (stderr, "  -r         (recurse) recurse into subdirectories\n");
    fprintf (stderr, "  -v         (verbose) be verbose\n");
    fprintf (stderr, "  -V         (version) display font config version and exit\n");
    fprintf (stderr, "  -?         (help)    display this help and exit\n");
#endif
    exit (1);
}

/*
 * return the path from the directory containing 'cache' to 'file'
 */

static const FcChar8 *
file_base_name (const FcChar8 *cache, const FcChar8 *file)
{
    int		    cache_len = strlen ((char *) cache);

    if (!strncmp ((char *) cache, (char *) file, cache_len) && file[cache_len] == '/')
	return file + cache_len + 1;
    return file;
}

#define FC_FONT_FILE_DIR	((FcChar8 *) ".dir")

static FcBool
cache_print_set (FcFontSet *set, FcStrSet *dirs, const FcChar8 *base_name, FcBool verbose)
{
    FcChar8	    *name, *dir;
    const FcChar8   *file, *base;
    int		    ret;
    int		    n;
    int		    id;
    int		    ndir = 0;
    FcStrList	    *list;

    list = FcStrListCreate (dirs);
    if (!list)
	goto bail2;
    
    while ((dir = FcStrListNext (list)))
    {
	base = file_base_name (base_name, dir);
	if (!write_string (stdout, base))
	    goto bail3;
	if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	if (!write_int (stdout, 0))
	    goto bail3;
        if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	if (!write_string (stdout, FC_FONT_FILE_DIR))
	    goto bail3;
	if (PUTC ('\n', stdout) == EOF)
	    goto bail3;
	ndir++;
    }
    
    for (n = 0; n < set->nfont; n++)
    {
	FcPattern   *font = set->fonts[n];

	if (FcPatternGetString (font, FC_FILE, 0, (FcChar8 **) &file) != FcResultMatch)
	    goto bail3;
	base = file_base_name (base_name, file);
	if (FcPatternGetInteger (font, FC_INDEX, 0, &id) != FcResultMatch)
	    goto bail3;
	if (!write_string (stdout, base))
	    goto bail3;
	if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	if (!write_int (stdout, id))
	    goto bail3;
        if (PUTC (' ', stdout) == EOF)
	    goto bail3;
	name = FcNameUnparse (font);
	if (!name)
	    goto bail3;
	ret = write_string (stdout, name);
	FcStrFree (name);
	if (!ret)
	    goto bail3;
	if (PUTC ('\n', stdout) == EOF)
	    goto bail3;
    }
    if (verbose && !set->nfont && !ndir)
	printf ("<empty>\n");
    
    FcStrListDone (list);

    return FcTrue;
    
bail3:
    FcStrListDone (list);
bail2:
    return FcFalse;
}

int
main (int argc, char **argv)
{
    int		i;
    int		ret = 0;
    FcFontSet	*fs;
    FcStrSet    *dirs;
    FcStrSet	*args = NULL;
    FcStrList	*arglist;
    FcCache	*cache;
    FcConfig	*config;
    FcChar8	*arg;
    int		verbose = 0;
    int		recurse = 0;
    FcBool	first = FcTrue;
#if HAVE_GETOPT_LONG || HAVE_GETOPT
    int		c;

#if HAVE_GETOPT_LONG
    while ((c = getopt_long (argc, argv, "Vvr?", longopts, NULL)) != -1)
#else
    while ((c = getopt (argc, argv, "Vvr?")) != -1)
#endif
    {
	switch (c) {
	case 'V':
	    fprintf (stderr, "fontconfig version %d.%d.%d\n", 
		     FC_MAJOR, FC_MINOR, FC_REVISION);
	    exit (0);
	case 'v':
	    verbose++;
	    break;
	case 'r':
	    recurse++;
	    break;
	default:
	    usage (argv[0]);
	}
    }
    i = optind;
#else
    i = 1;
#endif

    config = FcInitLoadConfig ();
    if (!config)
    {
	fprintf (stderr, "%s: Can't init font config library\n", argv[0]);
	return 1;
    }
    FcConfigSetCurrent (config);
    
    args = FcStrSetCreate ();
    if (!args)
    {
	fprintf (stderr, "%s: malloc failure\n", argv[0]);
	return 1;
    }
    if (i < argc)
    {
	for (; i < argc; i++)
	{
	    if (!FcStrSetAddFilename (args, (const FcChar8 *) argv[i]))
	    {
		fprintf (stderr, "%s: malloc failure\n", argv[0]);
		return 1;
	    }
	}
	arglist = FcStrListCreate (args);
	if (!arglist)
	{
	    fprintf (stderr, "%s: malloc failure\n", argv[0]);
	    return 1;
	}
    }
    else
    {
	recurse++;
	arglist = FcConfigGetFontDirs (config);
	while ((arg = FcStrListNext (arglist)))
	    if (!FcStrSetAdd (args, arg))
	    {
		fprintf (stderr, "%s: malloc failure\n", argv[0]);
		return 1;
	    }
	FcStrListDone (arglist);
    }
    arglist = FcStrListCreate (args);
    if (!arglist)
    {
	fprintf (stderr, "%s: malloc failure\n", argv[0]);
	return 1;
    }

    while ((arg = FcStrListNext (arglist)))
    {
	int	    j;
	FcChar8	    *cache_file = NULL;
	struct stat file_stat;
	
	if (FcFileIsDir (arg))
	    cache = FcDirCacheLoad (arg, config, &cache_file);
	else
	    cache = FcDirCacheLoadFile (arg, &file_stat);
	if (!cache)
	{
	    perror ((char *) arg);
	    ret++;
	    continue;
	}
	
	dirs = FcStrSetCreate ();
	fs = FcCacheCopySet (cache);
	for (j = 0; j < FcCacheNumSubdir (cache); j++) 
	{
	    FcStrSetAdd (dirs, FcCacheSubdir (cache, j));
	    if (recurse)
		FcStrSetAdd (args, FcCacheSubdir (cache, j));
	}

	if (verbose)
	{
	    if (!first)
		printf ("\n");
	    printf ("Directory: %s\nCache: %s\n--------\n",
		    FcCacheDir(cache), cache_file ? cache_file : arg);
	    first = FcFalse;
	}
        cache_print_set (fs, dirs, FcCacheDir (cache), verbose);

	FcStrSetDestroy (dirs);

	FcFontSetDestroy (fs);
	FcDirCacheUnload (cache);
	if (cache_file)
	    FcStrFree (cache_file);
    }

    return 0;
}
