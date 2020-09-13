/*
 * Copyright (C) 2010-2011 Chia-I Wu <olvaffe@gmail.com>
 * Copyright (C) 2010-2011 LunarG Inc.
 * Copyright (C) 2020 Android-RPi Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define LOG_TAG "mapper@2.0-drm_gralloc_mapper"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <sys/errno.h>
#include <string>

#include "drm_gralloc_mapper.h"

static int drm_init(struct drm_module_t *mod) {
	int err = 0;
	if (!mod->drm) {
		mod->drm = gralloc_drm_create();
		if (!mod->drm)
			err = -EINVAL;
	}
	return err;
}

int drm_register(struct drm_module_t *mod, buffer_handle_t handle) {
	pthread_mutex_lock(&mod->mutex);
	int ret = drm_init(mod);
	if (ret == 0) {
	    ret = gralloc_drm_handle_register(handle, mod->drm);
	}
	pthread_mutex_unlock(&mod->mutex);
	return ret;
}

int drm_unregister(buffer_handle_t handle) {
	return gralloc_drm_handle_unregister(handle);
}

int drm_lock(buffer_handle_t handle, int usage, int x, int y, int w, int h, void **ptr) {
	int ret = 0;
	struct gralloc_drm_bo_t *bo = gralloc_drm_bo_from_handle(handle);
	if (!bo) {
		ret = -EINVAL;
	} else {
	    ret = gralloc_drm_bo_lock(bo, usage, x, y, w, h, ptr);
	}
	return ret;
}

int drm_lock_ycbcr(buffer_handle_t handle, int usage, int x, int y, int w, int h,
		struct android_ycbcr *ycbcr) {
	int ret = 0;
	struct gralloc_drm_bo_t *bo = gralloc_drm_bo_from_handle(handle);
	if (!bo) {
		ret = -EINVAL;
	} else {
	    ret = gralloc_drm_bo_lock_ycbcr(bo, usage, x, y, w, h, ycbcr);
	}
	return ret;
}

int drm_unlock(buffer_handle_t handle) {
	int ret = 0;
	struct gralloc_drm_bo_t *bo = gralloc_drm_bo_from_handle(handle);
	if (!bo) {
		ret = -EINVAL;
	} else {
		gralloc_drm_bo_unlock(bo);
	}
	return ret;
}


