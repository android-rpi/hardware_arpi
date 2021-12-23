/*
 * Copyright 2016-2022 Android-RPi Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "gralloc.rpi4"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <sys/errno.h>
#include <pthread.h>
#include <gbm_module.h>

static buffer_handle_t rhandle = NULL;

static int lock_ycbcr(struct gralloc_module_t const* mod,
		buffer_handle_t handle, int usage,
		int x, int y, int w, int h, struct android_ycbcr *ycbcr) {
    struct gbm_module_t *gmod = (struct gbm_module_t *)mod;

    if (gbm_mod_register(gmod, handle) == 0) {
        ALOGV("lock_ycbcr() register handle %p", handle);
        rhandle = handle;
    }

    return gbm_mod_lock_ycbcr(gmod, handle, usage, x, y, w, h, ycbcr);
}

static int unlock(struct gralloc_module_t const* mod, buffer_handle_t handle) {
    struct gbm_module_t *gmod = (struct gbm_module_t *)mod;
    if (!gmod->gbm) return -EINVAL;

    int ret = gbm_mod_unlock(gmod, handle);
    if (ret) return ret;

    if (rhandle == handle) {
        ALOGV("unlock() unregister handle %p", handle);
        gbm_mod_unregister(gmod, handle);
	rhandle = NULL;
    }

    return 0;
}

struct gbm_module_t HAL_MODULE_INFO_SYM = {
	.base = {
		.common = {
			.tag = HARDWARE_MODULE_TAG,
			.version_major = 1,
			.version_minor = 0,
			.id = GRALLOC_HARDWARE_MODULE_ID,
			.name = "arpi gralloc",
			.author = "Android-RPi",
		},
		.lock_ycbcr = lock_ycbcr,
		.unlock = unlock,
	},
	.mutex = PTHREAD_MUTEX_INITIALIZER,
	.gbm = NULL
};
