// SPDX-License-Identifier: GPL-2.0-only
/* Host-only ABI/control-flow fixture. NOT hardware evidence.
 * Link the real probe with --wrap=ioctl,mmap,munmap,poll,read.
 */
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
static int creates, enabled, flipped;
static unsigned long scenario(void)
{
	const char *s = getenv("MOCK_SCENARIO");
	return s ? strtoul(s, NULL, 10) : 0;
}
int __wrap_ioctl(int fd, unsigned long req, ...)
{
	va_list ap;
	void *p;
	(void)fd;
	va_start(ap, req); p = va_arg(ap, void *); va_end(ap);
	switch (req) {
	case DRM_IOCTL_SET_MASTER: return 0;
	case DRM_IOCTL_MODE_GETRESOURCES: {
		struct drm_mode_card_res *r = p;
		*(uint32_t *)(uintptr_t)r->crtc_id_ptr = 34;
		*(uint32_t *)(uintptr_t)r->connector_id_ptr = 36;
		r->count_crtcs = r->count_connectors = r->count_encoders = 1;
		return 0;
	}
	case DRM_IOCTL_MODE_GETCRTC: {
		struct drm_mode_crtc *c = p;
		c->mode_valid = enabled || scenario() == 1;
		c->fb_id = c->mode_valid ? 101 : 0;
		return 0;
	}
	case DRM_IOCTL_MODE_GETCONNECTOR: {
		struct drm_mode_get_connector *c = p;
		struct drm_mode_modeinfo *m = (void *)(uintptr_t)c->modes_ptr;
		*m = (struct drm_mode_modeinfo){ .hdisplay = 1280, .vdisplay = 720,
			.htotal = 1650, .vtotal = 750, .clock = 64473 };
		c->connection = 1; c->count_modes = c->count_encoders = 1;
		return 0;
	}
	case DRM_IOCTL_MODE_CREATE_DUMB: {
		struct drm_mode_create_dumb *d = p;
		d->handle = ++creates; d->pitch = d->width * 4;
		d->size = (uint64_t)d->pitch * d->height;
		return 0;
	}
	case DRM_IOCTL_MODE_MAP_DUMB: return 0;
	case DRM_IOCTL_MODE_ADDFB: {
		struct drm_mode_fb_cmd *f = p;
		f->fb_id = 100 + f->handle; return 0;
	}
	case DRM_IOCTL_MODE_SETCRTC: {
		struct drm_mode_crtc *c = p;
		if (c->fb_id && scenario() == 2) { errno = EINVAL; return -1; }
		enabled = !!c->fb_id; return 0;
	}
	case DRM_IOCTL_MODE_PAGE_FLIP: {
		struct drm_mode_crtc_page_flip *f = p;
		assert(enabled && creates == 2 && f->fb_id == 102);
		flipped = 1; return 0;
	}
	case DRM_IOCTL_MODE_RMFB:
	case DRM_IOCTL_MODE_DESTROY_DUMB: return 0;
	default: abort();
	}
}
void *__wrap_mmap(void *a, size_t n, int prot, int flags, int fd, off_t off)
{
	(void)a; (void)prot; (void)flags; (void)fd; (void)off;
	return calloc(1, n);
}
int __wrap_munmap(void *p, size_t n) { (void)n; free(p); return 0; }
int __wrap_poll(struct pollfd *p, nfds_t n, int timeout)
{
	assert(flipped && n == 1 && timeout > 0 && timeout < 110);
	p->revents = scenario() == 3 ? POLLERR : POLLIN; return 1;
}
ssize_t __wrap_read(int fd, void *p, size_t n)
{
	struct drm_event_vblank e = { .base = { .type = DRM_EVENT_FLIP_COMPLETE,
		.length = sizeof(e) }, .user_data = 0x4d334232, .crtc_id = 34, .sequence = 2 };
	(void)fd; assert(n == sizeof(e));
	if (scenario() == 4) e.user_data = 0;
	memcpy(p, &e, sizeof(e)); return sizeof(e);
}
