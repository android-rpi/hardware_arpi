#pragma once

#include <gralloc_drm.h>
#include <gralloc_drm_priv.h>

int drm_register(struct drm_module_t *mod, buffer_handle_t handle);
int drm_unregister(buffer_handle_t handle);
int drm_lock(buffer_handle_t handle, int usage, int x, int y, int w, int h, void **ptr);
int drm_lock_ycbcr(buffer_handle_t handle, int usage, int x, int y, int w, int h, struct android_ycbcr *ycbcr);
int drm_unlock(buffer_handle_t handle);

