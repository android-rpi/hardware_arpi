/*
 * Copyright 2022 Android-RPi Project
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

#define LOG_TAG "GBM-MODULE"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <sys/errno.h>

#include <gbm_gralloc.h>

#include "gbm_module.h"

int gbm_mod_init(struct gbm_module_t *mod) {
	int err = 0;
	if (!mod->gbm) {
		pthread_mutex_init(&mod->mutex, nullptr);
		mod->gbm = gbm_init();
		if (!mod->gbm)
			err = -EINVAL;
	}
	return err;
}

int gbm_mod_register(struct gbm_module_t *mod, buffer_handle_t handle) {
	int ret = gbm_mod_init(mod);
	if (ret == 0) {
		pthread_mutex_lock(&mod->mutex);
		ret = gbm_register(mod->gbm, handle);
		pthread_mutex_unlock(&mod->mutex);
	}
    return ret;
}

int gbm_mod_unregister(struct gbm_module_t *mod, buffer_handle_t handle) {
	int err;
	pthread_mutex_lock(&mod->mutex);
	err = gbm_unregister(handle);
	pthread_mutex_unlock(&mod->mutex);
	return err;
}

int gbm_mod_lock_ycbcr(struct gbm_module_t *mod, buffer_handle_t handle,
        uint64_t usage, int x, int y, int w, int h, struct android_ycbcr *ycbcr) {
	int err;
	pthread_mutex_lock(&mod->mutex);
	err = gbm_lock_ycbcr(handle, usage, x, y, w, h, ycbcr);
	pthread_mutex_unlock(&mod->mutex);
	return err;
}

int gbm_mod_unlock(struct gbm_module_t *mod, buffer_handle_t handle) {
	int err;
	pthread_mutex_lock(&mod->mutex);
	err = gbm_unlock(handle);
	pthread_mutex_unlock(&mod->mutex);
	return err;
}
