#ifndef lint
static char rcsid[] = "$Header: /home/joerg/repo/netbsd/src/sbin/mount_isofs/Attic/mount_isofs.c,v 1.4 1993/07/19 11:41:47 cgd Exp $";
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/mount.h>

void
usage ()
{
	fprintf (stderr, "usage: mount_iso bdev dir\n");
	exit (1);
}
		
int
main (argc, argv)
int argc;
char **argv;
{
	char *dev;
	char *dir;
	struct ufs_args args;
	int c;
	extern char *optarg;
	extern int optind;
	int opts;

	opts = MNT_RDONLY;

	while ((c = getopt (argc, argv, "F:")) != EOF) {
		switch (c) {
		case 'F':
			opts |= atoi (optarg);
			break;
		default:
			usage ();
		}
	}

	if (optind + 2 != argc)
		usage ();

	dev = argv[optind];
	dir = argv[optind + 1];

	args.fspec = dev;
	args.exflags = MNT_EXRDONLY | opts;
	args.exroot = 0;

	if (mount (MOUNT_ISOFS, dir, MNT_RDONLY, &args) < 0) {
		perror ("mount");
		exit (1);
	}

	exit (0);
}

