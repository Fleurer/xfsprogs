/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef __JDM_H__
#define __JDM_H__

typedef int	intgen_t;
typedef void	jdm_fshandle_t;

struct xfs_bstat;
extern jdm_fshandle_t *jdm_getfshandle	(char *mntpnt);
extern intgen_t jdm_open	(jdm_fshandle_t *fsh, struct xfs_bstat *sp,
				 intgen_t oflags);
extern intgen_t jdm_readlink	(jdm_fshandle_t *fsh, struct xfs_bstat *sp,
				 char *bufp, size_t bufsz);

#ifdef EXTATTR

struct attrlist_cursor;
extern intgen_t jdm_attr_multi	(jdm_fshandle_t *fsh, struct xfs_bstat *sp,
				 char *bufp, int rtrvcnt, int flags);
extern intgen_t	jdm_attr_list	(jdm_fshandle_t *fsh, struct xfs_bstat *sp,
				 char *bufp, size_t bufsz, int flags, 
				 struct attrlist_cursor *cursor);
#endif	/* EXTATTR */

/* macro for determining the size of a structure member */
#define sizeofmember( t, m )	sizeof( ( ( t * )0 )->m )

/* macro for calculating the offset of a structure member */
#define offsetofmember( t, m )	( ( size_t )( char * )&( ( ( t * )0 )->m ) )

#endif	/* __JDM_H__ */