// SPDX-License-Identifier: GPL-2.0-only
/* M3-B2: fixed inherited mode, two distinct dumb buffers, one event flip.
 * No libdrm linkage. Requires DRM master; refuses an already-active CRTC.
 * Run under an external timeout: the poll is bounded, kernel ioctls may not be.
 * Closing disables this probe's CRTC; physical scanout is driver-defined.
 * Event completion alone is NOT a hardware presentation PASS.
 */
#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>

struct buffer { uint32_t handle, fb; uint64_t size; void *map; };
static int fd = -1;
static uint32_t crtc_id;
static int modeset;
static struct buffer buffers[2];

static void cleanup(void)
{
	if (fd < 0) return;
	if (modeset) {
		struct drm_mode_crtc off = { .crtc_id = crtc_id };
		printf("cleanup: disable probe CRTC\n");
		if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &off))
			perror("cleanup SETCRTC");
	}
	for (unsigned i = 0; i < 2; i++) {
		struct buffer *b = &buffers[i];
		if (b->fb && ioctl(fd, DRM_IOCTL_MODE_RMFB, &b->fb)) perror("RMFB");
		if (b->map && b->map != MAP_FAILED) munmap(b->map, b->size);
		if (b->handle) {
			struct drm_mode_destroy_dumb d = { .handle = b->handle };
			if (ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d)) perror("DESTROY_DUMB");
		}
	}
	close(fd);
}

static void check(int ok, const char *what)
{
	if (!ok) { fprintf(stderr, "FAIL %s (errno=%d: %s)\n", what, errno,
			   strerror(errno)); exit(1); }
}

static uint64_t now_ns(void)
{
	struct timespec ts;
	check(!clock_gettime(CLOCK_MONOTONIC, &ts), "clock_gettime");
	return (uint64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

static void snapshot(const char *path, const char *label)
{
	char line[256];
	FILE *f = fopen(path, "r");
	printf("=== %s (%s) ===\n", label, path);
	check(f != NULL, "open iftu_state");
	while (fgets(line, sizeof(line), f)) fputs(line, stdout);
	check(!ferror(f), "read iftu_state");
	fclose(f);
}

static void make_buffer(struct buffer *b, const struct drm_mode_modeinfo *mode,
			uint32_t colour)
{
	struct drm_mode_create_dumb d = {
		.width = mode->hdisplay, .height = mode->vdisplay, .bpp = 32
	};
	struct drm_mode_map_dumb m = {0};
	struct drm_mode_fb_cmd fb = {0};
	puts("CREATE_DUMB");
	check(!ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &d), "CREATE_DUMB");
	b->handle = d.handle; b->size = d.size;
	check(d.pitch >= d.width * 4 && d.size >= (uint64_t)d.pitch * d.height &&
	      d.size <= SIZE_MAX, "buffer bounds");
	m.handle = d.handle;
	puts("MAP_DUMB + mmap");
	check(!ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &m), "MAP_DUMB");
	b->map = mmap(NULL, d.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, m.offset);
	check(b->map != MAP_FAILED, "mmap");
	for (uint32_t y = 0; y < d.height; y++) {
		uint32_t *row = (uint32_t *)((char *)b->map + y * d.pitch);
		for (uint32_t x = 0; x < d.width; x++) row[x] = colour;
	}
	fb.width = d.width; fb.height = d.height; fb.pitch = d.pitch;
	fb.bpp = 32; fb.depth = 24; fb.handle = d.handle;
	puts("ADDFB");
	check(!ioctl(fd, DRM_IOCTL_MODE_ADDFB, &fb), "ADDFB");
	b->fb = fb.fb_id;
	printf("buffer: fb=%u pitch=%u bytes=%" PRIu64 " colour=%08x\n",
	       b->fb, d.pitch, b->size, colour);
}

int main(int argc, char **argv)
{
	const char *card = argc > 1 ? argv[1] : "/dev/dri/card0";
	const char *state = argc > 2 ? argv[2] : "/sys/kernel/debug/dri/0/iftu_state";
	uint32_t crtcs[8], connectors[8], encoders[8];
	struct drm_mode_card_res r = { .count_crtcs = 8, .count_connectors = 8,
		.count_encoders = 8, .crtc_id_ptr = (uintptr_t)crtcs,
		.connector_id_ptr = (uintptr_t)connectors, .encoder_id_ptr = (uintptr_t)encoders };
	struct drm_mode_get_connector conn = {0};
	struct drm_mode_modeinfo mode;
	struct drm_mode_crtc c = {0};
	uint32_t encoder_ids[8];
	struct drm_mode_modeinfo modes[8];
	const uint64_t cookie = 0x4d334232;
	struct drm_mode_crtc_page_flip flip = { .flags = DRM_MODE_PAGE_FLIP_EVENT,
		.user_data = cookie };
	struct drm_event_vblank ev;
	struct pollfd pfd;
	uint64_t start, elapsed, budget;
	int ret;

	setvbuf(stdout, NULL, _IONBF, 0);
	if (argc > 3) { fprintf(stderr, "usage: %s [card [iftu_state]]\n", argv[0]); return 2; }
	printf("M3-B2 modeset-first probe: %s\n", card);
	fd = open(card, O_RDWR | O_CLOEXEC | O_NONBLOCK);
	check(fd >= 0, "open card");
	atexit(cleanup);
	puts("SET_MASTER");
	check(!ioctl(fd, DRM_IOCTL_SET_MASTER, 0), "SET_MASTER");
	puts("GETRESOURCES");
	check(!ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &r), "GETRESOURCES");
	/* Single-pipe gate only, not a general display configuration tool. */
	check(r.count_crtcs == 1 && r.count_connectors == 1 && r.count_encoders <= 8,
	      "expected one CRTC and connector");
	crtc_id = crtcs[0]; c.crtc_id = crtc_id;
	puts("GETCRTC");
	check(!ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &c), "GETCRTC");
	printf("before: crtc=%u mode=%ux%u fb=%u valid=%u\n", crtc_id,
	       c.mode.hdisplay, c.mode.vdisplay, c.fb_id, c.mode_valid);
	check(!c.mode_valid && !c.fb_id, "refusing to replace active userspace state");
	conn.connector_id = connectors[0];
	conn.count_modes = 8; conn.modes_ptr = (uintptr_t)modes;
	conn.count_encoders = 8; conn.encoders_ptr = (uintptr_t)encoder_ids;
	puts("GETCONNECTOR");
	check(!ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn), "GETCONNECTOR");
	check(conn.connection == 1 && conn.count_modes == 1 && conn.count_encoders <= 8,
	      "expected connected fixed-mode connector");
	mode = modes[0];
	check(mode.hdisplay && mode.vdisplay && mode.clock && mode.htotal && mode.vtotal,
	      "invalid connector mode");
	printf("mode: %ux%u clock=%u htotal=%u vtotal=%u\n", mode.hdisplay,
	       mode.vdisplay, mode.clock, mode.htotal, mode.vtotal);
	make_buffer(&buffers[0], &mode, 0x00202020); /* initial grey */
	make_buffer(&buffers[1], &mode, 0x00ff00ff); /* distinct magenta */
	snapshot(state, "before modeset");
	c.fb_id = buffers[0].fb; c.mode = mode; c.mode_valid = 1;
	c.set_connectors_ptr = (uintptr_t)connectors; c.count_connectors = 1;
	puts("SETCRTC (first commit, grey)");
	/* Mark before ioctl: if kernel changes state then errors, attempt disable. */
	modeset = 1;
	check(!ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &c), "SETCRTC");
	puts("SETCRTC returned success");
	memset(&c, 0, sizeof(c)); c.crtc_id = crtc_id;
	check(!ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &c), "GETCRTC after modeset");
	check(c.mode_valid && c.fb_id == buffers[0].fb, "modeset state mismatch");
	snapshot(state, "after modeset / before flip");
	flip.crtc_id = crtc_id; flip.fb_id = buffers[1].fb;
	/* Five frame periods from the connector's reported timing, ceiling ns. */
	budget = ((uint64_t)mode.htotal * mode.vtotal * 5000000 + mode.clock - 1) / mode.clock;
	printf("PAGE_FLIP (magenta), event budget=%" PRIu64 " ns\n", budget);
	start = now_ns();
	check(!ioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip), "PAGE_FLIP");
	pfd = (struct pollfd){ .fd = fd, .events = POLLIN };
	do {
		elapsed = now_ns() - start;
		check(elapsed < budget, "flip exceeded five-frame budget");
		ret = poll(&pfd, 1, (int)((budget - elapsed + 999999) / 1000000));
	} while (ret < 0 && errno == EINTR);
	check(ret > 0 && (pfd.revents & POLLIN), "no flip event within five frames");
	ret = read(fd, &ev, sizeof(ev));
	elapsed = now_ns() - start;
	check(ret == sizeof(ev) && ev.base.length == sizeof(ev) &&
	      ev.base.type == DRM_EVENT_FLIP_COMPLETE && ev.user_data == cookie &&
	      ev.crtc_id == crtc_id, "invalid flip event");
	check(elapsed <= budget, "late flip event");
	printf("DRM_EVENT received: crtc=%u sequence=%u elapsed_ns=%" PRIu64 "\n",
	       ev.crtc_id, ev.sequence, elapsed);
	snapshot(state, "after flip");
	/* Read again before cleanup, so scanout movement is separately observable. */
	usleep(2000);
	snapshot(state, "after flip sample 2");
	usleep(3000);
	snapshot(state, "after flip sample 3");
	puts("DRM-side completion only; compare selector and scanout evidence separately.");
	return 0;
}
