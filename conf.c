/*
 * Copyright (c) 2011 Tristan Le Guern <leguern@medu.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifdef __OpenBSD__
# include <util.h>
# define MAXNAMLEN       255
#elif __Linux__
# define __USE_BSD
# define _BSD_SOURCE
# include "compat/util.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sysexits.h>
#include <dirent.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"
#include "conf.h"

#define FPARSELN(x)	fparseln((x), NULL, NULL, NULL, FPARSELN_UNESCALL)

static int
file_size(const char *path) {
	struct stat s;
	(void)stat(path, &s);
	return (int)s.st_size;
}

static struct list*
parse(FILE *fd) {
	char *b;
	struct list *l = calloc(1, sizeof *l);

	if (l == NULL)
		return NULL;

	while ((b = FPARSELN(fd)) != NULL) {
		char *key = strtok(b, "=");
		char *value = strtok(NULL, "=");

		if (strncmp("name", key, 4) == 0 && value != NULL)
			l->name = strdup(value);
		else if (strncmp("longname", key, 8) == 0 && value != NULL)
			l->lname = strdup(value);
		else if (strncmp("version", key, 7) == 0 && value != NULL)
			l->version = strdup(value);
		else if (strncmp("description", key, 10) == 0 && value != NULL)
			l->desc= strdup(value);
		else if (strncmp("path", key, 4) == 0 && value != NULL)
			l->path = strdup(value);
		else if (strncmp("params", key, 6) == 0 && value != NULL)
			l->params = strdup(value);
		else if (strncmp("env", key, 3) == 0 && value != NULL)
			l->env = strdup(value);
		free(b);
	}
	return l;
}

void
load_folder(const char *path, struct list_head *lh) {
	struct dirent* dp;
	DIR* dir = opendir(path);

	if (dir == NULL)
		err(EX_IOERR, "Can't open %s\n", path);

	while ((dp = readdir(dir)) != NULL) {
		char buf[ (MAXNAMLEN + 1) * 2 ];
		FILE* fd;
		struct list *lp;

		/* Skip non-regular files and empty files. */
		(void)snprintf(buf, strlen(path) + dp->d_namlen + 2, 
				"%s/%s\n", path, dp->d_name);
		if ((dp->d_type != DT_REG && dp->d_type != DT_LNK)
			|| file_size(buf) == 0)
			continue;

		fd = fopen(buf, "r");
		if (fd == NULL) {
			warn("%s", buf);
			continue;
		}

		lp = parse(fd);
		if (lp != NULL)
			SLIST_INSERT_HEAD(lh, lp, ls);
		(void)fclose(fd);
	}
	(void)closedir(dir);
}

void
list_release(struct list_head *lh) {
	struct list *lp;
	while (!SLIST_EMPTY(lh)) {
		lp = SLIST_FIRST(lh);
		SLIST_REMOVE_HEAD(lh, ls);

		free(lp->name);
		free(lp->lname);
		free(lp->version);
		free(lp->desc);
		free(lp->path);
		free(lp->params);
		free(lp->env);
		free(lp);
	}
}

size_t
list_size(struct list_head *lh) {
	size_t s = 0;
	struct list *lp;

	SLIST_FOREACH(lp, lh, ls)
		++s;
	return s;
}