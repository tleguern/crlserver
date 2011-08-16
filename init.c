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

#ifdef __Linux__
# define _BSD_SOURCE
# include <bsd/bsd.h>
#endif

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <curses.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "crlserver.h"
#include "db.h"
#include "init.h"
#include "log.h"
#include "pathnames.h"
#include "session.h"

struct session session;

static void
heed_signals(void) {
	signal(SIGINT, byebye);
	signal(SIGQUIT, byebye);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
}

static void
ignore_signals(void) {
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
}

void
byebye(int unused) {
	unused = 0;
	ignore_signals();
	/* TODO: Ask user if he really wants to quit */
	clean_upx(1, "Bye bye !\n");
	heed_signals();
}

int
init_playground_files(const char *path) {
	struct dirent* dp;
	const char *misc = CRLSERVER_MISC_DIR;
	DIR* dir = opendir(misc);

	if (dir == NULL)
		return -1;

	while ((dp = readdir(dir)) != NULL) {
		char in_path[ MAXPATHLEN ];
		char out_path[ MAXPATHLEN ];
		char buf[ 1024 ];
		FILE *ifd, *ofd;

		if (dp->d_name[0] == '.')
			continue;

		(void)snprintf(in_path, strlen(misc) + strlen(dp->d_name) + 2,
				"%s/%s\n", misc, dp->d_name);
		(void)snprintf(out_path, strlen(path) + strlen(dp->d_name) + 3,
				"%s/.%s\n", path, dp->d_name);

		ifd = fopen(in_path, "r");
		ofd = fopen(out_path, "a+");
		if (ifd == NULL || ofd == NULL) {
			(void)fclose(ifd);
			(void)fclose(ofd);
			continue;
		}
		
		while (feof(ifd) == 0) {
			(void)memset(buf, '\0', 1024);
			(void)fread(buf, 1, 1024, ifd);
			(void)fwrite(buf, 1, 1024, ofd);
		}
		if (fclose(ifd) != 0)
			logmsg(strerror(errno));
		if (fclose(ofd) != 0)
			logmsg(strerror(errno));
	}
	if (closedir(dir) == -1)
		logmsg(strerror(errno));
	return 0;
}

int
init_session(const char *name) {
	char path[MAXPATHLEN] = {0};

	session.name = strdup(name);
	if (session.name == NULL)
		fclean_up("Memory error");

	(void)snprintf(path, sizeof path, "%s/%c/%s",
		 CRLSERVER_PLAYGROUND, session.name[0], session.name);
	session.home = strdup(path);
	if (session.home == NULL)
		fclean_up("Memory error");
	if (access(session.home, F_OK) != 0) {
		free(session.home);
		free(session.name);
		return -1;
	}
	return 0;
}

int
init_playground_dir(const char *player_name) {
	char playground[MAXPATHLEN] = CRLSERVER_PLAYGROUND;

	if (player_name == NULL)
		return -1;

	(void)strlcat(playground, "/", sizeof playground);
	(void)strncat(playground, player_name, 1);
	(void)mkdir(playground, 0700);
	(void)strlcat(playground, "/", sizeof playground);
	(void)strlcat(playground, player_name, sizeof playground);
	(void)mkdir(playground, 0700);

	if (access(playground, F_OK) == -1)
		return -1;

	/*
	session.home = strdup(playground);
	if (session.home == NULL)
		fclean_up("Memory error");
	*/
	return init_playground_files(playground);
}

void
init(void) {
	heed_signals();

	db_init();

	(void)initscr();
	if (has_colors() == TRUE)
		(void)start_color();
	(void)curs_set(0);
	start_window();

	/* A lot of games ask this size, so check it now. */
	if ((LINES < DROWS) || (COLS < DCOLS))
		fclean_up("must be displayed on 24 x 80 screen (or larger)");

	session.logged = 0;
	session.name = NULL;
	session.home = NULL;
}

void
start_window(void) {
	(void)cbreak();
	(void)noecho();
	(void)nonl();
	(void)keypad(stdscr, TRUE);
}

__inline void
end_window() {
	(void)endwin();
}
