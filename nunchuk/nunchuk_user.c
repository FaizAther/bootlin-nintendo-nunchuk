// SPDX-License-Identifier: GPL-2.0
/*
 * Nunchuk live status — one terminal line, overwritten in place (\r).
 *
 *   ./nunchuk_user.exe [/dev/input/eventN]
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void on_signal(int sig)
{
	(void)sig;
	running = 0;
}

static int is_nunchuk_fd(int fd)
{
	char name[256] = {};

	if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0)
		return 0;

	return strstr(name, "Nunchuk") != NULL;
}

static int open_nunchuk_device(const char *path)
{
	char devpath[64];
	int fd, i;

	if (path)
		return open(path, O_RDONLY | O_NONBLOCK);

	for (i = 0; i < 32; i++) {
		snprintf(devpath, sizeof(devpath), "/dev/input/event%d", i);
		fd = open(devpath, O_RDONLY | O_NONBLOCK);
		if (fd < 0)
			continue;
		if (is_nunchuk_fd(fd))
			return fd;
		close(fd);
	}

	errno = ENOENT;
	return -1;
}

/* Pad with spaces so shorter text overwrites leftovers from longer lines */
static void draw_line(int x, int y, int z, int c)
{
	char z_ch = z ? '*' : '.';
	char c_ch = c ? '*' : '.';

	printf("\rjoystick  X=%3d  Y=%3d  |  Z:%c  C:%c    ",
	       x, y, z_ch, c_ch);
	fflush(stdout);
}

int main(int argc, char *argv[])
{
	const char *devpath = argc == 2 ? argv[1] : NULL;
	struct input_event ev;
	int fd, x = 128, y = 128, z = 0, c = 0;

	if (argc > 2) {
		fprintf(stderr, "Usage: %s [/dev/input/eventN]\n", argv[0]);
		return EXIT_FAILURE;
	}

	fd = open_nunchuk_device(devpath);
	if (fd < 0) {
		fprintf(stderr, "Nunchuk not found: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	draw_line(x, y, z, c);

	while (running) {
		struct pollfd pfd = { .fd = fd, .events = POLLIN };
		int ret = poll(&pfd, 1, 100);

		if (ret < 0) {
			if (errno == EINTR)
				break;
			perror("poll");
			break;
		}
		if (ret == 0)
			continue;

		while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
			if (ev.type == EV_ABS) {
				if (ev.code == ABS_X)
					x = ev.value;
				else if (ev.code == ABS_Y)
					y = ev.value;
				else
					continue;
			} else if (ev.type == EV_KEY) {
				if (ev.code == BTN_Z || ev.code == BTN_1)
					z = ev.value;
				else if (ev.code == BTN_C || ev.code == BTN_0)
					c = ev.value;
				else
					continue;
			} else {
				continue;
			}

			draw_line(x, y, z, c);
		}
	}

	printf("\n");
	close(fd);
	return EXIT_SUCCESS;
}
