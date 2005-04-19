/*
 * Copyright (c) 2003-2005 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <xfs/libxfs.h>
#include <xfs/command.h>
#include <xfs/input.h>

cmdinfo_t	*cmdtab;
int		ncmds;

static argsfunc_t	args_func;
static checkfunc_t	check_func;
static int		ncmdline;
static char		**cmdline;

static int
compare(const void *a, const void *b)
{
	return strcmp(((const cmdinfo_t *)a)->name,
		      ((const cmdinfo_t *)b)->name);
}

void
add_command(
	const cmdinfo_t	*ci)
{
	cmdtab = realloc((void *)cmdtab, ++ncmds * sizeof(*cmdtab));
	cmdtab[ncmds - 1] = *ci;
	qsort(cmdtab, ncmds, sizeof(*cmdtab), compare);
}

static int
check_command(
	const cmdinfo_t	*ci)
{
	if (check_func)
		return check_func(ci);
	return 1;
}

void
add_check_command(
	checkfunc_t	cf)
{
	check_func = cf;
}

int
command_usage(
	const cmdinfo_t *ci)
{
	printf("%s %s -- %s\n", ci->name, ci->args, ci->oneline);
	return 0;
}

int
command(
	int		argc,
	char		**argv)
{
	char		*cmd;
	const cmdinfo_t	*ct;

	cmd = argv[0];
	ct = find_command(cmd);
	if (!ct) {
		fprintf(stderr, _("command \"%s\" not found\n"), cmd);
		return 0;
	}
	if (!check_command(ct))
		return 0;
	if (argc-1 < ct->argmin || (ct->argmax != -1 && argc-1 > ct->argmax)) {
		if (ct->argmax == -1)
			fprintf(stderr,
	_("bad argument count %d to %s, expected at least %d arguments\n"),
				argc-1, cmd, ct->argmin);
		else if (ct->argmin == ct->argmax)
			fprintf(stderr,
	_("bad argument count %d to %s, expected %d arguments\n"),
				argc-1, cmd, ct->argmin);
		else
			fprintf(stderr,
	_("bad argument count %d to %s, expected between %d and %d arguments\n"),
			argc-1, cmd, ct->argmin, ct->argmax);
		return 0;
	}
	platform_getoptreset();
	return ct->cfunc(argc, argv);
}

const cmdinfo_t *
find_command(
	const char	*cmd)
{
	cmdinfo_t	*ct;

	for (ct = cmdtab; ct < &cmdtab[ncmds]; ct++) {
		if (strcmp(ct->name, cmd) == 0 ||
		    (ct->altname && strcmp(ct->altname, cmd) == 0))
			return (const cmdinfo_t *)ct;
	}
	return NULL;
}

void
add_user_command(char *optarg)
{
	ncmdline++;
	cmdline = realloc(cmdline, sizeof(char*) * (ncmdline));
	if (!cmdline) {
		perror("realloc");
		exit(1);
	}
	cmdline[ncmdline-1] = optarg;
}

static int
args_command(
	int	index)
{
	if (args_func)
		return args_func(index);
	return 0;
}

void
add_args_command(
	argsfunc_t	af)
{
	args_func = af;
}

void
command_loop(void)
{
	int	c, i, j = 0, done = 0;
	char	*input;
	char	**v;

	for (i = 0; !done && i < ncmdline; i++) {
		while (!done && (j = args_command(j))) {
			input = strdup(cmdline[i]);
			if (!input) {
				fprintf(stderr,
					_("cannot strdup command '%s': %s\n"),
					cmdline[i], strerror(errno));
				exit(1);
			}
			v = breakline(input, &c);
			if (c)
				done = command(c, v);
			free(v);
			free(input);
		}
	}
	if (cmdline) {
		free(cmdline);
		return;
	}
	while (!done) {
		if ((input = fetchline()) == NULL)
			break;
		v = breakline(input, &c);
		if (c)
			done = command(c, v);
		doneline(input, v);
	}
}
