// SPDX-License-Identifier: GPL-2.0-only
/* M3-B2/C: fixed inherited mode, two distinct buffers, bounded event flips.
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
#include <drm/drm_fourcc.h>

struct buffer { uint32_t handle, fb; uint64_t size; void *map; };
static int fd = -1;
static uint32_t crtc_id;
static int modeset, cleanup_errors, native_abgr;
static struct buffer buffers[2];

static void cleanup(void)
{
	if (fd < 0) return;
	if (modeset) {
		struct drm_mode_crtc off = { .crtc_id = crtc_id };
		printf("cleanup: disable probe CRTC\n");
		if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &off)) {
			perror("cleanup SETCRTC"); cleanup_errors++;
		}
	}
	for (unsigned i = 0; i < 2; i++) {
		struct buffer *b = &buffers[i];
		if (b->fb && ioctl(fd, DRM_IOCTL_MODE_RMFB, &b->fb)) { perror("RMFB"); cleanup_errors++; }
		if (b->map && b->map != MAP_FAILED) munmap(b->map, b->size);
		if (b->handle) {
			struct drm_mode_destroy_dumb d = { .handle = b->handle };
			if (ioctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d)) { perror("DESTROY_DUMB"); cleanup_errors++; }
		}
	}
	close(fd); fd = -1;
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
	struct drm_mode_fb_cmd2 fb2 = {0};
	if (native_abgr) colour |= 0xff000000; /* Opaque native alpha, not transparent. */
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
	if (native_abgr) {
		fb2.width = d.width; fb2.height = d.height;
		fb2.pixel_format = DRM_FORMAT_ABGR8888;
		fb2.handles[0] = d.handle; fb2.pitches[0] = d.pitch;
		puts("ADDFB2 native ABGR8888 (opaque alpha)");
		check(!ioctl(fd, DRM_IOCTL_MODE_ADDFB2, &fb2), "ADDFB2");
		b->fb = fb2.fb_id;
	} else {
		puts("ADDFB legacy XRGB8888");
		check(!ioctl(fd, DRM_IOCTL_MODE_ADDFB, &fb), "ADDFB");
		b->fb = fb.fb_id;
	}
	printf("buffer: fb=%u pitch=%u bytes=%" PRIu64 " colour=%08x\n",
	       b->fb, d.pitch, b->size, colour);
}

static unsigned number(const char *s, unsigned max)
{
	char *end;
	unsigned long n;
	errno = 0; n = strtoul(s, &end, 10);
	if (!*s || *s < '0' || *s > '9' || *end || errno || !n || n > max) {
		fprintf(stderr, "invalid bounded argument: %s\n", s); exit(2);
	}
	return (unsigned)n;
}

int main(int argc, char **argv)
{
	const char *card = "/dev/dri/card0", *state = "/sys/kernel/debug/dri/0/iftu_state";
	unsigned frames = 1, seconds = 0, hold = 0, refresh = 60, positional = 0, selection = 0;
	uint32_t crtcs[8], connectors[8], encoders[8];
	struct drm_mode_card_res r = { .count_crtcs = 8, .count_connectors = 8,
		.count_encoders = 8, .crtc_id_ptr = (uintptr_t)crtcs,
		.connector_id_ptr = (uintptr_t)connectors, .encoder_id_ptr = (uintptr_t)encoders };
	struct drm_mode_get_connector conn = {0};
	struct drm_mode_modeinfo mode;
	struct drm_mode_crtc c = {0};
	uint32_t encoder_ids[8];
	struct drm_mode_modeinfo modes[8];
	struct drm_mode_crtc_page_flip flip = { .flags = DRM_MODE_PAGE_FLIP_EVENT };
	struct drm_event_vblank ev;
	struct pollfd pfd;
	uint64_t start, returned, done, elapsed, budget, begin, previous_stamp = 0;
	uint32_t previous_seq = 0;
	unsigned completed = 0;
	int cadence_ok = 1;
	int ret;

	setvbuf(stdout, NULL, _IONBF, 0);
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--hold")) hold = 1;
		else if (!strcmp(argv[i], "--native-abgr")) native_abgr = 1;
		else if ((!strcmp(argv[i], "--frames") || !strcmp(argv[i], "--seconds") ||
			  !strcmp(argv[i], "--refresh-hz")) && i + 1 < argc) {
			const char *option = argv[i++];
			if (!strcmp(option, "--refresh-hz")) refresh = number(argv[i], 240);
			else {
				if (selection++) { fputs("choose frames OR seconds\n", stderr); return 2; }
				if (!strcmp(option, "--frames")) frames = number(argv[i], 3000);
				else { seconds = number(argv[i], 30); frames = 3000; }
			}
		} else if (argv[i][0] != '-' && positional < 2) {
			if (positional++) state = argv[i]; else card = argv[i];
		} else {
			fprintf(stderr, "usage: %s [card [iftu_state]] [--frames 1..3000 | "
				"--seconds 1..30] [--refresh-hz 1..240] [--hold] [--native-abgr]\n", argv[0]); return 2;
		}
	}
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
	/* A zero-count query forces fill_modes for the current DRM master. */
	puts("GETCONNECTOR (discover modes/counts)");
	check(!ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn), "GETCONNECTOR counts");
	printf("connector: id=%u connection=%u modes=%u encoders=%u\n",
	       conn.connector_id, conn.connection, conn.count_modes, conn.count_encoders);
	check(conn.connection == 1 && conn.count_modes == 1 && conn.count_encoders <= 8,
	      "expected connected fixed-mode connector");
	conn.count_modes = 8; conn.modes_ptr = (uintptr_t)modes;
	conn.count_encoders = 8; conn.encoders_ptr = (uintptr_t)encoder_ids;
	/* Counts query also returns property count; no property arrays requested. */
	conn.count_props = 0;
	puts("GETCONNECTOR (fetch mode)");
	check(!ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn), "GETCONNECTOR");
	check(conn.connection == 1 && conn.count_modes == 1 && conn.count_encoders <= 8,
	      "expected connected fixed-mode connector");
	mode = modes[0];
	check(mode.hdisplay && mode.vdisplay && mode.clock && mode.htotal && mode.vtotal,
	      "invalid connector mode");
	printf("mode: %ux%u clock=%u htotal=%u vtotal=%u\n", mode.hdisplay,
	       mode.vdisplay, mode.clock, mode.htotal, mode.vtotal);
	make_buffer(&buffers[0], &mode, 0x00202020); /* initial grey */
	/* Repeated tests must not strobe full-screen high-contrast colours. */
	make_buffer(&buffers[1], &mode, (frames > 1 || seconds) ? 0x00212121 : 0x00ff00ff);
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
	flip.crtc_id = crtc_id;
	/* Five periods of independently measured refresh, NOT connector clock metadata. */
	budget = (5000000000ULL + refresh - 1) / refresh;
	printf("refresh reference=%u Hz (%s); event budget=%" PRIu64 " ns\n", refresh,
	       refresh == 60 ? "default measured approximately 60" : "operator supplied", budget);
	begin = now_ns();
	for (unsigned i = 0; i < frames; i++) {
		uint64_t stamp;
		if (seconds && now_ns() - begin >= (uint64_t)seconds * 1000000000) break;
		flip.fb_id = buffers[(i + 1) % 2].fb;
		flip.user_data = 0x4d334232ULL + i;
		printf("PAGE_FLIP index=%u fb=%u cookie=%" PRIu64 ", event budget=%" PRIu64 " ns\n",
		       i + 1, flip.fb_id, (uint64_t)flip.user_data, budget);
		start = now_ns();
		check(!ioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip), "PAGE_FLIP");
		returned = now_ns();
		pfd = (struct pollfd){ .fd = fd, .events = POLLIN };
		do {
			elapsed = now_ns() - start;
			check(elapsed < budget, "flip exceeded five-frame budget");
			ret = poll(&pfd, 1, (int)((budget - elapsed + 999999) / 1000000));
		} while (ret < 0 && errno == EINTR);
		check(ret > 0 && (pfd.revents & POLLIN) && !(pfd.revents & (POLLERR | POLLHUP | POLLNVAL)),
		      "no flip event within five frames");
		ret = read(fd, &ev, sizeof(ev));
		done = now_ns(); elapsed = done - start;
		check(ret == sizeof(ev) && ev.base.length == sizeof(ev) &&
		      ev.base.type == DRM_EVENT_FLIP_COMPLETE && ev.user_data == flip.user_data &&
		      ev.crtc_id == crtc_id && ev.tv_usec < 1000000, "invalid flip event");
		stamp = (uint64_t)ev.tv_sec * 1000000000 + (uint64_t)ev.tv_usec * 1000;
		check(!i || (stamp > previous_stamp && (uint32_t)(ev.sequence - previous_seq) > 0 &&
		      (uint32_t)(ev.sequence - previous_seq) < 0x80000000U), "nonmonotonic flip event");
		previous_stamp = stamp; previous_seq = ev.sequence;
		check(elapsed <= budget, "late flip event");
		printf("DRM_EVENT received: crtc=%u sequence=%u elapsed_ns=%" PRIu64
		       " ioctl_ns=%" PRIu64 " event_wait_ns=%" PRIu64 " cookie=%" PRIu64 "\n",
		       ev.crtc_id, ev.sequence, elapsed, returned - start, done - returned,
		       (uint64_t)ev.user_data);
		completed++;
		if (frames > 1 && (i == 0 || completed % 61 == 0)) {
			char label[48];
			snprintf(label, sizeof(label), "flip sample %u", completed);
			snapshot(state, label);
		}
	}
	elapsed = now_ns() - begin;
	printf("total: flips=%u elapsed_ns=%" PRIu64 " cadence_hz=%.3f reference_hz=%u\n",
	       completed, elapsed, elapsed ? completed * 1e9 / elapsed : 0.0, refresh);
	puts("ioctl_ns includes all ioctl work (not exclusively CPU copy); event_wait_ns follows return.");
	if (selection && (frames > 1 || seconds)) {
		cadence_ok = elapsed && completed * 1e9 / elapsed >= refresh * 0.95;
		printf("cadence gate: %s (>=95%% refresh required); not a hardware presentation PASS\n",
		       cadence_ok ? "threshold met" : "BELOW threshold");
	}
	snapshot(state, "after flip");
	usleep(2000); snapshot(state, "after flip sample 2");
	usleep(3000); snapshot(state, "after flip sample 3");
	if (hold) {
		snapshot(state, "before hold");
		puts("HOLD ready: kill-test window 30 seconds; SIGKILL bypasses userspace cleanup");
		sleep(30);
	}
	puts("DRM-side completion only; compare selector and scanout evidence separately.");
	cleanup(); snapshot(state, "after cleanup");
	return cadence_ok && !cleanup_errors ? 0 : 1;
}
