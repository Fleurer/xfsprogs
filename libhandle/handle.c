/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston MA 02111-1307,
 * USA.
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

#include <libxfs.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

/* attributes.h (purposefully) unavailable to xfsprogs, make do */
struct attrlist_cursor { __u32 opaque[4]; };

#include <handle.h>

/* just pick a value we know is more than big enough */
#define	MAXHANSIZ	64

/*
 *  The actual content of a handle is supposed to be opaque here.
 *  But, to do handle_to_fshandle, we need to know what it is.  Sigh.
 *  However we can get by with knowing only that the first 8 bytes of
 *  a file handle are the file system ID, and that a file system handle
 *  consists of only those 8 bytes.
 */

#define	FSIDSIZE	8

typedef union {
	int	fd;
	char	*path;
} comarg_t;

int
obj_to_handle (
	int		fsfd,
	unsigned int	opcode,
	comarg_t	obj,
	void		**hanp,
	size_t		*hlen);


/*
 * Filesystem Handle -> Open File Descriptor Cache
 *
 * Maps filesystem handles to a corresponding open file descriptor for that
 * filesystem. We need this because we're doing handle operations via ioctl
 * and we need to remember the open file descriptor for each filesystem.
 */

struct fdhash {
	int	fsfd;
	char	fsh[FSIDSIZE];
	struct fdhash *fnxt;
};

struct fdhash *fdhash_head = NULL;

int
path_to_fshandle (
	char		*path,		/* input,  path to convert */
	void		**hanp,		/* output, pointer to data */
	size_t		*hlen)		/* output, size of returned data */
{
	int		result;
	int		fd;
	comarg_t	obj;
	struct fdhash	*fdhp;

	fd = open(path, O_RDONLY);

	if (fd <  0) {
		perror(path);
		exit(1);
	}

	obj.path = path;

	result = obj_to_handle (fd, XFS_IOC_PATH_TO_FSHANDLE,
				obj, hanp, hlen);

	if (result >= 0) {
		fdhp = malloc(sizeof(struct fdhash));

		if (fdhp == NULL) {
			errno = ENOMEM;
			return -1;
		}

		fdhp->fsfd = fd;
		fdhp->fnxt = NULL;

		memcpy(fdhp->fsh, *hanp, FSIDSIZE);

		if (fdhash_head)
			fdhash_head->fnxt = fdhp;
		else
			fdhash_head       = fdhp;
	}

	return result;
}


int
path_to_handle (
	char		*path,		/* input,  path to convert */
	void		**hanp,		/* output, pointer to data */
	size_t		*hlen)		/* output, size of returned data */
{
	int		fd;
	int		result;
	comarg_t	obj;

	fd = open(path, O_RDONLY);

	if (fd <  0) {
		perror(path);
		exit(1);
	}

	obj.path = path;

	result = obj_to_handle (fd, XFS_IOC_PATH_TO_HANDLE, obj, hanp, hlen);

	close(fd);

	return result;
}


int
fd_to_handle (
	int		fd,		/* input,  file descriptor */
	void		**hanp,		/* output, pointer to data */
	size_t		*hlen)		/* output, size of returned data */
{
	comarg_t	obj;

	obj.fd = fd;

	return obj_to_handle (fd, XFS_IOC_FD_TO_HANDLE, obj, hanp, hlen);
}


int
handle_to_fshandle (
	void		*hanp,
	size_t		hlen,
	void		**fshanp,
	size_t		*fshlen)
{
	if (hlen < FSIDSIZE)
		return EINVAL;

	*fshanp = malloc (FSIDSIZE);

	if (*fshanp == NULL)
		return ENOMEM;

	*fshlen = FSIDSIZE;

	memcpy(*fshanp, hanp, FSIDSIZE);

	return 0;
}


int
handle_to_fsfd(void *hanp)
{
	struct fdhash	*fdhp;

	for (fdhp = fdhash_head; fdhp != NULL; fdhp = fdhp->fnxt) {
		if (memcmp(fdhp->fsh, hanp, FSIDSIZE) == 0)
			return fdhp->fsfd;
	}
	return -1;
}


int
obj_to_handle (
	int		fsfd,
	unsigned int	opcode,
	comarg_t	obj,
	void		**hanp,
	size_t		*hlen)
{
	char		hbuf [MAXHANSIZ];
	int		ret;
	xfs_fsop_handlereq_t hreq;

	if (opcode == XFS_IOC_FD_TO_HANDLE) {
		hreq.fd      = obj.fd;
		hreq.path    = NULL;
	} else {
		hreq.fd      = 0;
		hreq.path    = obj.path;
	}

	hreq.oflags   = 0;
	hreq.ihandle  = NULL;
	hreq.ihandlen = 0;
	hreq.ohandle  = hbuf;
	hreq.ohandlen = (__u32 *)hlen;

	ret = (int) ioctl(fsfd, opcode, &hreq);

	if (ret)
		return ret;

	*hanp = malloc(*hlen);	

	if (*hanp == NULL) {
		errno = ENOMEM;
		return -1;
	}

	memcpy(*hanp, hbuf, (int) *hlen);

	return 0;
}



int
open_by_handle (
	void		*hanp,
	size_t		hlen,
	int		rw)
{
	int		fd;
	int		result;
	xfs_fsop_handlereq_t hreq;

	if ((fd = handle_to_fsfd(hanp)) < 0) {
		errno = EBADF;
		return -1;
	}

	hreq.fd       = 0;
	hreq.path     = NULL;
	hreq.oflags   = rw;
	hreq.ihandle  = hanp;
	hreq.ihandlen = hlen;
	hreq.ohandle  = NULL;
	hreq.ohandlen = NULL;

	result = ioctl(fd, XFS_IOC_OPEN_BY_HANDLE, &hreq);

	return result;
}

int
readlink_by_handle (
	void		*hanp,
	size_t		hlen,
	void		*buf,
	size_t		bufsiz)
{
	int		fd;
	xfs_fsop_handlereq_t hreq;


	if ((fd = handle_to_fsfd(hanp)) < 0) {
		errno = EBADF;
		return -1;
	}

	hreq.fd       = 0;
	hreq.path     = NULL;
	hreq.oflags   = 0;
	hreq.ihandle  = hanp;
	hreq.ihandlen = hlen;
	hreq.ohandle  = buf;
	hreq.ohandlen = (__u32 *)&bufsiz;

	return (int) ioctl(fd, XFS_IOC_READLINK_BY_HANDLE, &hreq);
}

int
attr_multi_by_handle(
	void		*hanp,
	size_t		hlen,
	void		*buf,
	int		rtrvcnt,
	int		flags)
{
	int		fd;
	xfs_fsop_attrmulti_handlereq_t amhreq;

	if ((fd = handle_to_fsfd(hanp)) < 0) {
		errno = EBADF;
		return -1;
	}

	amhreq.hreq.fd       = 0;
	amhreq.hreq.path     = NULL;
	amhreq.hreq.oflags   = 0;
	amhreq.hreq.ihandle  = hanp;
	amhreq.hreq.ihandlen = hlen;
	amhreq.hreq.ohandle  = NULL;
	amhreq.hreq.ohandlen = NULL;

	amhreq.opcount = rtrvcnt;
	amhreq.ops = buf;

	return (int) ioctl(fd, XFS_IOC_ATTRMULTI_BY_HANDLE, &amhreq);
}

int
attr_list_by_handle(
	void		*hanp,
	size_t		hlen,
	void		*buf,
	size_t		bufsize,
	int		flags,
	struct attrlist_cursor *cursor)
{
	int		fd;
	xfs_fsop_attrlist_handlereq_t alhreq;

	if ((fd = handle_to_fsfd(hanp)) < 0) {
		errno = EBADF;
		return -1;
	}

	alhreq.hreq.fd       = 0;
	alhreq.hreq.path     = NULL;
	alhreq.hreq.oflags   = 0;
	alhreq.hreq.ihandle  = hanp;
	alhreq.hreq.ihandlen = hlen;
	alhreq.hreq.ohandle  = NULL;
	alhreq.hreq.ohandlen = NULL;

	memcpy(&alhreq.pos, cursor, sizeof(alhreq.pos));
	alhreq.flags = flags;
	alhreq.buflen = bufsize;
	alhreq.buffer = buf;

	return (int) ioctl(fd, XFS_IOC_ATTRLIST_BY_HANDLE, &alhreq);
}

int
fssetdm_by_handle (
	void		*hanp,
	size_t		hlen,
	struct fsdmidata *fsdmidata)
{
	int		fd;
	xfs_fsop_setdm_handlereq_t dmhreq;


	if ((fd = handle_to_fsfd(hanp)) < 0) {
		errno = EBADF;
		return -1;
	}

	dmhreq.hreq.fd       = 0;
	dmhreq.hreq.path     = NULL;
	dmhreq.hreq.oflags   = 0;
	dmhreq.hreq.ihandle  = hanp;
	dmhreq.hreq.ihandlen = hlen;
	dmhreq.hreq.ohandle  = NULL;
	dmhreq.hreq.ohandlen = NULL;

	dmhreq.data = fsdmidata;

	return (int) ioctl(fd, XFS_IOC_FSSETDM_BY_HANDLE, &dmhreq);
}

/*ARGSUSED*/
void
free_handle (
	void		*hanp,
	size_t		hlen)
{
	free (hanp);
}
