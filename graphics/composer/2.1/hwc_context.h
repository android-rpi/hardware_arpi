/*
 * Copyright 2020 Android-RPi Project
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

#pragma once

#include <xf86drmMode.h>
#include <gralloc_drm_priv.h>

namespace android {

struct kms_output
{
	uint32_t crtc_id;
	uint32_t connector_id;
	uint32_t pipe;
	drmModeModeInfo mode;
	int xdpi, ydpi;
	int fb_format;
	int bpp;
	uint32_t active;
};

#ifndef ANDROID_HARDWARE_HWCOMPOSER2_H
typedef uint64_t hwc2_display_t;
#endif

class hwc_context {
  public :
    hwc_context();
    int hwc_post(hwc2_display_t display_id, buffer_handle_t handle);
    bool is_display2_active();

    uint32_t  width;
    uint32_t  height;
    int       format;
    float     fps;
    float     xdpi;
    float     ydpi;

  private:
    int init_kms();
    drmModeConnectorPtr fetch_connector(uint32_t type);
    drmModeConnectorPtr fetch_connector2(uint32_t type);
    int init_with_connector(struct kms_output *output,
    		drmModeConnectorPtr connector);
    void init_features();
    int bo_post(struct gralloc_drm_bo_t *bo);
    int bo_post2(struct gralloc_drm_bo_t *bo);
    void wait_for_post(int flip);
    void wait_for_post2(int flip);
    int set_crtc(struct kms_output *output, int fb_id);
    int bo_add_fb(struct gralloc_drm_bo_t *bo);

	int kms_fd;
	drmModeResPtr resources;
	int primary_connector;
	struct kms_output primary_output;
	struct kms_output secondary_output;

	int swap_interval;
	drmEventContext evctx, evctx2;
	int first_post, first_post2;
	unsigned int last_swap, last_swap2;

  public:
    int page_flip(struct gralloc_drm_bo_t *bo);
    int page_flip2(struct gralloc_drm_bo_t *bo);
    int waiting_flip, waiting_flip2;
    struct gralloc_drm_bo_t *current_front, *next_front;
    struct gralloc_drm_bo_t *current_front2, *next_front2;
};

} // namespace android
