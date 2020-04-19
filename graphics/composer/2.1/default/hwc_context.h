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

class hwc_context {
  public :
    hwc_context();
    int hwc_post(buffer_handle_t handle);

    uint32_t  width;
    uint32_t  height;
    int       format;
    float     fps;
    float     xdpi;
    float     ydpi;

  private:
    int init_kms();
    drmModeConnectorPtr fetch_connector(uint32_t type);
    int init_with_connector(struct kms_output *output,
    		drmModeConnectorPtr connector);
    void init_features();
    int bo_post(struct gralloc_drm_bo_t *bo);
    void wait_for_post(int flip);
    int set_crtc(struct kms_output *output, int fb_id);
    int bo_add_fb(struct gralloc_drm_bo_t *bo);

	int kms_fd;
	drmModeResPtr resources;
	struct kms_output primary_output;

	int swap_interval;
	drmEventContext evctx;
	int first_post;
	unsigned int last_swap;

  public:
    int page_flip(struct gralloc_drm_bo_t *bo);
    int waiting_flip;
    struct gralloc_drm_bo_t *current_front, *next_front;
};

} // namespace android
