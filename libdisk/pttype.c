/*
 * Copyright (c) 2000-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
 * or the like.  Any license provided herein, whether implied or
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

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <dvh.h>
#include "pttype.h"

#define blksize 512

static __uint32_t
twos_complement_32bit_sum(__uint32_t *base, int size)
{
	int i;
	__uint32_t sum = 0;

	size = size / sizeof(__uint32_t);
	for (i = 0; i < size; i++)
		sum = sum - ntohl(base[i]);
	return sum;
}

static int
sgi_parttable(char *base)
{
	__uint32_t csum;
	struct volume_header *vh = (struct volume_header *)base;

	if (ntohl(vh->vh_magic) != VHMAGIC)
		return 0;
	csum = twos_complement_32bit_sum((__uint32_t *)vh,
					 sizeof(struct volume_header));
	return !csum;
}

static int
dos_parttable(char *base)
{
        return (base[510] == 0x55 && base[511] == 0xaa);
}

static int
aix_parttable(char *base)
{
	return (aixlabel(base)->magic == AIX_LABEL_MAGIC ||
		aixlabel(base)->magic == AIX_LABEL_MAGIC_SWAPPED);
}

static int
sun_parttable(char *base)
{
	unsigned short *ush;
	int csum = 0;

	if (sunlabel(base)->magic != SUN_LABEL_MAGIC &&
            sunlabel(base)->magic != SUN_LABEL_MAGIC_SWAPPED)
		return csum;
	ush = ((unsigned short *) (sunlabel(base) + 1)) - 1;
	while (ush >= (unsigned short *)sunlabel(base))
		csum ^= *ush--;
	return !csum;
}


char *
pttype(char *device)
{
	int	fd;
	char	*type = NULL;
	char	buf[blksize];

	if ((fd = open(device, O_RDONLY)) < 0)
		;
	else if (read(fd, buf, blksize) != blksize)
		;
	else {
		if (sgi_parttable(buf))
			type = "SGI";
		else if (sun_parttable(buf))
			type = "Sun";
		else if (aix_parttable(buf))
			type = "AIX";
		else if (dos_parttable(buf))
			type = "DOS";
	}

	if (fd >= 0)
		close(fd);
	return type;
}
