// SPDX-License-Identifier: GPL-2.0-only
/*
 * drm-flip-probe -- one userspace page flip through the standard DRM ABI.
 *
 * M3-B2 of docs/plans/2026-10-21-gpu-m3-userspace-flip-gate.md.
 *
 * WHY RAW IOCTLS
 * --------------
 * libdrm would be one more thing to install on a console whose root is a loop
 * image on an exFAT card. The whole probe is four ioctls and a poll(); using the
 * kernel UAPI directly keeps it a single static binary that can be copied over
 * and run with nothing else on the device.
 *
 * WHAT IT DOES
 * ------------
 *   1. reads the card's CRTC state        (is anything actually scanning out?)
 *   2. allocates and fills a dumb buffer (a colour the console is not using)
 *   3. ADDFB
 *   4. PAGE_FLIP with DRM_MODE_PAGE_FLIP_EVENT
 *   5. waits for the completion event, bounded
 *
 * Every step is printed before it is attempted, so a failure leaves a record of
 * how far it got rather than a single exit code. It never writes registers and
 * never touches /dev/mem: the selector register is read separately through the
 * driver's debugfs "state" file, because the driver is the only thing that owns
 * that mapping.
 *
 * Exit: 0 flip submitted and completed, 1 failed at some step, 2 usage/setup.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
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

static int g_fd = -1;

static void die(const char *what)
{
	printf("FAIL %s: %s\n", what, strerror(errno));
	exit(1);
}

static void note(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
	fflush(stdout);
}

static uint32_t pick_crtc(uint32_t *out_count)
{
	struct drm_mode_card_res res;
	uint32_t crtcs[8] = {0};
	uint32_t connectors[8] = {0};
	uint32_t encoders[8] = {0};
	int ret;

	memset(&res, 0, sizeof(res));
	res.count_crtcs = 8;
	res.count_connectors = 8;
	res.count_encoders = 8;
	res.crtc_id_ptr = (uint64_t)(uintptr_t)crtcs;
	res.connector_id_ptr = (uint64_t)(uintptr_t)connectors;
	res.encoder_id_ptr = (uint64_t)(uintptr_t)encoders;

	ret = ioctl(g_fd, DRM_IOCTL_MODE_GETRESOURCES, &res);
	if (ret)
		die("GETRESOURCES");

	*out_count = res.count_crtcs;
	note("card: %u crtc(s), %u connector(s), %u encoder(s)",
	     res.count_crtcs, res.count_connectors, res.count_encoders);
	if (!res.count_crtcs) {
		printf("FAIL no CRTC on this card\n");
		exit(1);
	}
	return crtcs[0];
}

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/dev/dri/card0";
	struct drm_mode_crtc crtc;
	uint32_t crtc_count;
	uint32_t crtc_id;
	struct drm_mode_create_dumb dumb;
	struct drm_mode_map_dumb map;
	struct drm_mode_fb_cmd fb;
	uint32_t *pixels;
	uint32_t colour = 0x00ff00ff; /* magenta in XRGB8888 */
	size_t i, npix;
	struct drm_mode_crtc_page_flip flip;
	struct pollfd pfd;
	struct drm_event_vblank ev;
	uint64_t cookie = 0x4d334232ULL; /* "M3B2" */
	int ret;

	note("M3-B2 probe: %s", path);
	g_fd = open(path, O_RDWR | O_CLOEXEC);
	if (g_fd < 0)
		die("open card");

	crtc_id = pick_crtc(&crtc_count);

	/* 1. what is the CRTC actually doing? */
	memset(&crtc, 0, sizeof(crtc));
	crtc.crtc_id = crtc_id;
	ret = ioctl(g_fd, DRM_IOCTL_MODE_GETCRTC, &crtc);
	if (ret)
		die("GETCRTC");
	note("crtc %u: mode=%ux%u fb_id=%u enabled=%s",
	     crtc_id, crtc.mode.hdisplay, crtc.mode.vdisplay, crtc.fb_id,
	     crtc.mode_valid && crtc.mode.hdisplay ? "yes" : "no");
	if (!crtc.mode.hdisplay) {
		printf("FAIL crtc has no mode; nothing to flip onto\n");
		return 1;
	}

	/* 2. a dumb buffer at the CRTC's size */
	memset(&dumb, 0, sizeof(dumb));
	dumb.width = crtc.mode.hdisplay;
	dumb.height = crtc.mode.vdisplay;
	dumb.bpp = 32;
	ret = ioctl(g_fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb);
	if (ret)
		die("CREATE_DUMB");
	note("dumb buffer: handle=%u %ux%u pitch=%u size=%u",
	     dumb.handle, dumb.width, dumb.height, dumb.pitch, dumb.size);

	memset(&map, 0, sizeof(map));
	map.handle = dumb.handle;
	if (ioctl(g_fd, DRM_IOCTL_MODE_MAP_DUMB, &map))
		die("MAP_DUMB");

	pixels = mmap(NULL, dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED,
		      g_fd, map.offset);
	if (pixels == MAP_FAILED)
		die("mmap dumb");
	npix = dumb.size / 4;
	for (i = 0; i < npix; i++)
		pixels[i] = colour;
	note("filled %zu pixels with 0x%06x", npix, colour & 0xffffff);

	/* 3. expose it as a framebuffer */
	memset(&fb, 0, sizeof(fb));
	fb.width = dumb.width;
	fb.height = dumb.height;
	fb.pitch = dumb.pitch;
	fb.bpp = dumb.bpp;
	fb.depth = 24;
	fb.handle = dumb.handle;
	if (ioctl(g_fd, DRM_IOCTL_MODE_ADDFB, &fb))
		die("ADDFB");
	note("addfb: fb_id=%u", fb.fb_id);

	/* 4. the flip itself */
	memset(&flip, 0, sizeof(flip));
	flip.crtc_id = crtc_id;
	flip.fb_id = fb.fb_id;
	flip.flags = DRM_MODE_PAGE_FLIP_EVENT;
	flip.user_data = cookie;
	note("PAGE_FLIP submitted (cookie=0x%llx)", (unsigned long long)cookie);
	if (ioctl(g_fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip))
		die("PAGE_FLIP");

	/* 5. the completion event, bounded (~2 s covers ~100 frames at 52 Hz) */
	pfd.fd = g_fd;
	pfd.events = POLLIN;
	ret = poll(&pfd, 1, 2000);
	if (ret < 0)
		die("poll");
	if (ret == 0) {
		printf("FAIL no flip event within 2000 ms\n");
		return 1;
	}

	memset(&ev, 0, sizeof(ev));
	ret = read(g_fd, &ev, sizeof(ev));
	if (ret < (int)sizeof(ev)) {
		printf("FAIL short event read (%d bytes)\n", ret);
		return 1;
	}
	if (ev.base.type != DRM_EVENT_FLIP_COMPLETE) {
		printf("FAIL unexpected event type %u\n", ev.base.type);
		return 1;
	}
	if (ev.user_data != cookie) {
		printf("FAIL event cookie 0x%llx != 0x%llx\n",
		       (unsigned long long)ev.user_data, (unsigned long long)cookie);
		return 1;
	}

	printf("PASS flip completed by the kernel: crtc=%u fb=%u sequence=%u\n",
	       ev.crtc_id, fb.fb_id, ev.sequence);
	printf("NOTE this is the DRM-side half only. The gate also requires the\n"
	       "     selector register to have changed and scanout to be advancing;\n"
	       "     read those from the driver's debugfs \"state\" file.\n");
	return 0;
}
