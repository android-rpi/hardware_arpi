#ifndef _DRM_GRALLOC_MAPPER_H_
#define _DRM_GRALLOC_MAPPER_H_

#include <gralloc_drm.h>
#include <gralloc_drm_priv.h>

int drm_init(struct drm_module_t *mod);
int drm_register(struct drm_module_t *mod, buffer_handle_t handle);
int drm_unregister(buffer_handle_t handle);
int drm_lock(buffer_handle_t handle, int usage, int x, int y, int w, int h, void **ptr);
int drm_unlock(buffer_handle_t handle);

#endif /* _DRM_GRALLOC_MAPPER_H_ */
