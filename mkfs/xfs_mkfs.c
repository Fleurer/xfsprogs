/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <fstyp.h>
#include <stdio.h>
#include <volume.h>
#include <mountinfo.h>
#include <libxfs.h>

#include "xfs_mkfs.h"
#include "maxtrres.h"
#include "proto.h"

/*
 * Prototypes for internal functions.
 */
static void conflict(char opt, char *tab[], int oldidx, int newidx);
static void illegal(char *value, char *opt);
static void reqval(char opt, char *tab[], int idx);
static void respec(char opt, char *tab[], int idx);
static void unknown(char opt, char *s);
static int  ispow2(unsigned int i);
static int  max_trans_res(xfs_mount_t *mp);

/*
 * option tables for getsubopt calls
 */
char	*bopts[] = {
#define	B_LOG		0
	"log",
#define	B_SIZE		1
	"size",
	NULL
};

char	*dopts[] = {
#define	D_AGCOUNT	0
	"agcount",
#define	D_FILE		1
	"file",
#define	D_NAME		2
	"name",
#define	D_SIZE		3
	"size",
#define D_SUNIT		4
	"sunit",
#define D_SWIDTH	5
	"swidth",
#define D_UNWRITTEN	6
	"unwritten",
#define D_AGSIZE	7
	"agsize",
#define D_SU		8
	"su",
#define D_SW		9
	"sw",
	NULL
};

char	*iopts[] = {
#define	I_ALIGN		0
	"align",
#define	I_LOG		1
	"log",
#define	I_MAXPCT	2
	"maxpct",
#define	I_PERBLOCK	3
	"perblock",
#define	I_SIZE		4
	"size",
	NULL
};

char	*lopts[] = {
#define	L_AGNUM		0
	"agnum",
#define	L_INTERNAL	1
	"internal",
#define	L_SIZE		2
	"size",
#define L_VERSION	3
	"version",
#define L_LSUNIT	4
	"sunit",
#define L_DEV		5
	"logdev",
#ifdef MKFS_SIMULATION
#define	L_FILE		6
	"file",
#define	L_NAME		7
	"name",
#endif
	NULL
};

char	*nopts[] = {
#define	N_LOG		0
	"log",
#define	N_SIZE		1
	"size",
#define	N_VERSION	2
	"version",
	NULL,
};

char	*ropts[] = {
#define	R_EXTSIZE	0
	"extsize",
#define	R_SIZE		1
	"size",
#define	R_DEV		2
	"rtdev",
#ifdef MKFS_SIMULATION
#define	R_FILE		3
	"file",
#define	R_NAME		4
	"name",
#endif
	NULL
};

/*
 * max transaction reservation values
 * version 1:
 * first dimension log(blocksize) (base XFS_MIN_BLOCKSIZE_LOG)
 * second dimension log(inodesize) (base XFS_DINODE_MIN_LOG)
 * version 2:
 * first dimension log(blocksize) (base XFS_MIN_BLOCKSIZE_LOG)
 * second dimension log(inodesize) (base XFS_DINODE_MIN_LOG)
 * third dimension log(dirblocksize) (base XFS_MIN_BLOCKSIZE_LOG)
 */
#define	DFL_B	(XFS_MAX_BLOCKSIZE_LOG + 1 - XFS_MIN_BLOCKSIZE_LOG)
#define	DFL_I	(XFS_DINODE_MAX_LOG + 1 - XFS_DINODE_MIN_LOG)
#define	DFL_D	(XFS_MAX_BLOCKSIZE_LOG + 1 - XFS_MIN_BLOCKSIZE_LOG)

static const int max_trres_v1[DFL_B][DFL_I] = {
	{ MAXTRRES_B9_I8_D9_V1, 0, 0, 0 },
	{ MAXTRRES_B10_I8_D10_V1, MAXTRRES_B10_I9_D10_V1, 0, 0 },
	{ MAXTRRES_B11_I8_D11_V1, MAXTRRES_B11_I9_D11_V1,
	  MAXTRRES_B11_I10_D11_V1, 0 },
	{ MAXTRRES_B12_I8_D12_V1, MAXTRRES_B12_I9_D12_V1,
	  MAXTRRES_B12_I10_D12_V1, MAXTRRES_B12_I11_D12_V1 },
	{ MAXTRRES_B13_I8_D13_V1, MAXTRRES_B13_I9_D13_V1,
	  MAXTRRES_B13_I10_D13_V1, MAXTRRES_B13_I11_D13_V1 },
	{ MAXTRRES_B14_I8_D14_V1, MAXTRRES_B14_I9_D14_V1,
	  MAXTRRES_B14_I10_D14_V1, MAXTRRES_B14_I11_D14_V1 },
	{ MAXTRRES_B15_I8_D15_V1, MAXTRRES_B15_I9_D15_V1,
	  MAXTRRES_B15_I10_D15_V1, MAXTRRES_B15_I11_D15_V1 },
	{ MAXTRRES_B16_I8_D16_V1, MAXTRRES_B16_I9_D16_V1,
	  MAXTRRES_B16_I10_D16_V1, MAXTRRES_B16_I11_D16_V1 },
};

static const int max_trres_v2[DFL_B][DFL_I][DFL_D] = {
	{ { MAXTRRES_B9_I8_D9_V2, MAXTRRES_B9_I8_D10_V2, MAXTRRES_B9_I8_D11_V2,
	    MAXTRRES_B9_I8_D12_V2, MAXTRRES_B9_I8_D13_V2, MAXTRRES_B9_I8_D14_V2,
	    MAXTRRES_B9_I8_D15_V2, MAXTRRES_B9_I8_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0, MAXTRRES_B10_I8_D10_V2, MAXTRRES_B10_I8_D11_V2,
	    MAXTRRES_B10_I8_D12_V2, MAXTRRES_B10_I8_D13_V2,
	    MAXTRRES_B10_I8_D14_V2, MAXTRRES_B10_I8_D15_V2,
	    MAXTRRES_B10_I8_D16_V2 },
	  { 0, MAXTRRES_B10_I9_D10_V2, MAXTRRES_B10_I9_D11_V2,
	    MAXTRRES_B10_I9_D12_V2, MAXTRRES_B10_I9_D13_V2,
	    MAXTRRES_B10_I9_D14_V2, MAXTRRES_B10_I9_D15_V2,
	    MAXTRRES_B10_I9_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0, 0, MAXTRRES_B11_I8_D11_V2, MAXTRRES_B11_I8_D12_V2,
	    MAXTRRES_B11_I8_D13_V2, MAXTRRES_B11_I8_D14_V2,
	    MAXTRRES_B11_I8_D15_V2, MAXTRRES_B11_I8_D16_V2 },
	  { 0, 0, MAXTRRES_B11_I9_D11_V2, MAXTRRES_B11_I9_D12_V2,
	    MAXTRRES_B11_I9_D13_V2, MAXTRRES_B11_I9_D14_V2,
	    MAXTRRES_B11_I9_D15_V2, MAXTRRES_B11_I9_D16_V2 },
	  { 0, 0, MAXTRRES_B11_I10_D11_V2, MAXTRRES_B11_I10_D12_V2,
	    MAXTRRES_B11_I10_D13_V2, MAXTRRES_B11_I10_D14_V2,
	    MAXTRRES_B11_I10_D15_V2, MAXTRRES_B11_I10_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, 0 } },
	{ { 0, 0, 0, MAXTRRES_B12_I8_D12_V2, MAXTRRES_B12_I8_D13_V2,
	    MAXTRRES_B12_I8_D14_V2, MAXTRRES_B12_I8_D15_V2,
	    MAXTRRES_B12_I8_D16_V2 },
	  { 0, 0, 0, MAXTRRES_B12_I9_D12_V2, MAXTRRES_B12_I9_D13_V2,
	    MAXTRRES_B12_I9_D14_V2, MAXTRRES_B12_I9_D15_V2,
	    MAXTRRES_B12_I9_D16_V2 },
	  { 0, 0, 0, MAXTRRES_B12_I10_D12_V2, MAXTRRES_B12_I10_D13_V2,
	    MAXTRRES_B12_I10_D14_V2, MAXTRRES_B12_I10_D15_V2,
	    MAXTRRES_B12_I10_D16_V2 },
	  { 0, 0, 0, MAXTRRES_B12_I11_D12_V2, MAXTRRES_B12_I11_D13_V2,
	    MAXTRRES_B12_I11_D14_V2, MAXTRRES_B12_I11_D15_V2,
	    MAXTRRES_B12_I11_D16_V2 } },
	{ { 0, 0, 0, 0, MAXTRRES_B13_I8_D13_V2, MAXTRRES_B13_I8_D14_V2,
	    MAXTRRES_B13_I8_D15_V2, MAXTRRES_B13_I8_D16_V2 },
	  { 0, 0, 0, 0, MAXTRRES_B13_I9_D13_V2, MAXTRRES_B13_I9_D14_V2,
	    MAXTRRES_B13_I9_D15_V2, MAXTRRES_B13_I9_D16_V2 },
	  { 0, 0, 0, 0, MAXTRRES_B13_I10_D13_V2, MAXTRRES_B13_I10_D14_V2,
	    MAXTRRES_B13_I10_D15_V2, MAXTRRES_B13_I10_D16_V2 },
	  { 0, 0, 0, 0, MAXTRRES_B13_I11_D13_V2, MAXTRRES_B13_I11_D14_V2,
	    MAXTRRES_B13_I11_D15_V2, MAXTRRES_B13_I11_D16_V2 } },
	{ { 0, 0, 0, 0, 0, MAXTRRES_B14_I8_D14_V2, MAXTRRES_B14_I8_D15_V2,
	    MAXTRRES_B14_I8_D16_V2 },
	  { 0, 0, 0, 0, 0, MAXTRRES_B14_I9_D14_V2, MAXTRRES_B14_I9_D15_V2,
	    MAXTRRES_B14_I9_D16_V2 },
	  { 0, 0, 0, 0, 0, MAXTRRES_B14_I10_D14_V2, MAXTRRES_B14_I10_D15_V2,
	    MAXTRRES_B14_I10_D16_V2 },
	  { 0, 0, 0, 0, 0, MAXTRRES_B14_I11_D14_V2, MAXTRRES_B14_I11_D15_V2,
	    MAXTRRES_B14_I11_D16_V2 } },
	{ { 0, 0, 0, 0, 0, 0, MAXTRRES_B15_I8_D15_V2, MAXTRRES_B15_I8_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, MAXTRRES_B15_I9_D15_V2, MAXTRRES_B15_I9_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, MAXTRRES_B15_I10_D15_V2,
	    MAXTRRES_B15_I10_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, MAXTRRES_B15_I11_D15_V2,
	    MAXTRRES_B15_I11_D16_V2 } },
	{ { 0, 0, 0, 0, 0, 0, 0, MAXTRRES_B16_I8_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, MAXTRRES_B16_I9_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, MAXTRRES_B16_I10_D16_V2 },
	  { 0, 0, 0, 0, 0, 0, 0, MAXTRRES_B16_I11_D16_V2, } },
};

/*
 * Use this before we have a superblock, else would use XFS_DTOBT
 */
#define	DTOBT(d)	((xfs_drfsbno_t)((d) >> (blocklog - BBSHIFT)))

/*
 * Use this for block reservations needed for mkfs's conditions
 * (basically no fragmentation).
 */
#define	MKFS_BLOCKRES_INODE	\
	((uint)(XFS_IALLOC_BLOCKS(mp) + (XFS_IN_MAXLEVELS(mp) - 1)))
#define	MKFS_BLOCKRES(rb)	\
	((uint)(MKFS_BLOCKRES_INODE + XFS_DA_NODE_MAXDEPTH + \
	(XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) - 1) + (rb)))

static void
calc_stripe_factors(int dsu, int dsw, int *dsunit, int *dswidth)
{
	if (*dsunit || *dswidth) {
		if (dsu || dsw) {
			fprintf(stderr,
	"su/sw should not be used in conjunction with sunit/swidth\n");
			usage();
		}

		if ((*dsunit && !*dswidth) || (!*dsunit && *dswidth)) {
			fprintf(stderr,
	"both sunit and swidth options have to be specified\n");
			usage();
		}
	}

	if (dsu || dsw) {
		if (*dsunit || *dswidth) {
			fprintf(stderr,
	"sunit/swidth should not be used in conjunction with su/sw\n");
			usage();
		}

		if ((dsu && !dsw) || (!dsu && dsw)) {
			fprintf(stderr,
	"both su and sw options have to be specified\n");
			usage();
		}

		if (dsu % BBSIZE) {
			fprintf(stderr, "su must be a multiple of %d\n",
								BBSIZE);
			usage();
		}

		*dsunit  = (int)BTOBBT(dsu);
		*dswidth = *dsunit * dsw;
	}

	if (*dsunit && (*dswidth % *dsunit != 0)) {
		fprintf(stderr,
	"stripe width (%d) has to be a multiple of the stripe unit (%d)\n",
			*dswidth, *dsunit);
		usage();
	}
}

static int
get_default_blocksize(void)
{
	size_t	pagesize = getpagesize();
	int	i;

	/* default is between 4K and 16K */
	for (i = 12; i <= 16; i++)
		if ((1 << i) == pagesize)
			return pagesize;
	return (1 << XFS_DFL_BLOCKSIZE_LOG);
}

static int
check_overwrite(char *device)
{
	char *type;

	if (device && *device) {
		if ((type = fstype(device)) != NULL) {
			fprintf(stderr,
		"%s: %s appears to contain an existing filesystem (%s).\n",
				progname, device, type);
			return 1;
		}
		if ((type = pttype(device)) != NULL) {
			fprintf(stderr,
		"%s: %s appears to contain a partition table (%s).\n",
				progname, device, type);
			return 1;
		}
	}
	return 0;
}

xfs_dfsbno_t
fixup_log_stripe(
	xfs_mount_t	*mp,
	int		lsflag,
	xfs_dfsbno_t	logstart,
	__uint64_t	agsize,
	int		sunit,
	xfs_drfsbno_t	*logblocks,
	int		blocklog)
{
	__uint64_t	tmp_logblocks;

	logstart = ((logstart + (sunit - 1))/sunit) * sunit;
	/* 
	 * Make sure that the log size is a multiple of the
	 * stripe unit
	 */
	if ((*logblocks % sunit) != 0) {
		if (!lsflag) {
			tmp_logblocks = ((*logblocks + (sunit - 1))
						/ sunit) * sunit;
			/*
			 * If the log is too large, round down
			 * instead of round up
			 */
			if ((tmp_logblocks > XFS_MAX_LOG_BLOCKS) ||
			    ((tmp_logblocks << blocklog) > XFS_MAX_LOG_BYTES)) {
				tmp_logblocks = (*logblocks / sunit) * sunit;
			}
			*logblocks = tmp_logblocks;
		} else {
			fprintf(stderr,
	"internal log size %lld is not a multiple of the log stripe unit %d\n", 
					(long long)*logblocks, sunit);
			usage();
		}
	}

	if (*logblocks > agsize-XFS_FSB_TO_AGBNO(mp,logstart)) {
		fprintf(stderr,
	"Due to stripe alignment, the internal log size %lld is too large.\n"
	"Must fit in allocation group\n",
			(long long)*logblocks);
		usage();
	}
	return logstart;
}

int
main(int argc, char **argv)
{
	__uint64_t		agcount;
	xfs_agf_t		*agf;
	xfs_agi_t		*agi;
	xfs_agnumber_t		agno;
	__uint64_t		agsize;
	xfs_alloc_rec_t		*arec;
	xfs_btree_sblock_t	*block;
	int			blflag;
	int			blocklog;
	int			blocksize;
	int			bsflag;
	int			bsize;
	xfs_buf_t		*buf;
	int			c;
	int			daflag;
	int			dasize;
	xfs_drfsbno_t		dblocks;
	char			*dfile;
	int			dirblocklog;
	int			dirblocksize;
	int			dirversion;
	int                     do_overlap_checks;
	char			*dsize;
	int			dsu;
	int			dsw;
	int			dsunit;
	int			dswidth;
	int			extent_flagging;
	int			force_overwrite;
	int			i;
	int			iaflag;
	int			ilflag;
	int			imaxpct;
	int			imflag;
	int			inodelog;
	int			inopblock;
	int			ipflag;
	int			isflag;
	int			isize;
	char			*label = NULL;
	int			laflag;
	int			lalign;
	int			ldflag;
	int			liflag;
	xfs_agnumber_t		logagno;
	xfs_drfsbno_t		logblocks;
	char			*logfile;
	int			loginternal;
	char			*logsize;
	xfs_dfsbno_t		logstart;
	int			logversion;
	int			lvflag;
	int			lsflag;
	int			lsunit;
	char			*logstripe;
	int			min_logblocks;
	mnt_check_state_t       *mnt_check_state;
	int                     mnt_partition_count;
	xfs_mount_t		*mp;
	xfs_mount_t		mbuf;
	xfs_extlen_t		nbmblocks;
	int			nlflag;
	int			nodsflag;
	xfs_alloc_rec_t		*nrec;
	int			nsflag;
	int			nvflag;
	char			*p;
	char			*protofile;
	char			*protostring;
	int			qflag;
	xfs_drfsbno_t		rtblocks;
	xfs_extlen_t		rtextblocks;
	xfs_drtbno_t		rtextents;
	char			*rtextsize;
	char			*rtfile;
	char			*rtsize;
	xfs_sb_t		*sbp;
	int			sectlog;
	__uint64_t		tmp_agsize;
	uuid_t			uuid;
	int			worst_freelist;
	libxfs_init_t		xi;
	int 			xlv_dsunit;
	int			xlv_dswidth;

	progname = basename(argv[0]);
	agcount = 8;
	blflag = bsflag = 0;
	dasize = daflag = 0;
	blocksize = get_default_blocksize();
	blocklog = libxfs_highbit32(blocksize);
	agsize = daflag = dblocks = 0;
	ilflag = imflag = ipflag = isflag = 0;
	liflag = laflag = lsflag = ldflag = lvflag = 0;
	loginternal = 1;
	logversion = 1;
	logagno = logblocks = rtblocks = 0;
	nlflag = nsflag = nvflag = 0;
	dirblocklog = dirblocksize = dirversion = 0;
	qflag = 0;
	imaxpct = inodelog = inopblock = isize = 0;
	iaflag = XFS_IFLAG_ALIGN;
	bzero(&xi, sizeof(xi));
	xi.notvolok = 1;
	xi.setblksize = 1;
	dfile = logfile = rtfile = NULL;
	dsize = logsize = logstripe = rtsize = rtextsize = protofile = NULL;
	opterr = 0;
	dsu = dsw = dsunit = dswidth = nodsflag = lalign = lsunit = 0;
	do_overlap_checks = 1;
	extent_flagging = 0;
	force_overwrite = 0;
	worst_freelist = 0;

	while ((c = getopt(argc, argv, "b:d:i:l:L:n:p:qr:CfV")) != EOF) {
		switch (c) {
		case 'C':
			do_overlap_checks = 0;
			break;
		case 'f':
			force_overwrite = 1;
			break;
		case 'b':
			p = optarg;
			while (*p != '\0') {
				char	*value;

				switch (getsubopt(&p, (constpp)bopts, &value)) {
				case B_LOG:
					if (!value)
						reqval('b', bopts, B_LOG);
					if (blflag)
						respec('b', bopts, B_LOG);
					if (bsflag)
						conflict('b', bopts, B_SIZE,
							 B_LOG);
					blocklog = atoi(value);
					if (blocklog <= 0)
						illegal(value, "b log");
					blocksize = 1 << blocklog;
					blflag = 1;
					break;
				case B_SIZE:
					if (!value)
						reqval('b', bopts, B_SIZE);
					if (bsflag)
						respec('b', bopts, B_SIZE);
					if (blflag)
						conflict('b', bopts, B_LOG,
							 B_SIZE);
					blocksize = cvtnum(0, value);
					if (blocksize <= 0 ||
					    !ispow2(blocksize))
						illegal(value, "b size");
					blocklog = libxfs_highbit32(blocksize);
					bsflag = 1;
					break;
				default:
					unknown('b', value);
				}
			}
			break;
		case 'd':
			p = optarg;
			while (*p != '\0') {
				char	*value;

				switch (getsubopt(&p, (constpp)dopts, &value)) {
				case D_AGCOUNT:
					if (!value)
						reqval('d', dopts, D_AGCOUNT);
					if (daflag)
						respec('d', dopts, D_AGCOUNT);
					agcount = (__uint64_t)atoll(value);
					if ((__int64_t)agcount <= 0)
						illegal(value, "d agcount");
					daflag = 1;
					break;
				case D_AGSIZE:
					if (!value)
						reqval('d', dopts, D_AGSIZE);
					if (dasize)
						respec('d', dopts, D_AGSIZE);
					if (blflag || bsflag)
						agsize = cvtnum(blocksize,
								value);
					else
						agsize = cvtnum(0, value);
					dasize = 1;
					break;
				case D_FILE:
					if (!value)
						value = "1";
					xi.disfile = atoi(value);
					if (xi.disfile < 0 || xi.disfile > 1)
						illegal(value, "d file");
					if (xi.disfile)
						xi.dcreat = 1;
					break;
				case D_NAME:
					if (!value)
						reqval('d', dopts, D_NAME);
					if (xi.dname)
						respec('d', dopts, D_NAME);
					xi.dname = value;
					break;
				case D_SIZE:
					if (!value)
						reqval('d', dopts, D_SIZE);
					if (dsize)
						respec('d', dopts, D_SIZE);
					dsize = value;
					break;
				case D_SUNIT:
					if (!value)
						reqval('d', dopts, D_SUNIT);
					if (dsunit)
						respec('d', dopts, D_SUNIT);
					if (blflag || bsflag)
						dsunit = cvtnum(blocksize,
								value);
					else
						dsunit = cvtnum(0, value);
					break;
				case D_SWIDTH:
					if (!value)
						reqval('d', dopts, D_SWIDTH);
					if (dswidth)
						respec('d', dopts, D_SWIDTH);
					if (blflag || bsflag)
						dswidth = cvtnum(blocksize,
								 value);
					else
						dswidth = cvtnum(0, value);
					break;
				case D_SU:
					if (!value)
						reqval('d', dopts, D_SU);
					if (dsu)
						respec('d', dopts, D_SU);
					dsu = cvtnum(0, value);
					break;
				case D_SW:
					if (!value)
						reqval('d', dopts, D_SW);
					if (dsw)
						respec('d', dopts, D_SW);
					dsw = cvtnum(0, value);
					break;
				case D_UNWRITTEN:
					if (!value)
					    reqval('d', dopts, D_UNWRITTEN);
					i = atoi(value);
					if (i < 0 || i > 1)
					    illegal(value, "d unwritten");
					extent_flagging = i;
					break;
				default:
					unknown('d', value);
				}
			}
			break;
		case 'i':
			p = optarg;
			while (*p != '\0') {
				char	*value;

				switch (getsubopt(&p, (constpp)iopts, &value)) {
				case I_ALIGN:
					if (!value)
						value = "1";
					iaflag = atoi(value);
					if (iaflag < 0 || iaflag > 1)
						illegal(value, "i align");
					break;
				case I_LOG:
					if (!value)
						reqval('i', iopts, I_LOG);
					if (ilflag)
						respec('i', iopts, I_LOG);
					if (ipflag)
						conflict('i', iopts, I_PERBLOCK,
							 I_LOG);
					if (isflag)
						conflict('i', iopts, I_SIZE,
							 I_LOG);
					inodelog = atoi(value);
					if (inodelog <= 0)
						illegal(value, "i log");
					isize = 1 << inodelog;
					ilflag = 1;
					break;
				case I_MAXPCT:
					if (!value)
						reqval('i', iopts, I_MAXPCT);
					if (imflag)
						respec('i', iopts, I_MAXPCT);
					imaxpct = atoi(value);
					if (imaxpct < 0 || imaxpct > 100)
						illegal(value, "i maxpct");
					imflag = 1;
					break;
				case I_PERBLOCK:
					if (!value)
						reqval('i', iopts, I_PERBLOCK);
					if (ilflag)
						conflict('i', iopts, I_LOG,
							 I_PERBLOCK);
					if (ipflag)
						respec('i', iopts, I_PERBLOCK);
					if (isflag)
						conflict('i', iopts, I_SIZE,
							 I_PERBLOCK);
					inopblock = atoi(value);
					if (inopblock <
						XFS_MIN_INODE_PERBLOCK ||
					    !ispow2(inopblock))
						illegal(value, "i perblock");
					ipflag = 1;
					break;
				case I_SIZE:
					if (!value)
						reqval('i', iopts, I_SIZE);
					if (ilflag)
						conflict('i', iopts, I_LOG,
							 I_SIZE);
					if (ipflag)
						conflict('i', iopts, I_PERBLOCK,
							 I_SIZE);
					if (isflag)
						respec('i', iopts, I_SIZE);
					isize = cvtnum(0, value);
					if (isize <= 0 || !ispow2(isize))
						illegal(value, "i size");
					inodelog = libxfs_highbit32(isize);
					isflag = 1;
					break;
				default:
					unknown('i', value);
				}
			}
			break;
		case 'l':
			p = optarg;
			while (*p != '\0') {
				char	*value;

				switch (getsubopt(&p, (constpp)lopts, &value)) {
				case L_AGNUM:
					if (laflag)
						respec('l', lopts, L_AGNUM);

					if (ldflag) 
						conflict('l', lopts, L_AGNUM, L_DEV);

					logagno = atoi(value);
					laflag = 1;
					break;
				case L_DEV:
					if (!value) {
						fprintf (stderr, "Must specify log device\n");
						usage();
					}

					if (laflag)
						conflict('l', lopts, L_AGNUM, L_DEV);

					if (liflag)
						conflict('l', lopts, L_INTERNAL, L_DEV);
					
					ldflag = 1;
					loginternal = 0;
					logfile = value;
					xi.logname = value;
					break;
#ifdef HAVE_VOLUME_MANAGER
				case L_FILE:
					if (!value)
						value = "1";
					if (loginternal)
						conflict('l', lopts, L_INTERNAL,
							 L_FILE);
					xi.lisfile = atoi(value);
					if (xi.lisfile < 0 || xi.lisfile > 1)
						illegal(value, "l file");
					if (xi.lisfile)
						xi.lcreat = 1;
					break;
#endif
				case L_INTERNAL:
					if (!value)
						value = "1";

					if (ldflag) 
						conflict('l', lopts, L_INTERNAL, L_DEV);
#ifdef HAVE_VOLUME_MANAGER
					if (xi.logname)
						conflict('l', lopts, L_NAME,
							 L_INTERNAL);
					if (xi.lisfile)
						conflict('l', lopts, L_FILE,
							 L_INTERNAL);
#endif
					if (liflag)
						respec('l', lopts, L_INTERNAL);
					loginternal = atoi(value);
					if (loginternal < 0 || loginternal > 1)
						illegal(value, "l internal");
					liflag = 1;
					break;
				case L_LSUNIT:
					if (!value)
						reqval('l', lopts, L_LSUNIT);
					if (logstripe)
						respec('l', lopts, L_LSUNIT);
					logstripe = value;
					break;
#ifdef HAVE_VOLUME_MANAGER
				case L_NAME:
					if (!value)
						reqval('l', lopts, L_NAME);
					if (loginternal)
						conflict('l', lopts, L_INTERNAL,
							 L_NAME);
					if (xi.logname)
						respec('l', lopts, L_NAME);
					xi.logname = value;
					break;
#endif
				case L_VERSION:
					if (!value)
						reqval('n', nopts, L_VERSION);
					if (lvflag)
						respec('n', nopts, L_VERSION);
					logversion = atoi(value);
					if (logversion < 1 || logversion > 2)
						illegal(value, "l version");
					lvflag = 1;
					break;
				case L_SIZE:
					if (!value)
						reqval('l', lopts, L_SIZE);
					if (logsize)
						respec('l', lopts, L_SIZE);
					logsize = value;
					lsflag = 1;
					break;
				default:
					unknown('l', value);
				}
			}
			break;
		case 'L':
			if (strlen(optarg) > sizeof(sbp->sb_fname))
				illegal(optarg, "L");
			label = optarg;
			break;
		case 'n':
			p = optarg;
			while (*p != '\0') {
				char	*value;

				switch (getsubopt(&p, (constpp)nopts, &value)) {
				case N_LOG:
					if (!value)
						reqval('n', nopts, N_LOG);
					if (nlflag)
						respec('n', nopts, N_LOG);
					if (nsflag)
						conflict('n', nopts, N_SIZE,
							 N_LOG);
					dirblocklog = atoi(value);
					if (dirblocklog <= 0)
						illegal(value, "n log");
					dirblocksize = 1 << dirblocklog;
					nlflag = 1;
					break;
				case N_SIZE:
					if (!value)
						reqval('n', nopts, N_SIZE);
					if (nsflag)
						respec('n', nopts, N_SIZE);
					if (nlflag)
						conflict('n', nopts, N_LOG,
							 N_SIZE);
					dirblocksize = cvtnum(0, value);
					if (dirblocksize <= 0 ||
					    !ispow2(dirblocksize))
						illegal(value, "n size");
					dirblocklog =
						libxfs_highbit32(dirblocksize);
					nsflag = 1;
					break;
				case N_VERSION:
					if (!value)
						reqval('n', nopts, N_VERSION);
					if (nvflag)
						respec('n', nopts, N_VERSION);
					dirversion = atoi(value);
					if (dirversion < 1 || dirversion > 2)
						illegal(value, "n version");
					nvflag = 1;
					break;
				default:
					unknown('n', value);
				}
			}
			break;
		case 'p':
			if (protofile)
				respec('p', 0, 0);
			protofile = optarg;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			p = optarg;
			while (*p != '\0') {
				char	*value;

				switch (getsubopt(&p, (constpp)ropts, &value)) {
				case R_EXTSIZE:
					if (!value)
						reqval('r', ropts, R_EXTSIZE);
					if (rtextsize)
						respec('r', ropts, R_EXTSIZE);
					rtextsize = value;
					break;
				case R_DEV:
					if (!value)
						reqval('r', ropts, R_DEV);
					xi.rtname = value;
					break;
#ifdef HAVE_VOLUME_MANAGER
				case R_FILE:
					if (!value)
						value = "1";
					xi.risfile = atoi(value);
					if (xi.risfile < 0 || xi.risfile > 1)
						illegal(value, "r file");
					if (xi.risfile)
						xi.rcreat = 1;
					break;
				case R_NAME:
					if (!value)
						reqval('r', ropts, R_NAME);
					if (xi.rtname)
						respec('r', ropts, R_NAME);
					xi.rtname = value;
					break;
#endif
				case R_SIZE:
					if (!value)
						reqval('r', ropts, R_SIZE);
					if (rtsize)
						respec('r', ropts, R_SIZE);
					rtsize = value;
					break;

				default:
					unknown('r', value);
				}
			}
			break;
		case 'V':
			printf("%s version %s\n", progname, VERSION);
			exit(0);
		case '?':
			unknown(optopt, "");
		}
	}
	if (argc - optind > 1) {
		fprintf(stderr, "extra arguments\n");
		usage();
	} else if (argc - optind == 1) {
		dfile = xi.volname = argv[optind];
		if (xi.dname) {
			fprintf(stderr,
				"cannot specify both %s and -d name=%s\n",
				xi.volname, xi.dname);
			usage();
		}
	} else
		dfile = xi.dname;
	/* option post-processing */
	if (blocksize < XFS_MIN_BLOCKSIZE || blocksize > XFS_MAX_BLOCKSIZE) {
		fprintf(stderr, "illegal block size %d\n", blocksize);
		usage();
	}
	if (!nvflag)
		dirversion = (nsflag || nlflag) ? 2 : XFS_DFL_DIR_VERSION;
	switch (dirversion) {
	case 1:
		if ((nsflag || nlflag) && dirblocklog != blocklog) {
			fprintf(stderr, "illegal directory block size %d\n",
				dirblocksize);
			usage();
		}
		break;
	case 2:
		if (nsflag || nlflag) {
			if (dirblocksize < blocksize ||
			    dirblocksize > XFS_MAX_BLOCKSIZE) {
				fprintf(stderr,
					"illegal directory block size %d\n",
					dirblocksize);
				usage();
			}
		} else {
			if (blocksize < (1 << XFS_MIN_REC_DIRSIZE))
				dirblocklog = XFS_MIN_REC_DIRSIZE;
			else
				dirblocklog = blocklog;
			dirblocksize = 1 << dirblocklog;
		}
		break;
	}

	if (daflag && dasize) {
		fprintf(stderr,
	"both -d agcount= and agsize= specified, use one or the other\n");
		usage();
	}

	if (!daflag)
		agcount = 8;

	if (xi.disfile && (!dsize || !xi.dname)) {
		fprintf(stderr,
			"if -d file then -d name and -d size are required\n");
		usage();
	}
	if (dsize) {
		__uint64_t dbytes;

		dbytes = cvtnum(blocksize, dsize);
		if (dbytes % XFS_MIN_BLOCKSIZE) {
			fprintf(stderr,
			"illegal data length %lld, not a multiple of %d\n",
				(long long)dbytes, XFS_MIN_BLOCKSIZE);
			usage();
		}
		dblocks = (xfs_drfsbno_t)(dbytes >> blocklog);
		if (dbytes % blocksize)
			fprintf(stderr,
	"warning: data length %lld not a multiple of %d, truncated to %lld\n",
				(long long)dbytes, blocksize,
				(long long)(dblocks << blocklog));
	}
	if (ipflag) {
		inodelog = blocklog - libxfs_highbit32(inopblock);
		isize = 1 << inodelog;
	} else if (!ilflag && !isflag) {
		inodelog = XFS_DINODE_DFL_LOG;
		isize = 1 << inodelog;
	}
#ifdef HAVE_VOLUME_MANAGER
	if (xi.lisfile && (!logsize || !xi.logname)) {
		fprintf(stderr,
			"if -l file then -l name and -l size are required\n");
		usage();
	}
#endif
	if (logsize) {
		__uint64_t logbytes;

		logbytes = cvtnum(blocksize, logsize);
		if (logbytes % XFS_MIN_BLOCKSIZE) {
			fprintf(stderr,
			"illegal log length %lld, not a multiple of %d\n",
				(long long)logbytes, XFS_MIN_BLOCKSIZE);
			usage();
		}
		logblocks = (xfs_drfsbno_t)(logbytes >> blocklog);
		if (logbytes % blocksize)
			fprintf(stderr,
	"warning: log length %lld not a multiple of %d, truncated to %lld\n",
				(long long)logbytes, blocksize,
				(long long)(logblocks << blocklog));
	}
	if (logstripe) {
		lsunit = cvtnum(blocksize, logstripe);
	}
#ifdef HAVE_VOLUME_MANAGER
	if (xi.risfile && (!rtsize || !xi.rtname)) {
		fprintf(stderr,
			"if -r file then -r name and -r size are required\n");
		usage();
	}
#endif
	if (rtsize) {
		__uint64_t rtbytes;

		rtbytes = cvtnum(blocksize, rtsize);
		if (rtbytes % XFS_MIN_BLOCKSIZE) {
			fprintf(stderr,
			"illegal rt length %lld, not a multiple of %d\n",
				(long long)rtbytes, XFS_MIN_BLOCKSIZE);
			usage();
		}
		rtblocks = (xfs_drfsbno_t)(rtbytes >> blocklog);
		if (rtbytes % blocksize)
			fprintf(stderr,
	"warning: rt length %lld not a multiple of %d, truncated to %lld\n",
				(long long)rtbytes, blocksize,
				(long long)(rtblocks << blocklog));
	}
	/*
	 * If specified, check rt extent size against its constraints.
	 */
	if (rtextsize) {
		__uint64_t rtextbytes;

		rtextbytes = cvtnum(blocksize, rtextsize);
		if (rtextbytes % blocksize) {
			fprintf(stderr,
			"illegal rt extent size %lld, not a multiple of %d\n",
				(long long)rtextbytes, blocksize);
			usage();
		}
		if (rtextbytes > XFS_MAX_RTEXTSIZE) {
			fprintf(stderr,
				"rt extent size %s too large, maximum %d\n",
				rtextsize, XFS_MAX_RTEXTSIZE);
			usage();
		}
		if (rtextbytes < XFS_MIN_RTEXTSIZE) {
			fprintf(stderr,
				"rt extent size %s too small, minimum %d\n",
				rtextsize, XFS_MIN_RTEXTSIZE);
			usage();
		}
		rtextblocks = (xfs_extlen_t)(rtextbytes >> blocklog);
	} else {
		/*
		 * If realtime extsize has not been specified by the user,
		 * and the underlying volume is striped, then set rtextblocks
		 * to the stripe width.
		 */
		int dummy1, rswidth;
		__uint64_t rtextbytes;
		dummy1 = rswidth = 0;
                
                if (!xi.disfile)
		        get_subvol_stripe_wrapper(dfile, SVTYPE_RT, &dummy1, 
						    &rswidth);

		/* check that rswidth is a multiple of fs blocksize */
		if (rswidth && !(BBTOB(rswidth) % blocksize)) {
			rswidth = DTOBT(rswidth);
			rtextbytes = rswidth << blocklog;
			if (XFS_MIN_RTEXTSIZE <= rtextbytes &&
                                (rtextbytes <= XFS_MAX_RTEXTSIZE))  {
       		                 rtextblocks = rswidth;
			} else {
				rtextblocks = XFS_DFL_RTEXTSIZE >> blocklog;
			}
		} else
			rtextblocks = XFS_DFL_RTEXTSIZE >> blocklog;
	}

	/*
	 * Check some argument sizes against mins, maxes.
	 */
	if (isize > blocksize / XFS_MIN_INODE_PERBLOCK ||
	    isize < XFS_DINODE_MIN_SIZE ||
	    isize > XFS_DINODE_MAX_SIZE) {
		int	maxsz;

		fprintf(stderr, "illegal inode size %d\n", isize);
		maxsz = MIN(blocksize / XFS_MIN_INODE_PERBLOCK,
			    XFS_DINODE_MAX_SIZE);
		if (XFS_DINODE_MIN_SIZE == maxsz)
			fprintf(stderr,
			"allowable inode size with %d byte blocks is %d\n",
				blocksize, XFS_DINODE_MIN_SIZE);
		else
			fprintf(stderr,
	"allowable inode size with %d byte blocks is between %d and %d\n",
				blocksize, XFS_DINODE_MIN_SIZE, maxsz);
		usage();
	}

	calc_stripe_factors(dsu, dsw, &dsunit, &dswidth);

	/* other global variables */
	sectlog = 9;		/* i.e. 512 bytes */

	/*
	 * Initialize.  This will open the log and rt devices as well.
	 */
	if (!libxfs_init(&xi))
		usage();
	if (!xi.ddev) {
		fprintf(stderr, "no device name given in argument list\n");
		usage();
	}

	/*
	 * Ok, Linux only has a 1024-byte resolution on device _size_,
	 * and the sizes below are in basic 512-byte blocks,
	 * so if we have (size % 2), on any partition, we can't get
	 * to the last 512 bytes.  Just chop it down by a block.
	 */

	xi.dsize -= (xi.dsize % 2);
	xi.rtsize -= (xi.rtsize % 2);
	xi.logBBsize -= (xi.logBBsize % 2);

	if (!force_overwrite) {
		if (check_overwrite(dfile) ||
		    check_overwrite(logfile) ||
		    check_overwrite(xi.rtname)) {
			fprintf(stderr, "%s: "
				"Use the -f option to force overwrite.\n",
				progname);
			exit(1);
		}
	}

	if (!xi.disfile && do_overlap_checks) {
	        /*
		 * do partition overlap check
		 * If this is a straight file we assume that it's been created
		 * before the call to mnt_check_init()
		 */

                if (mnt_check_init(&mnt_check_state) == -1) {
                        fprintf(stderr,
				"unable to initialize mount checking "
				"routines, bypassing protection checks.\n");
		} else {
		        mnt_partition_count = mnt_find_mount_conflicts(
				mnt_check_state, dfile);

			/* 
			 * ignore -1 return codes, since 3rd party devices
			 * may not be part of hinv.
			 */
			if (mnt_partition_count > 0) {
			        if (mnt_causes_test(mnt_check_state, MNT_CAUSE_MOUNTED)) {
				        fprintf(stderr, "%s: "
						"%s is already in use.\n",
						progname, dfile);
				} else if (mnt_causes_test(mnt_check_state, MNT_CAUSE_OVERLAP)) {
				        fprintf(stderr, "%s: "
						"%s overlaps partition(s) "
						"already in use.\n",
						progname, dfile);
				} else {
				        mnt_causes_show(mnt_check_state, stderr, progname);
				}
				fprintf(stderr, "\n");
				fflush(stderr);
				mnt_plist_show(mnt_check_state, stderr, progname);
				fprintf(stderr, "\n");
			}
			mnt_check_end(mnt_check_state);
			if (mnt_partition_count > 0) {
			        usage();
			}
		}
	}

	if (!liflag && !ldflag)
		loginternal = xi.logdev == 0;
	if (xi.logname)
		logfile = xi.logname;
	else if (loginternal)
		logfile = "internal log";
	else if (xi.volname && xi.logdev)
		logfile = "volume log";
	else if (!ldflag) {
		fprintf(stderr, "no log subvolume or internal log\n");
		usage();
	}
	if (xi.rtname)
		rtfile = xi.rtname;
	else
	if (xi.volname && xi.rtdev)
		rtfile = "volume rt";
	else if (!xi.rtdev)
		rtfile = "none";
	if (dsize && xi.dsize > 0 && dblocks > DTOBT(xi.dsize)) {
		fprintf(stderr,
"size %s specified for data subvolume is too large, maximum is %lld blocks\n",
			dsize, (long long)DTOBT(xi.dsize));
		usage();
	} else if (!dsize && xi.dsize > 0)
		dblocks = DTOBT(xi.dsize);
	else if (!dsize) {
		fprintf(stderr, "can't get size of data subvolume\n");
		usage();
	} 
	if (dblocks < XFS_MIN_DATA_BLOCKS) {
		fprintf(stderr,
		"size %lld of data subvolume is too small, minimum %d blocks\n",
			(long long)dblocks, XFS_MIN_DATA_BLOCKS);
		usage();
	}
	if (xi.logdev && loginternal) {
		fprintf(stderr, "can't have both external and internal logs\n");
		usage();
	}
	if (dirversion == 1)
		i = max_trres_v1[blocklog - XFS_MIN_BLOCKSIZE_LOG]
				[inodelog - XFS_DINODE_MIN_LOG];
	else
		i = max_trres_v2[blocklog - XFS_MIN_BLOCKSIZE_LOG]
				[inodelog - XFS_DINODE_MIN_LOG]
				[dirblocklog - XFS_MIN_BLOCKSIZE_LOG];
	min_logblocks = MAX(XFS_MIN_LOG_BLOCKS, i * XFS_MIN_LOG_FACTOR);
	if (logsize && xi.logBBsize > 0 && logblocks > DTOBT(xi.logBBsize)) {
		fprintf(stderr,
"size %s specified for log subvolume is too large, maximum is %lld blocks\n",
			logsize, (long long)DTOBT(xi.logBBsize));
		usage();
	} else if (!logsize && xi.logBBsize > 0)
		logblocks = DTOBT(xi.logBBsize);
	else if (logsize && !xi.logdev && !loginternal) {
		fprintf(stderr,
			"size specified for non-existent log subvolume\n");
		usage();
	} else if (loginternal && logsize && logblocks >= dblocks) {
		fprintf(stderr, "size %lld too large for internal log\n",
			(long long)logblocks);
		usage();
	} else if (!loginternal && !xi.logdev)
		logblocks = 0;
	else if (loginternal && !logsize) {
		/*
		 * logblocks grows from min_logblocks to XFS_MAX_LOG_BLOCKS
		 * at 1TB
		 *
		 * 8192 = 1TB / MAX_LOG_BYTES
		 */
		logblocks = (dblocks << blocklog) / 8192;
		logblocks = logblocks >> blocklog;
		logblocks = MAX(min_logblocks, logblocks);
                logblocks = MAX(logblocks,
				MAX(XFS_DFL_LOG_SIZE, i * XFS_DFL_LOG_FACTOR));
                logblocks = MIN(logblocks, XFS_MAX_LOG_BLOCKS); 
		if ((logblocks << blocklog) > XFS_MAX_LOG_BYTES) {
			logblocks = XFS_MAX_LOG_BYTES >> blocklog;
		}
        } 
	if (logblocks < min_logblocks) {
		fprintf(stderr,
		"log size %lld blocks too small, minimum size is %d blocks\n",
			(long long)logblocks, min_logblocks);
		usage();
	}
	if (logblocks > XFS_MAX_LOG_BLOCKS) {
		fprintf(stderr,
		"log size %lld blocks too large, maximum size is %d blocks\n",
			(long long)logblocks, XFS_MAX_LOG_BLOCKS);
		usage();
	}
	if ((logblocks << blocklog) > XFS_MAX_LOG_BYTES) {
		fprintf(stderr,
		"log size %lld bytes too large, maximum size is %d bytes\n",
			(long long)(logblocks << blocklog), XFS_MAX_LOG_BYTES);
		usage();
	}
	if (rtsize && xi.rtsize > 0 && rtblocks > DTOBT(xi.rtsize)) {
		fprintf(stderr,
"size %s specified for rt subvolume is too large, maximum is %lld blocks\n",
			rtsize, (long long)DTOBT(xi.rtsize));
		usage();
	} else if (!rtsize && xi.rtsize > 0)
		rtblocks = DTOBT(xi.rtsize);
	else if (rtsize && !xi.rtdev) {
		fprintf(stderr,
			"size specified for non-existent rt subvolume\n");
		usage();
	}
	if (xi.rtdev) {
		rtextents = rtblocks / rtextblocks;
		nbmblocks = (xfs_extlen_t)howmany(rtextents, NBBY * blocksize);
	} else {
		rtextents = rtblocks = 0;
		nbmblocks = 0;
	}

	if (dasize) {
		/*
		 * If the specified agsize isn't a multiple of fs blks,
		 * complain.
		 */
		if (agsize % blocksize) {
			fprintf(stderr,
			"agsize (%lld) not a multiple of fs blk size (%d)\n",
				(long long)agsize, blocksize);
			usage();
		}

		agsize /= blocksize;

		/*
		 * If the specified agsize is too small, or too large,
		 * complain.
		 */
		if (agsize < XFS_AG_MIN_BLOCKS(blocklog)) {
			fprintf(stderr,
			"agsize (%lldb) too small, need at least %lld blocks\n",
				(long long)agsize,
				(long long)XFS_AG_MIN_BLOCKS(blocklog));
			usage();
		}

		if (agsize > XFS_AG_MAX_BLOCKS(blocklog)) {
			fprintf(stderr,
			"agsize (%lldb) too big, maximum is %lld blocks\n",
				(long long)agsize,
				(long long)XFS_AG_MAX_BLOCKS(blocklog));
			usage();
		}

		if (agsize > dblocks)  {
			fprintf(stderr,
			"agsize (%lldb) too big, data area is %lld blocks\n",
				(long long)agsize, (long long)dblocks);
			usage();
		}

		agcount = dblocks / agsize + (dblocks % agsize != 0);
	} else {
		agsize = dblocks / agcount + (dblocks % agcount != 0);
	}

	/*
	 * If the ag size is too small, complain if agcount/agsize was
	 * specified, and fix it otherwise.
	 */
	if (agsize < XFS_AG_MIN_BLOCKS(blocklog)) {
		if (daflag || dasize) {
			fprintf(stderr,
				"too many allocation groups for size = %lld\n",
				(long long)agsize);
			fprintf(stderr, "need at most %lld allocation groups\n",
				(long long)
				(dblocks / XFS_AG_MIN_BLOCKS(blocklog) +
				(dblocks % XFS_AG_MIN_BLOCKS(blocklog) != 0)));
			usage();
		}
		agsize = XFS_AG_MIN_BLOCKS(blocklog);
		if (dblocks < agsize)
			agcount = 1;
		else
			agcount = dblocks / agsize;

		agsize = dblocks / agcount + (dblocks % agcount != 0);
	}

	/*
	 * If the ag size is too large, complain if agcount/agsize was
	 * specified, and fix it otherwise.
	 */
	else if (agsize > XFS_AG_MAX_BLOCKS(blocklog)) {
		if (daflag || dasize) {
			fprintf(stderr, "too few allocation groups for size = %lld\n",
				(long long)agsize);
			fprintf(stderr,
				"need at least %lld allocation groups\n",
				(long long)
				(dblocks / XFS_AG_MAX_BLOCKS(blocklog) + 
				(dblocks % XFS_AG_MAX_BLOCKS(blocklog) != 0)));
			usage();
		}
		agsize = XFS_AG_MAX_BLOCKS(blocklog);
		agcount = dblocks / agsize + (dblocks % agsize != 0);
	}

	/*
	 * If the last AG is too small, reduce the filesystem size
	 * and drop the blocks.
	 */
	if ( dblocks % agsize != 0 &&
	     (dblocks % agsize < XFS_AG_MIN_BLOCKS(blocklog))) {
		dblocks = (xfs_drfsbno_t)((agcount - 1) * agsize);
		agcount--;
		ASSERT(agcount != 0);
	}

	/*
	 * If agcount was not specified, and agsize is larger than
	 * we'd like, make it the size we want.
	 */
	if (!daflag && !dasize &&
	    (agsize > XFS_AG_BEST_BLOCKS(blocklog,dblocks))) {
		agsize = XFS_AG_BEST_BLOCKS(blocklog,dblocks);
		agcount = dblocks / agsize + (dblocks % agsize != 0);
		/*
		 * If the last AG is too small, reduce the filesystem size
		 * and drop the blocks.
		 */
		if ( dblocks % agsize != 0 &&
		    (dblocks % agsize < XFS_AG_MIN_BLOCKS(blocklog))) {
			dblocks = (xfs_drfsbno_t)((agcount - 1) * agsize);
			agcount--;
			ASSERT(agcount != 0);
		}
	}

	/*
	 * If agcount is too large, make it smaller.
	 */
	if (agcount > XFS_MAX_AGNUMBER + 1) {
		agcount = XFS_MAX_AGNUMBER + 1;
		agsize = dblocks / agcount + (dblocks % agcount != 0);

		if (dasize || daflag)
			fprintf(stderr,
		"agsize set to %lld, agcount %lld > max (%lld)\n",
				(long long)agsize, (long long)agcount,
				(long long)XFS_MAX_AGNUMBER+1);

		if (agsize > XFS_AG_MAX_BLOCKS(blocklog)) {
			/*
			 * We're confused.
			 */
			fprintf(stderr, "%s: can't compute agsize/agcount\n",
				progname);
			exit(1);
		}
	}

	xlv_dsunit = xlv_dswidth = 0;
        if (!xi.disfile)
	        get_subvol_stripe_wrapper(dfile, SVTYPE_DATA, &xlv_dsunit, 
				&xlv_dswidth);
	if (dsunit) {

		if (xlv_dsunit && xlv_dsunit != dsunit) {
			fprintf(stderr, "%s: "
  "Specified data stripe unit %d is not the same as the xlv stripe unit %d\n", 
				progname, dsunit, xlv_dsunit);
		}
		if (xlv_dswidth && xlv_dswidth != dswidth) {
			fprintf(stderr, "%s: "
"Specified data stripe width %d is not the same as the xlv stripe width %d\n",
				progname, dswidth, xlv_dswidth);
		}
	} else {
		dsunit = xlv_dsunit;
		dswidth = xlv_dswidth;
		nodsflag = 1;
	}

	/*
	 * If dsunit is a multiple of fs blocksize, then check that is a
	 * multiple of the agsize too
	 */
	if (dsunit && !(BBTOB(dsunit) % blocksize) && 
	    dswidth && !(BBTOB(dswidth) % blocksize)) {

		/* convert from 512 byte blocks to fs blocksize */
		dsunit = DTOBT(dsunit);
		dswidth = DTOBT(dswidth);

		/* 
		 * agsize is not a multiple of dsunit
		 */
		if ((agsize % dsunit) != 0) {
                	/*
                 	 * Round up to stripe unit boundary. Also make sure 
			 * that agsize is still larger than 
			 * XFS_AG_MIN_BLOCKS(blocklog)
		 	 */
                	tmp_agsize = ((agsize + (dsunit - 1))/ dsunit) * dsunit;
			/*
			 * Round down to stripe unit boundary if rounding up
			 * created an AG size that is larger than the AG max.
			 */
			if (tmp_agsize > XFS_AG_MAX_BLOCKS(blocklog))
				tmp_agsize = ((agsize) / dsunit) * dsunit;
                	if ((tmp_agsize >= XFS_AG_MIN_BLOCKS(blocklog)) &&
			    (tmp_agsize <= XFS_AG_MAX_BLOCKS(blocklog)) &&
			    !daflag) {
				agsize = tmp_agsize;
				agcount = dblocks/agsize + 
						(dblocks % agsize != 0);
				if (dasize || daflag)
					fprintf(stderr,
					"agsize rounded to %lld, swidth = %d\n",
						(long long)agsize, dswidth);
                	} else {
				if (nodsflag) {
					dsunit = dswidth = 0;
				} else { 
					fprintf(stderr,
"Allocation group size %lld is not a multiple of the stripe unit %d\n",
						(long long)agsize, dsunit);
					exit(1);
				}
        		}
		}
		if ((agsize % dswidth) == 0) {
			/* This is a non-optimal configuration because all AGs
			 * start on the same disk in the stripe.  Decreasing
			 * the AG size by one sunit will guarantee that this
			 * does not happen
			 */
			tmp_agsize = agsize - dsunit;
			if (tmp_agsize < XFS_AG_MIN_BLOCKS(blocklog))
				tmp_agsize = agsize + dsunit;
			if (daflag || dasize) {
				fprintf(stderr,
"Warning: AG size is a multiple of stripe width.  This can cause performance\n"
"problems by aligning all AGs on the same disk.  To avoid this, rerun mkfs with\n"
"an AG size that is one stripe unit smaller, for example %lld\n",
					tmp_agsize);
			} else {
				agsize = tmp_agsize;
				agcount = dblocks/agsize + (dblocks % agsize != 0);
				/*
				 * If the last AG is too small, reduce the
				 * filesystem size and drop the blocks.
				 */
				if ( dblocks % agsize != 0 &&
				    (dblocks % agsize <
				    XFS_AG_MIN_BLOCKS(blocklog))) {
					dblocks = (xfs_drfsbno_t)((agcount - 1) * agsize);
					agcount--;
					ASSERT(agcount != 0);
				}
			}
		}
	} else {
		if (nodsflag)
			dsunit = dswidth = 0;
		else { 
			fprintf(stderr, "%s: "
"Stripe unit(%d) or stripe width(%d) is not a multiple of the block size(%d)\n",
				progname, dsunit, dswidth, blocksize); 	
			exit(1);
		}
	}

	/*
	 * check that log sunit is modulo fsblksize or default it to dsunit.
	 */

	if (lsunit) {
		if (lsunit % blocksize != 0) {
			fprintf(stderr,
"log stripe unit (%d) is not a multiple of the block size (%d)\n",
			lsunit, blocksize);
			exit(1);
		}
	} else if (dsunit) {
		/* lsunit is in bytes here, dsunit is fs blocks */
		lsunit = dsunit << blocklog;
	}

	if (lsunit > 256 * 1024) {
		fprintf(stderr,
"log stripe unit (%d) is too large for kernel to handle\n", lsunit);
		exit(1);
	}

	protostring = setup_proto(protofile);
	bsize = 1 << (blocklog - BBSHIFT);
	mp = &mbuf;
	sbp = &mp->m_sb;
	bzero(mp, sizeof(xfs_mount_t));
	sbp->sb_blocklog = (__uint8_t)blocklog;
	sbp->sb_agblklog = (__uint8_t)libxfs_log2_roundup((unsigned int)agsize);
	mp->m_blkbb_log = sbp->sb_blocklog - BBSHIFT;
	if (loginternal) {
		/*
		 * Readjust the log size to fit within an AG if it was sized
		 * automaticly.
		 */
		if (!logsize) {
			logblocks = MIN(logblocks,
					agsize - XFS_PREALLOC_BLOCKS(mp));
		}
		if (logblocks > agsize - XFS_PREALLOC_BLOCKS(mp)) {
			fprintf(stderr,
	"internal log size %lld too large, must fit in allocation group\n",
				(long long)logblocks);
			usage();
		}
		if (laflag) {
			if (logagno >= agcount) {
				fprintf(stderr,
			"log ag number %d too large, must be less than %lld\n",
					logagno, (long long)agcount);
				usage();
			}
		} else
			logagno = (xfs_agnumber_t)(agcount / 2);

		logstart = XFS_AGB_TO_FSB(mp, logagno, XFS_PREALLOC_BLOCKS(mp));
		/*
		 * Align the logstart at stripe unit boundary.
		 */

		if (lsunit && ((logstart % XFS_B_TO_FSB(mp, lsunit)) != 0)) {
			logstart = fixup_log_stripe(mp, lsflag, logstart,
					agsize, XFS_B_TO_FSB(mp, lsunit),
					&logblocks, blocklog);
			lalign = 1;
		} else if (dsunit && ((logstart % dsunit) != 0)) {
			logstart = fixup_log_stripe(mp, lsflag, logstart,
					agsize, dsunit, &logblocks,
					blocklog);
			lalign = 1;
		}
	} else
		logstart = 0;

	if (label)
		strncpy(sbp->sb_fname, label, sizeof(sbp->sb_fname));
	sbp->sb_magicnum = XFS_SB_MAGIC;
	sbp->sb_blocksize = blocksize;
	sbp->sb_dblocks = dblocks;
	sbp->sb_rblocks = rtblocks;
	sbp->sb_rextents = rtextents;
	uuid_generate(uuid);
	uuid_copy(sbp->sb_uuid, uuid);
	sbp->sb_logstart = logstart;
	sbp->sb_rootino = sbp->sb_rbmino = sbp->sb_rsumino = NULLFSINO;
	sbp->sb_rextsize = rtextblocks;
	sbp->sb_agblocks = (xfs_agblock_t)agsize;
	sbp->sb_agcount = (xfs_agnumber_t)agcount;
	sbp->sb_rbmblocks = nbmblocks;
	sbp->sb_logblocks = (xfs_extlen_t)logblocks;
	sbp->sb_sectsize = 1 << sectlog;
	sbp->sb_inodesize = (__uint16_t)isize;
	sbp->sb_inopblock = (__uint16_t)(blocksize / isize);
	sbp->sb_sectlog = (__uint8_t)sectlog;
	sbp->sb_inodelog = (__uint8_t)inodelog;
	sbp->sb_inopblog = (__uint8_t)(blocklog - inodelog);
	sbp->sb_rextslog =
		(__uint8_t)(rtextents ?
			libxfs_highbit32((unsigned int)rtextents) : 0);
	sbp->sb_inprogress = 1;	/* mkfs is in progress */
	sbp->sb_imax_pct = imflag ? imaxpct : XFS_DFL_IMAXIMUM_PCT;
	sbp->sb_icount = 0;
	sbp->sb_ifree = 0;
	sbp->sb_fdblocks = dblocks - agcount * XFS_PREALLOC_BLOCKS(mp) -
		(loginternal ? logblocks : 0);
	sbp->sb_frextents = 0;	/* will do a free later */
	sbp->sb_uquotino = sbp->sb_gquotino = 0;
	sbp->sb_qflags = 0;
	sbp->sb_unit = dsunit;
	sbp->sb_width = dswidth;
	if (dirversion == 2)
		sbp->sb_dirblklog = dirblocklog - blocklog;
	if (logversion == 2)
		sbp->sb_logsunit = (lsunit == 0) ? 1 : lsunit;
	else
		sbp->sb_logsunit = 0;
	if (iaflag) {
		sbp->sb_inoalignmt = XFS_INODE_BIG_CLUSTER_SIZE >> blocklog;
		iaflag = sbp->sb_inoalignmt != 0;
	} else
		sbp->sb_inoalignmt = 0;
	sbp->sb_versionnum =
		XFS_SB_VERSION_MKFS(iaflag, dsunit != 0, extent_flagging,
			dirversion == 2, logversion == 2);

	/*
	 * Zero out the first 68k in on the device, to obliterate any old 
	 * filesystem signatures out there.  This should take care of 
	 * swap (somewhere around the page size), jfs (32k), 
	 * ext[2,3] and reiser (64k) - and hopefully all else.
	 */
	 
	buf = libxfs_getbuf(xi.ddev, 0, 136);
	bzero(XFS_BUF_PTR(buf), 136*BBSIZE);
	libxfs_writebuf(buf, 1);

	/* OK, now write the superblock */
	buf = libxfs_getbuf(xi.ddev, XFS_SB_DADDR, 1);
	bzero(XFS_BUF_PTR(buf), BBSIZE);
	libxfs_xlate_sb(XFS_BUF_PTR(buf), sbp, -1, ARCH_CONVERT,
			XFS_SB_ALL_BITS);
	libxfs_writebuf(buf, 1);

	if (!qflag)
		printf(
		   "meta-data=%-22s isize=%-6d agcount=%lld, agsize=%lld blks\n"
		   "data     =%-22s bsize=%-6d blocks=%lld, imaxpct=%d\n"
		   "         =%-22s sunit=%-6d swidth=%d blks, unwritten=%d\n"
		   "naming   =version %-14d bsize=%-6d\n"
		   "log      =%-22s bsize=%-6d blocks=%lld, version=%d\n"
		   "         =%-22s sunit=%d\n"
		   "realtime =%-22s extsz=%-6d blocks=%lld, rtextents=%lld\n",
			dfile, isize, (long long)agcount, (long long)agsize,
			"", blocksize, (long long)dblocks, sbp->sb_imax_pct,
			"", dsunit, dswidth, extent_flagging,
			dirversion, dirversion == 1 ? blocksize : dirblocksize,
			logfile, 1 << blocklog, (long long)logblocks,
			logversion, "", lsunit,
			rtfile, rtextblocks << blocklog,
			(long long)rtblocks, (long long)rtextents);
	/*
	 * If the data area is a file, then grow it out to its final size
	 * so that the reads for the end of the device in the mount code
	 * will succeed.
	 */
	if (xi.disfile && ftruncate64(xi.dfd, dblocks * blocksize) < 0) {
		fprintf(stderr, "%s: Growing the data section file failed\n",
			progname);
		exit(1);
	}

	/*
	 * Zero out the last 64k on the device, to obliterate any
	 * old MD RAID (or other) metadata at the end of the device.
	 */
	if (!xi.disfile) {
		buf = libxfs_getbuf(xi.ddev, (xi.dsize - BTOBB(65536)), 
				    BTOBB(65536));
		bzero(XFS_BUF_PTR(buf), 65536);
		libxfs_writebuf(buf, 1);
	}

	/*
	 * Zero the log if there is one.
	 */
	if (loginternal)
		xi.logdev = xi.ddev;
	if (xi.logdev)
		libxfs_log_clear(
                    xi.logdev, 
                    XFS_FSB_TO_DADDR(mp, logstart),
		    (xfs_extlen_t)XFS_FSB_TO_BB(mp, logblocks),
                    &sbp->sb_uuid,
                    logversion, lsunit, XLOG_FMT);

	mp = libxfs_mount(mp, sbp, xi.ddev, xi.logdev, xi.rtdev, 1);
	if (mp == NULL) {
		fprintf(stderr, "%s: filesystem failed to initialize\n", progname);
		exit(1);
	}
	if (xi.logdev &&
	    XFS_FSB_TO_B(mp, logblocks) <
	    XFS_MIN_LOG_FACTOR * max_trans_res(mp)) {
		fprintf(stderr, "%s: log size (%lld) is too small for "
				"transaction reservations\n",
			progname, (long long)logblocks);
		exit(1);
	}

	for (agno = 0; agno < agcount; agno++) {
		/*
		 * Superblock.
		 */
		buf = libxfs_getbuf(xi.ddev,
				XFS_AG_DADDR(mp, agno, XFS_SB_DADDR), 1);
		bzero(XFS_BUF_PTR(buf), BBSIZE);
                libxfs_xlate_sb(XFS_BUF_PTR(buf), sbp, -1, ARCH_CONVERT,
				XFS_SB_ALL_BITS);
		libxfs_writebuf(buf, 1);

		/*
		 * AG header block: freespace
		 */
		buf = libxfs_getbuf(mp->m_dev,
				XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR), 1);
		agf = XFS_BUF_TO_AGF(buf);
		bzero(agf, BBSIZE);
		if (agno == agcount - 1)
			agsize = dblocks - (xfs_drfsbno_t)(agno * agsize);
		INT_SET(agf->agf_magicnum, ARCH_CONVERT, XFS_AGF_MAGIC);
		INT_SET(agf->agf_versionnum, ARCH_CONVERT, XFS_AGF_VERSION);
		INT_SET(agf->agf_seqno, ARCH_CONVERT, agno);
		INT_SET(agf->agf_length, ARCH_CONVERT, (xfs_agblock_t)agsize);
		INT_SET(agf->agf_roots[XFS_BTNUM_BNOi], ARCH_CONVERT,
				XFS_BNO_BLOCK(mp));
		INT_SET(agf->agf_roots[XFS_BTNUM_CNTi], ARCH_CONVERT,
				XFS_CNT_BLOCK(mp));
		INT_SET(agf->agf_levels[XFS_BTNUM_BNOi], ARCH_CONVERT, 1);
		INT_SET(agf->agf_levels[XFS_BTNUM_CNTi], ARCH_CONVERT, 1);
		INT_SET(agf->agf_flfirst, ARCH_CONVERT, 0);
		INT_SET(agf->agf_fllast, ARCH_CONVERT, XFS_AGFL_SIZE - 1);
		INT_SET(agf->agf_flcount, ARCH_CONVERT, 0);
		nbmblocks = (xfs_extlen_t)(agsize - XFS_PREALLOC_BLOCKS(mp));
		INT_SET(agf->agf_freeblks, ARCH_CONVERT, nbmblocks);
		INT_SET(agf->agf_longest, ARCH_CONVERT, nbmblocks);
		if (loginternal && agno == logagno) {
			INT_MOD(agf->agf_freeblks, ARCH_CONVERT, -logblocks);
			INT_SET(agf->agf_longest, ARCH_CONVERT, agsize - 
				XFS_FSB_TO_AGBNO(mp, logstart) - logblocks);
		}
		if (XFS_MIN_FREELIST(agf, mp) > worst_freelist)
			worst_freelist = XFS_MIN_FREELIST(agf, mp);
		libxfs_writebuf(buf, 1);

		/*
		 * AG header block: inodes
		 */
		buf = libxfs_getbuf(mp->m_dev,
				XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR), 1);
		agi = XFS_BUF_TO_AGI(buf);
		bzero(agi, BBSIZE);
		INT_SET(agi->agi_magicnum, ARCH_CONVERT, XFS_AGI_MAGIC);
		INT_SET(agi->agi_versionnum, ARCH_CONVERT, XFS_AGI_VERSION);
		INT_SET(agi->agi_seqno, ARCH_CONVERT, agno);
		INT_SET(agi->agi_length, ARCH_CONVERT, (xfs_agblock_t)agsize);
		INT_SET(agi->agi_count, ARCH_CONVERT, 0);
		INT_SET(agi->agi_root, ARCH_CONVERT, XFS_IBT_BLOCK(mp));
		INT_SET(agi->agi_level, ARCH_CONVERT, 1);
		INT_SET(agi->agi_freecount, ARCH_CONVERT, 0);
		INT_SET(agi->agi_newino, ARCH_CONVERT, NULLAGINO);
		INT_SET(agi->agi_dirino, ARCH_CONVERT, NULLAGINO);
		for (i = 0; i < XFS_AGI_UNLINKED_BUCKETS; i++)
			INT_SET(agi->agi_unlinked[i], ARCH_CONVERT, NULLAGINO);
		libxfs_writebuf(buf, 1);

		/*
		 * BNO btree root block
		 */
		buf = libxfs_getbuf(mp->m_dev,
				XFS_AGB_TO_DADDR(mp, agno, XFS_BNO_BLOCK(mp)),
				bsize);
		block = XFS_BUF_TO_SBLOCK(buf);
		bzero(block, blocksize);
		INT_SET(block->bb_magic, ARCH_CONVERT, XFS_ABTB_MAGIC);
		INT_SET(block->bb_level, ARCH_CONVERT, 0);
		INT_SET(block->bb_numrecs, ARCH_CONVERT, 1);
		INT_SET(block->bb_leftsib, ARCH_CONVERT, NULLAGBLOCK);
		INT_SET(block->bb_rightsib, ARCH_CONVERT, NULLAGBLOCK);
		arec = XFS_BTREE_REC_ADDR(blocksize, xfs_alloc, block, 1,
			XFS_BTREE_BLOCK_MAXRECS(blocksize, xfs_alloc, 1));
		INT_SET(arec->ar_startblock, ARCH_CONVERT,
			XFS_PREALLOC_BLOCKS(mp));
		if (loginternal && agno == logagno) {
			if (lalign) {
				/*
				 * Have to insert two records
				 */
				INT_SET(arec->ar_blockcount, ARCH_CONVERT, 
					(xfs_extlen_t)(XFS_FSB_TO_AGBNO(
						mp, logstart)
				  	- (INT_GET(arec->ar_startblock,
						ARCH_CONVERT))));
				nrec = arec + 1;
				INT_SET(nrec->ar_startblock, ARCH_CONVERT,
					INT_GET(arec->ar_startblock,
						ARCH_CONVERT) +
					INT_GET(arec->ar_blockcount,
						ARCH_CONVERT));
				arec = nrec;
				INT_MOD(block->bb_numrecs, ARCH_CONVERT, 1);
			} 
			INT_MOD(arec->ar_startblock, ARCH_CONVERT, logblocks);
		} 
		INT_SET(arec->ar_blockcount, ARCH_CONVERT,
			(xfs_extlen_t)(agsize -
				INT_GET(arec->ar_startblock, ARCH_CONVERT)));
		libxfs_writebuf(buf, 1);

		/*
		 * CNT btree root block
		 */
		buf = libxfs_getbuf(mp->m_dev,
				XFS_AGB_TO_DADDR(mp, agno, XFS_CNT_BLOCK(mp)),
				bsize);
		block = XFS_BUF_TO_SBLOCK(buf);
		bzero(block, blocksize);
		INT_SET(block->bb_magic, ARCH_CONVERT, XFS_ABTC_MAGIC);
		INT_SET(block->bb_level, ARCH_CONVERT, 0);
		INT_SET(block->bb_numrecs, ARCH_CONVERT, 1);
		INT_SET(block->bb_leftsib, ARCH_CONVERT, NULLAGBLOCK);
		INT_SET(block->bb_rightsib, ARCH_CONVERT, NULLAGBLOCK);
		arec = XFS_BTREE_REC_ADDR(blocksize, xfs_alloc, block, 1,
			XFS_BTREE_BLOCK_MAXRECS(blocksize, xfs_alloc, 1));
		INT_SET(arec->ar_startblock, ARCH_CONVERT,
			XFS_PREALLOC_BLOCKS(mp));
		if (loginternal && agno == logagno) {
			if (lalign) {
				INT_SET(arec->ar_blockcount, ARCH_CONVERT,
				    (xfs_extlen_t)( XFS_FSB_TO_AGBNO(
					mp, logstart) - (INT_GET(
					arec->ar_startblock, ARCH_CONVERT)) )
				);
				nrec = arec + 1;
				INT_SET(nrec->ar_startblock, ARCH_CONVERT,
				    INT_GET(arec->ar_startblock, ARCH_CONVERT) +
				    INT_GET(arec->ar_blockcount, ARCH_CONVERT));
				arec = nrec;
				INT_MOD(block->bb_numrecs, ARCH_CONVERT, 1);
			}
			INT_MOD(arec->ar_startblock, ARCH_CONVERT, logblocks);
		}	
		INT_SET(arec->ar_blockcount, ARCH_CONVERT, (xfs_extlen_t)
			(agsize - INT_GET(arec->ar_startblock, ARCH_CONVERT)));
		libxfs_writebuf(buf, 1);
		/*
		 * INO btree root block
		 */
		buf = libxfs_getbuf(mp->m_dev,
				XFS_AGB_TO_DADDR(mp, agno, XFS_IBT_BLOCK(mp)),
				bsize);
		block = XFS_BUF_TO_SBLOCK(buf);
		bzero(block, blocksize);
		INT_SET(block->bb_magic, ARCH_CONVERT, XFS_IBT_MAGIC);
		INT_SET(block->bb_level, ARCH_CONVERT, 0);
		INT_SET(block->bb_numrecs, ARCH_CONVERT, 0);
		INT_SET(block->bb_leftsib, ARCH_CONVERT, NULLAGBLOCK);
		INT_SET(block->bb_rightsib, ARCH_CONVERT, NULLAGBLOCK);
		libxfs_writebuf(buf, 1);
	}

	/*
	 * Touch last block, make fs the right size if it's a file.
	 */
	buf = libxfs_getbuf(mp->m_dev,
		(xfs_daddr_t)XFS_FSB_TO_BB(mp, dblocks - 1LL), bsize);
	bzero(XFS_BUF_PTR(buf), blocksize);
	libxfs_writebuf(buf, 1);

	/*
	 * Make sure we can write the last block in the realtime area.
	 */
	if (mp->m_rtdev && rtblocks > 0) {
		buf = libxfs_getbuf(mp->m_rtdev,
				XFS_FSB_TO_BB(mp, rtblocks - 1LL), bsize);
		bzero(XFS_BUF_PTR(buf), blocksize);
		libxfs_writebuf(buf, 1);
	}
	/*
	 * BNO, CNT free block list
	 */
	for (agno = 0; agno < agcount; agno++) {
		xfs_alloc_arg_t	args;
		xfs_trans_t	*tp;

		bzero(&args, sizeof(args));
		args.tp = tp = libxfs_trans_alloc(mp, 0);
		args.mp = mp;
		args.agno = agno;
		args.alignment = 1;
		args.minalignslop = UINT_MAX;
		args.pag = &mp->m_perag[agno];
		if ((i = libxfs_trans_reserve(tp, worst_freelist, 0, 0, 0, 0)))
			res_failed(i);
		libxfs_alloc_fix_freelist(&args, 0);
		libxfs_trans_commit(tp, 0, NULL);
	}
	/*
	 * Allocate the root inode and anything else in the proto file.
	 */
	mp->m_rootip = NULL;
	parseproto(mp, NULL, &protostring, NULL);

	/*
	 * protect ourselves against possible stupidity
	 */
	if (XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino) != 0) {
		fprintf(stderr, "%s: root inode not created in AG 0, "
				"created in AG %u",
			progname, XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino));
		exit(1);
	}

	/*
	 * write out multiple copies of superblocks with the rootinode field set
	 */
	if (mp->m_sb.sb_agcount > 1) {
		/*
		 * the last superblock
		 */
		buf = libxfs_readbuf(mp->m_dev,
				XFS_AGB_TO_DADDR(mp, mp->m_sb.sb_agcount-1,
					XFS_SB_DADDR),
				BTOBB(mp->m_sb.sb_sectsize), 1);
		INT_SET((XFS_BUF_TO_SBP(buf))->sb_rootino,
				ARCH_CONVERT, mp->m_sb.sb_rootino);
		libxfs_writebuf(buf, 1);
		/*
		 * and one in the middle for luck
		 */
		if (mp->m_sb.sb_agcount > 2) {
			buf = libxfs_readbuf(mp->m_dev,
				XFS_AGB_TO_DADDR(mp, (mp->m_sb.sb_agcount-1)/2,
					XFS_SB_DADDR),
				BTOBB(mp->m_sb.sb_sectsize), 1);
			INT_SET((XFS_BUF_TO_SBP(buf))->sb_rootino,
				ARCH_CONVERT, mp->m_sb.sb_rootino);
			libxfs_writebuf(buf, 1);
		}
	}

	/*
	 * Mark the filesystem ok.
	 */
	buf = libxfs_getsb(mp, 1);
	(XFS_BUF_TO_SBP(buf))->sb_inprogress = 0;
	libxfs_writebuf(buf, 1);

	libxfs_umount(mp);
	if (xi.rtdev)
		libxfs_device_close(xi.rtdev);
	if (xi.logdev && xi.logdev != xi.ddev)
		libxfs_device_close(xi.logdev);
	libxfs_device_close(xi.ddev);

	return 0;
}

static void
conflict(
	char	opt,
	char	*tab[],
	int	oldidx,
	int	newidx)
{
	fprintf(stderr, "Cannot specify both -%c %s and -%c %s\n",
		opt, tab[oldidx], opt, tab[newidx]);
	usage();
}


static void
illegal(
	char	*value,
	char	*opt)
{
	fprintf(stderr, "Illegal value %s for -%s option\n", value, opt);
	usage();
}

static int
ispow2(
	unsigned int	i)
{
	return (i & (i - 1)) == 0;
}

static void
reqval(
	char	opt,
	char	*tab[],
	int	idx)
{
	fprintf(stderr, "-%c %s option requires a value\n", opt, tab[idx]);
	usage();
}

static void
respec(
	char	opt,
	char	*tab[],
	int	idx)
{
	fprintf(stderr, "-%c ", opt);
	if (tab)
		fprintf(stderr, "%s ", tab[idx]);
	fprintf(stderr, "option respecified\n");
	usage();
}

static void
unknown(
	char	opt,
	char	*s)
{
	fprintf(stderr, "unknown option -%c %s\n", opt, s);
	usage();
}

static int
max_trans_res(
	xfs_mount_t			*mp)
{
	uint				*p;
	int				rval;
	xfs_trans_reservations_t	*tr;

	tr = &mp->m_reservations;

	for (rval = 0, p = (uint *)tr; p < (uint *)(tr + 1); p++) {
		if ((int)*p > rval)
			rval = (int)*p;
	}
	return rval;
}

long long
cvtnum(
	int		blocksize,
	char		*s)
{
	long long	i;
	char		*sp;
	extern void	usage(void);

	i = strtoll(s, &sp, 0);
	if (i == 0 && sp == s)
		return -1LL;
	if (*sp == '\0')
		return i;

	if (*sp == 'b' && sp[1] == '\0') {
		if (blocksize)
			return i * blocksize;

		fprintf(stderr, "blocksize not available yet.\n");
		usage();
	}

	if (*sp == 'k' && sp[1] == '\0')
		return 1024LL * i;
	if (*sp == 'm' && sp[1] == '\0')
		return 1024LL * 1024LL * i;
	if (*sp == 'g' && sp[1] == '\0')
		return 1024LL * 1024LL * 1024LL * i;
	return -1LL;
}

void
usage(void)
{
	fprintf(stderr, "Usage: %s\n\
/* blocksize */		[-b log=n|size=num]\n\
/* data subvol */	[-d agcount=n,agsize=n,file,name=xxx,size=num,\n\
			    sunit=value,swidth=value,unwritten=0|1,\n\
			    su=value,sw=value]\n\
/* inode size */	[-i log=n|perblock=n|size=num,maxpct=n]\n\
/* log subvol */	[-l agnum=n,internal,size=num,logdev=xxx]\n\
			    version=n,sunit=value]\n\
/* naming */		[-n log=n|size=num|version=n]\n\
/* label */		[-L label (maximum 12 characters)]\n\
/* prototype file */	[-p fname]\n\
/* quiet */		[-q]\n\
/* version */		[-V]\n\
/* realtime subvol */	[-r extsize=num,size=num,rtdev=xxx]\n\
			devicename\n\
<devicename> is required unless -d name=xxx is given.\n\
Internal log by default, size is scaled from 1,000 blocks to 32,768 blocks\n\
based on the filesystem size.  Default log reaches its largest size at 1TB.\n\
This can be overridden with the -l options or using a volume manager with a\n\
log subvolume.\n\
<num> is xxx (bytes), or xxxb (blocks), or xxxk (xxx KB), or xxxm (xxx MB)\n\
<value> is xxx (512 blocks).\n",
		progname);
	exit(1);
}
