#ifndef _DRM_GRALLOC_V3D_H_
#define _DRM_GRALLOC_V3D_H_

#include <gralloc_drm.h>
#include <gralloc_drm_priv.h>

int drm_init(struct drm_module_t *cmod);
void drm_deinit(struct drm_module_t *mod);
int drm_alloc(const struct drm_module_t *mod, int w, int h, int format, int usage, buffer_handle_t *handle, int *stride);
int drm_free(buffer_handle_t handle);

#endif /* _DRM_GRALLOC_V3D_H_ */
