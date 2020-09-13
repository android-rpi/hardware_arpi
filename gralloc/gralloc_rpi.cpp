#include <string>
#include <sys/errno.h>
#include <gralloc_drm.h>
#include <gralloc_drm_priv.h>

int drm_init(struct drm_module_t const* cmod) {
	struct drm_module_t *mod = (struct drm_module_t *) cmod;
	int err = 0;
	pthread_mutex_lock(&mod->mutex);
	if (!mod->drm) {
		mod->drm = gralloc_drm_create();
		if (!mod->drm)
			err = -EINVAL;
	}
	pthread_mutex_unlock(&mod->mutex);
	return err;
}

static int drm_mod_perform(struct gralloc_module_t const* mod, int op, ...)
{
	struct drm_module_t *dmod = (struct drm_module_t *) mod;
	va_list args;
	int err;

	err = drm_init(dmod);
	if (err)
		return err;

	va_start(args, op);
	switch (op) {
	case GRALLOC_MODULE_PERFORM_GET_DRM_FD:
		{
			int *fd = va_arg(args, int *);
			*fd = gralloc_drm_get_fd(dmod->drm);
			err = 0;
		}
		break;
	default:
		err = -EINVAL;
		break;
	}
	va_end(args);

	return err;
}

static int drm_mod_lock_ycbcr(struct gralloc_module_t const* /*mod*/,
		buffer_handle_t handle, int usage,
		int x, int y, int w, int h, struct android_ycbcr *ycbcr)
{
	int ret = 0;
	struct gralloc_drm_bo_t *bo = gralloc_drm_bo_from_handle(handle);
	if (!bo) {
		ret = -EINVAL;
	} else {
	    ret = gralloc_drm_bo_lock_ycbcr(bo, usage, x, y, w, h, ycbcr);
	}
	return ret;
}

static int drm_mod_unlock(struct gralloc_module_t const* /*mod*/, buffer_handle_t handle)
{
	int ret = 0;
	struct gralloc_drm_bo_t *bo = gralloc_drm_bo_from_handle(handle);
	if (!bo) {
		ret = -EINVAL;
	} else {
		gralloc_drm_bo_unlock(bo);
	}
	return ret;
}

struct drm_module_t HAL_MODULE_INFO_SYM = {
	.base = {
		.common = {
			.tag = HARDWARE_MODULE_TAG,
			.version_major = 1,
			.version_minor = 0,
			.id = GRALLOC_HARDWARE_MODULE_ID,
			.name = "arpi gralloc",
			.author = "Android-RPi",
		},
		.lock_ycbcr = drm_mod_lock_ycbcr,
		.perform = drm_mod_perform,
		.unlock = drm_mod_unlock,
	},
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.drm = NULL
};
