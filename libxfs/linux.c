/*
 * Copyright (c) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.	 Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#define ustat __kernel_ustat
#include <xfs/libxfs.h>
#include <mntent.h>
#include <sys/stat.h>
#undef ustat
#include <sys/ustat.h>
#include <sys/mount.h>
#include <sys/ioctl.h>

extern char *progname;

#ifndef BLKGETSIZE64
# define BLKGETSIZE64	_IOR(0x12,114,sizeof(__uint64_t))
#endif
#ifndef BLKBSZSET
# define BLKBSZSET	_IOW(0x12,113,sizeof(int))
#endif

#define PROC_MOUNTED	"/proc/mounts"

int
platform_check_ismounted(char *name, char *block, struct stat64 *s, int verbose)
{
	struct ustat	ust;
	struct stat64	st;

	if (!s) {
		if (stat64(block, &st) < 0)
			return 0;
		if ((st.st_mode & S_IFMT) != S_IFBLK)
			return 0;
		s = &st;
	}

	if (ustat(s->st_rdev, &ust) >= 0) {
		if (verbose)
			fprintf(stderr,
				_("%s: %s contains a mounted filesystem\n"),
				progname, name);
		return 1;
	}
	return 0;
}

int
platform_check_iswritable(char *name, char *block, struct stat64 *s, int fatal)
{
	int		sts = 0;
	FILE		*f;
	struct stat64	mst;
	struct mntent	*mnt;
	char		mounts[MAXPATHLEN];

	strcpy(mounts, access(PROC_MOUNTED, R_OK)? PROC_MOUNTED : MOUNTED);
	if ((f = setmntent(mounts, "r")) == NULL) {
		fprintf(stderr, _("%s: %s contains a possibly writable, "
				"mounted filesystem\n"), progname, name);
			return fatal;
	}
	while ((mnt = getmntent(f)) != NULL) {
		if (stat64(mnt->mnt_fsname, &mst) < 0)
			continue;
		if ((mst.st_mode & S_IFMT) != S_IFBLK)
			continue;
		if (mst.st_rdev == s->st_rdev
		    && hasmntopt(mnt, MNTOPT_RO) != NULL)
			break;
	}
	if (mnt == NULL) {
		fprintf(stderr, _("%s: %s contains a mounted and writable "
				"filesystem\n"), progname, name);
		sts = fatal;
	}
	endmntent(f);
	return sts;
}

void
platform_set_blocksize(int fd, char *path, int blocksize)
{
	if (ioctl(fd, BLKBSZSET, &blocksize) < 0) {
		fprintf(stderr, _("%s: warning - cannot set blocksize "
				"on block device %s: %s\n"),
			progname, path, strerror(errno));
	}
}

void
platform_flush_device(int fd)
{
	ioctl(fd, BLKFLSBUF, 0);
}

__int64_t
platform_findsize(char *path)
{
	int		fd;
	int		error;
	__int64_t	ssize;
	__uint64_t	size;
	struct stat64	st;

	if (stat64(path, &st) < 0) {
		fprintf(stderr, _("%s: "
			"cannot stat the device file \"%s\": %s\n"),
			progname, path, strerror(errno));
		exit(1);
	}
	if ((st.st_mode & S_IFMT) == S_IFREG)
		return (__int64_t)(st.st_size >> 9);

	if ((fd = open(path, O_RDONLY, 0)) < 0) {
		fprintf(stderr, _("%s: "
			"error opening the device special file \"%s\": %s\n"),
			progname, path, strerror(errno));
		exit(1);
	}

	error = ioctl(fd, BLKGETSIZE64, &size);
	if (error >= 0) {
		/* BLKGETSIZE64 returns size in bytes not 512-byte blocks */
		ssize = size >> 9;
	} else {
		/* If BLKGETSIZE64 fails, try BLKGETSIZE */
		unsigned long tmpsize;
		error = ioctl(fd, BLKGETSIZE, &tmpsize);
		if (error < 0) {
			fprintf(stderr, _("%s: can't determine device size\n"),
				progname);
			exit(1);
		}
		ssize = (__int64_t)tmpsize;
	}

	close(fd);
	return ssize;
}
