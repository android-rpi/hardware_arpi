#pragma once

#include <cutils/native_handle.h>
#include <gbm.h>
#include <pthread.h>
#include <hardware/gralloc.h>
#include <hardware/gralloc1.h>

struct gbm_module_t {
        gralloc_module_t base;
	pthread_mutex_t mutex;
	struct gbm_device *gbm;
};

int gbm_mod_init(struct gbm_module_t *mod);

int gbm_mod_register(struct gbm_module_t *mod, buffer_handle_t handle);
int gbm_mod_unregister(struct gbm_module_t *mod, buffer_handle_t handle);

int gbm_mod_lock_ycbcr(struct gbm_module_t *mod, buffer_handle_t handle, uint64_t usage, int x, int y, int w, int h, struct android_ycbcr *ycbcr);
int gbm_mod_unlock(struct gbm_module_t *mod, buffer_handle_t handle);
