/*
 * Copyright (C) 2010-2011 Chia-I Wu
 * Copyright (C) 2010-2011 LunarG Inc.
 * Copyright (C) 2020-2023 Android-RPi Project
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

#define LOG_TAG "composer-hwc_context"
//#define LOG_NDEBUG 0
#include <cutils/properties.h>
#include <utils/Log.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <math.h>
#include <system/graphics.h>
#include <hardware_legacy/uevent.h>

#include <drm_fourcc.h>

#include "hwc_context.h"

namespace aidl::android::hardware::graphics::composer3::impl {

int hwc_context::add_fb(const private_handle_t *hnd)
{
	if (hnd->fb_id)
		return 0;

	uint32_t pitches[4] = { 0, 0, 0, 0 };
	uint32_t offsets[4] = { 0, 0, 0, 0 };
	uint32_t handles[4] = { 0, 0, 0, 0 };
	uint64_t modifiers[4] = { 0, 0, 0, 0 };

        uint32_t width = (uint32_t)primary_output.mode.hdisplay;
        uint32_t height = (uint32_t)primary_output.mode.vdisplay;
        uint32_t drm_format = primary_output.drm_format;

	uint32_t handle;
	int ret = drmPrimeFDToHandle(kms_fd, hnd->fd, &handle);
	if (ret != 0) {
		ALOGE("add_fb() error drmPrimeFDToHandle()");
		return ret;
	}

	pitches[0] = width * 4;
	handles[0] = handle;
	modifiers[0] = DRM_FORMAT_MOD_LINEAR;

	ALOGV("add_fb() width:%d height:%d format:%x handle:%d pitch:%d",
			width, height, drm_format, handle, pitches[0]);
	return drmModeAddFB2WithModifiers(kms_fd,
		width, height,
		drm_format, handles, pitches, offsets, modifiers,
                (uint32_t *)&hnd->fb_id, DRM_MODE_FB_MODIFIERS);
}


static int64_t get_property_value(int fd, drmModeObjectPropertiesPtr props,
				  const char *name) {
	drmModePropertyPtr prop;
	uint64_t value;

	bool found = false;
	for (int j = 0; j < props->count_props && !found; j++) {
		prop = drmModeGetProperty(fd, props->props[j]);
		if (!strcmp(prop->name, name)) {
			value = props->prop_values[j];
			found = true;
		}
		drmModeFreeProperty(prop);
	}

	if (!found)
		return -1;
	return value;
}

static uint32_t get_property_id(int fd, drmModeObjectPropertiesPtr props,
				   const char *name) {
	drmModePropertyPtr prop;
	uint32_t prop_id = 0;

	bool found = false;
	for (int j = 0; j < props->count_props && !found; j++) {
		prop = drmModeGetProperty(fd, props->props[j]);
		if (!strcmp(prop->name, name)) {
		    prop_id = prop->prop_id;
			found = true;
		}
		drmModeFreeProperty(prop);
	}
	return prop_id;
}


int hwc_context::atomic_commit(hwc2_display_t display_id, struct kms_output *output,
			       const private_handle_t *hnd, int32_t *out_fence) {
    if (!hnd)
        return 0;

    int ret = 0;
    drmModeAtomicReq *req = drmModeAtomicAlloc();
    drmModeAtomicAddProperty(req, output->crtc_id, output->prop_out_fence, uint64_t(out_fence));
    drmModeAtomicAddProperty(req, output->plane_id, output->prop_fb_id, hnd->fb_id);
    drmModeAtomicAddProperty(req, output->plane_id, output->prop_crtc_id, output->crtc_id);

    uint32_t flags = DRM_MODE_ATOMIC_ALLOW_MODESET | DRM_MODE_ATOMIC_NONBLOCK;
    ret = drmModeAtomicCommit(kms_fd, req, flags, (void *)this);
    if (ret < 0)  {
        ALOGE("failed to perform page flip for primary (%s) (crtc %d fb %d))",
            strerror(errno), output->crtc_id, hnd->fb_id);
        /* try to set mode for next frame */
        if (errno != EBUSY) {
           if (display_id == 0) first_post = 1;
           else if (display_id == 1) first_post2 = 1;
        }
    }
    drmModeAtomicFree(req);
    return ret < 0 ? ret : 0; 
}

int hwc_context::hwc_post(hwc2_display_t display_id, buffer_handle_t buffer, int32_t *out_fence)
{
    if (private_handle_t::validate(buffer) < 0)
       return -EINVAL;

    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(buffer);

	if (!hnd->fb_id) {
		int err = add_fb(hnd);
		if (err) {
			ALOGE("%s: could not create drm fb, (%s)",
				__func__, strerror(-err));
			ALOGE("unable to post %p without fb", hnd);
			return err;
		}
	}

    int ret;
    struct kms_output *output;
    if (display_id == 0) {
	output = &primary_output;
	if (first_post) {
		ret = drmModeSetCrtc(kms_fd, output->crtc_id, hnd->fb_id,
			0, 0, &output->connector_id, 1, &output->mode);
		if (!ret) first_post = 0;
                *out_fence = -1;
		return ret;
        }
    } else if (display_id == 1){
	output = &secondary_output;
	if (first_post2) {
		ret = drmModeSetCrtc(kms_fd, output->crtc_id, hnd->fb_id,
			0, 0, &output->connector_id, 1, &output->mode);
		if (!ret) first_post2 = 0;
                *out_fence = -1;
		return ret;
        }
    } else {
        return -EINVAL;
    }

    ret = atomic_commit(display_id, output, hnd, out_fence);
    ALOGV("hwc_post() fd %d, fb_id %d, out_fence %d",
        hnd->fd, hnd->fb_id, *out_fence);

    return ret;
}

bool hwc_context::is_display2_active() {
    return (secondary_output.active == 1);
}


#define MARGIN_PERCENT 1.8   /* % of active vertical image*/
#define CELL_GRAN 8.0   /* assumed character cell granularity*/
#define MIN_PORCH 1 /* minimum front porch   */
#define V_SYNC_RQD 3 /* width of vsync in lines   */
#define H_SYNC_PERCENT 8.0   /* width of hsync as % of total line */
#define MIN_VSYNC_PLUS_BP 550.0 /* min time of vsync + back porch (microsec) */
#define M 600.0 /* blanking formula gradient */
#define C 40.0  /* blanking formula offset   */
#define K 128.0 /* blanking formula scaling factor   */
#define J 20.0  /* blanking formula scaling factor   */
/* C' and M' are part of the Blanking Duty Cycle computation */
#define C_PRIME   (((C - J) * K / 256.0) + J)
#define M_PRIME   (K / 256.0 * M)

static drmModeModeInfoPtr generate_mode(int h_pixels, int v_lines, float freq)
{
	float h_pixels_rnd;
	float v_lines_rnd;
	float v_field_rate_rqd;
	float top_margin;
	float bottom_margin;
	float interlace;
	float h_period_est;
	float vsync_plus_bp;
	float v_back_porch;
	float total_v_lines;
	float v_field_rate_est;
	float h_period;
	float v_field_rate;
	float v_frame_rate;
	float left_margin;
	float right_margin;
	float total_active_pixels;
	float ideal_duty_cycle;
	float h_blank;
	float total_pixels;
	float pixel_freq;
	float h_freq;

	float h_sync;
	float h_front_porch;
	float v_odd_front_porch_lines;
	int interlaced = 0;
	int margins = 0;

	drmModeModeInfoPtr m = (drmModeModeInfoPtr) malloc(sizeof(drmModeModeInfo));

	h_pixels_rnd = rint((float) h_pixels / CELL_GRAN) * CELL_GRAN;
	v_lines_rnd = interlaced ? rint((float) v_lines) / 2.0 : rint((float) v_lines);
	v_field_rate_rqd = interlaced ? (freq * 2.0) : (freq);
	top_margin = margins ? rint(MARGIN_PERCENT / 100.0 * v_lines_rnd) : (0.0);
	bottom_margin = margins ? rint(MARGIN_PERCENT / 100.0 * v_lines_rnd) : (0.0);
	interlace = interlaced ? 0.5 : 0.0;
	h_period_est = (((1.0 / v_field_rate_rqd) - (MIN_VSYNC_PLUS_BP / 1000000.0)) / (v_lines_rnd + (2 * top_margin) + MIN_PORCH + interlace) * 1000000.0);
	vsync_plus_bp = rint(MIN_VSYNC_PLUS_BP / h_period_est);
	v_back_porch = vsync_plus_bp - V_SYNC_RQD;
	total_v_lines = v_lines_rnd + top_margin + bottom_margin + vsync_plus_bp + interlace + MIN_PORCH;
	v_field_rate_est = 1.0 / h_period_est / total_v_lines * 1000000.0;
	h_period = h_period_est / (v_field_rate_rqd / v_field_rate_est);
	v_field_rate = 1.0 / h_period / total_v_lines * 1000000.0;
	v_frame_rate = interlaced ? v_field_rate / 2.0 : v_field_rate;
	left_margin = margins ? rint(h_pixels_rnd * MARGIN_PERCENT / 100.0 / CELL_GRAN) * CELL_GRAN : 0.0;
	right_margin = margins ? rint(h_pixels_rnd * MARGIN_PERCENT / 100.0 / CELL_GRAN) * CELL_GRAN : 0.0;
	total_active_pixels = h_pixels_rnd + left_margin + right_margin;
	ideal_duty_cycle = C_PRIME - (M_PRIME * h_period / 1000.0);
	h_blank = rint(total_active_pixels * ideal_duty_cycle / (100.0 - ideal_duty_cycle) / (2.0 * CELL_GRAN)) * (2.0 * CELL_GRAN);
	total_pixels = total_active_pixels + h_blank;
	pixel_freq = total_pixels / h_period;
	h_freq = 1000.0 / h_period;
	h_sync = rint(H_SYNC_PERCENT / 100.0 * total_pixels / CELL_GRAN) * CELL_GRAN;
	h_front_porch = (h_blank / 2.0) - h_sync;
	v_odd_front_porch_lines = MIN_PORCH + interlace;

	m->clock = ceil(pixel_freq) * 1000;
	m->hdisplay = (int) (h_pixels_rnd);
	m->hsync_start = (int) (h_pixels_rnd + h_front_porch);
	m->hsync_end = (int) (h_pixels_rnd + h_front_porch + h_sync);
	m->htotal = (int) (total_pixels);
	m->hskew = 0;
	m->vdisplay = (int) (v_lines_rnd);
	m->vsync_start = (int) (v_lines_rnd + v_odd_front_porch_lines);
	m->vsync_end = (int) (int) (v_lines_rnd + v_odd_front_porch_lines + V_SYNC_RQD);
	m->vtotal = (int) (total_v_lines);
	m->vscan = 0;
	m->vrefresh = freq;
	m->flags = DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC;
	m->type = DRM_MODE_TYPE_DRIVER;

	return (m);
}

static drmModeModeInfoPtr find_mode(drmModeConnectorPtr connector)
{
	char value[PROPERTY_VALUE_MAX];
	drmModeModeInfoPtr mode;
	int dist = INT_MAX, i;
	int xres = 0, yres = 0, rate = 0;
	int forcemode = 0;
	int found_prop_match = 0;

	if (property_get("debug.drm.mode", value, NULL)) {
		/* parse <xres>x<yres>[@<refreshrate>] */
		if (sscanf(value, "%dx%d@%d", &xres, &yres, &rate) != 3) {
			rate = 60;
			if (sscanf(value, "%dx%d", &xres, &yres) != 2)
				xres = yres = 0;
		}

		if (xres && yres) {
			ALOGV("will find the closest match for %dx%d",
					xres, yres);
		}
	} else if (property_get("debug.drm.mode.force", value, NULL)) {
		/* parse <xres>x<yres>[@<refreshrate>] */
		if (sscanf(value, "%dx%d@%d", &xres, &yres, &rate) != 3) {
			rate = 60;
			if (sscanf(value, "%dx%d", &xres, &yres) != 2)
				xres = yres = 0;
		}

		if (xres && yres && rate) {
			ALOGV("will use %dx%d@%dHz", xres, yres, rate);
			forcemode = 1;
		}
	}

	if (xres && yres && rate) {
		for (i = 0; i < connector->count_modes; i++) {
			drmModeModeInfoPtr m = &connector->modes[i];
			if ((m->hdisplay == xres) && (m->vdisplay == yres)
					&& (m->vrefresh == rate)) {
				mode = m;
				found_prop_match = 1;
				ALOGV("Found property match %dx%d@%dHz", xres, yres, rate);
				break;
			}
		}
	}

	if (!found_prop_match) {
		if (forcemode)
			mode = generate_mode(xres, yres, rate);
		else {
			mode = NULL;
			for (i = 0; i < connector->count_modes; i++) {
				drmModeModeInfoPtr m = &connector->modes[i];
				int tmp;

				if (xres && yres) {
					tmp = (m->hdisplay - xres) * (m->hdisplay - xres) +
							(m->vdisplay - yres) * (m->vdisplay - yres);
				}
				else {
					/* use the first preferred mode */
					tmp = (m->type & DRM_MODE_TYPE_PREFERRED) ? 0 : dist;
				}

				if (tmp < dist) {
					mode = m;
					dist = tmp;
					if (!dist)
						break;
				}
			}
		}
		/* fallback to the first mode */
		if (!mode)
			mode = &connector->modes[0];
	}

	if ((74200 <= mode->clock) && (mode->clock < 74500)) {
		// Avoid interference with Wifi
		ALOGV("Trim display clock from %d to 75000", mode->clock);
		mode->clock = 75000;
	}

	ALOGV("Established mode:");
	ALOGV("clock: %d, hdisplay: %d, hsync_start: %d, hsync_end: %d, htotal: %d, hskew: %d", mode->clock, mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal, mode->hskew);
	ALOGV("vdisplay: %d, vsync_start: %d, vsync_end: %d, vtotal: %d, vscan: %d, vrefresh: %d", mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal, mode->vscan, mode->vrefresh);
	ALOGV("flags: %d, type: %d, name %s", mode->flags, mode->type, mode->name);

	return mode;
}

/*
 * Initialize KMS with a connector.
 */
int hwc_context::init_with_connector(struct kms_output *output,
		drmModeConnectorPtr connector) {
	drmModeEncoderPtr encoder;
	drmModeModeInfoPtr mode;
	static int used_crtcs = 0;
	int i, j;

	encoder = drmModeGetEncoder(kms_fd, connector->encoders[0]);
	if (!encoder)
		return -EINVAL;

	/* find first possible crtc which is not used yet */
	for (i = 0; i < resources->count_crtcs; i++) {
		if (encoder->possible_crtcs & (1 << i) &&
			(used_crtcs & (1 << i)) != (1 << i))
			break;
	}

	used_crtcs |= (1 << i);

	drmModeFreeEncoder(encoder);
	if (i == resources->count_crtcs)
		return -EINVAL;

	/* find primary plane id */
	bool found_primary = false;
	output->plane_id = 0;
	for (j = 0; j < plane_resources->count_planes && !found_primary; j++) {
		uint32_t plane_id = plane_resources->planes[j];
		drmModePlanePtr plane = drmModeGetPlane(kms_fd, plane_resources->planes[j]);
		if (!plane) {
			ALOGW("drmModeGetPlane(%u) failed", plane_resources->planes[j]);
			continue;
		}
		if (plane->possible_crtcs & (1 << i)) {
			drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(kms_fd,
					plane_resources->planes[j], DRM_MODE_OBJECT_PLANE);
			if (get_property_value(kms_fd, props, "type") == DRM_PLANE_TYPE_PRIMARY) {
				found_primary = true;
				output->plane_id = plane_id;
				output->prop_fb_id = get_property_id(kms_fd, props, "FB_ID");
				output->prop_crtc_id = get_property_id(kms_fd, props, "CRTC_ID");
				ALOGI("found primary plane %u, fb %u, crtc %u", plane_id,
				        output->prop_fb_id, output->prop_crtc_id);
			}
			drmModeFreeObjectProperties(props);
		}
		drmModeFreePlane(plane);
	}

	output->crtc_id = resources->crtcs[i];
	drmModeObjectPropertiesPtr crtc_props = drmModeObjectGetProperties(kms_fd,
			output->crtc_id, DRM_MODE_OBJECT_CRTC);
	output->prop_out_fence = get_property_id(kms_fd, crtc_props, "OUT_FENCE_PTR");
	ALOGI("prop_out_fence %u", output->prop_out_fence);
	drmModeFreeObjectProperties(crtc_props);

	output->connector_id = connector->connector_id;
	output->pipe = i;

	/* print connector info */
	ALOGI("there are %d modes on connector 0x%x, type %d",
		connector->count_modes,
		connector->connector_id,
		connector->connector_type);
	for (i = 0; i < connector->count_modes; i++) {
		ALOGV("  %s@%dHz flags:0x%x type:0x%x", connector->modes[i].name, connector->modes[i].vrefresh,
				connector->modes[i].flags, connector->modes[i].type);
	}

	mode = find_mode(connector);
	ALOGI("the best mode is %s", mode->name);

	output->mode = *mode;
	output->drm_format = DRM_FORMAT_ABGR8888;

	if (connector->mmWidth && connector->mmHeight) {
		output->xdpi = (output->mode.hdisplay * 25.4 / connector->mmWidth);
		output->ydpi = (output->mode.vdisplay * 25.4 / connector->mmHeight);
	}
	else {
		output->xdpi = 75;
		output->ydpi = 75;
	}

	return 0;
}


/*
 * Fetch a connector of particular type
 */
drmModeConnectorPtr hwc_context::fetch_connector(uint32_t type)
{
	int i;
	primary_connector = -1;

	if (!resources)
		return NULL;

	for (i = 0; i < resources->count_connectors; i++) {
		drmModeConnectorPtr connector =
			connector = drmModeGetConnector(kms_fd,
				resources->connectors[i]);
		if (connector) {
			if (connector->connector_type == type &&
				connector->connection == DRM_MODE_CONNECTED) {
				primary_connector = i;
				return connector;
			}
			drmModeFreeConnector(connector);
		}
	}
	return NULL;
}

drmModeConnectorPtr hwc_context::fetch_connector2(uint32_t type)
{
	int i;

	if (!resources)
		return NULL;

	for (i = 0; i < resources->count_connectors; i++) {
		if (i == primary_connector) continue;
		drmModeConnectorPtr connector =
			connector = drmModeGetConnector(kms_fd,
				resources->connectors[i]);
		if (connector) {
			if (connector->connector_type == type &&
				connector->connection == DRM_MODE_CONNECTED)
				return connector;
			drmModeFreeConnector(connector);
		}
	}
	return NULL;
}

/*
 * Initialize KMS.
 */
int hwc_context::init_kms()
{
	drmModeConnectorPtr primary;
	drmModeConnectorPtr secondary;
	int i;

	int ret = drmSetClientCap(kms_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
	if (ret) {
		ALOGE("failed to set universal planes cap, %d", ret);
		return ret;
	}

	ret = drmSetClientCap(kms_fd, DRM_CLIENT_CAP_ATOMIC, 1);
	if (ret) {
		ALOGE("failed to set atomic cap, %d", ret);
		return ret;
	}

	resources = drmModeGetResources(kms_fd);
	if (!resources) {
		ALOGE("failed to get modeset resources");
		return -EINVAL;
	}

	plane_resources = drmModeGetPlaneResources(kms_fd);
	if (!plane_resources) {
		ALOGE("failed to get plane resources");
		return -EINVAL;
	}

	/* find the crtc/connector/mode to use */
	primary = fetch_connector(DRM_MODE_CONNECTOR_HDMIA);
	if (primary) {
		init_with_connector(&primary_output, primary);
		drmModeFreeConnector(primary);
		primary_output.active = 1;
		secondary = fetch_connector2(DRM_MODE_CONNECTOR_HDMIA);
		if (secondary) {
			init_with_connector(&secondary_output, secondary);
			drmModeFreeConnector(secondary);
			secondary_output.active = 1;
		}
	}

	/* if still no connector, find first connected connector and try it */
	int lastValidConnectorIndex = -1;
	if (!primary_output.active) {

		for (i = 0; i < resources->count_connectors; i++) {
			drmModeConnectorPtr connector;

			connector = drmModeGetConnector(kms_fd,
					resources->connectors[i]);
			if (connector) {
				lastValidConnectorIndex = i;
				if (connector->connection == DRM_MODE_CONNECTED) {
					if (!init_with_connector(
							&primary_output, connector))
						break;
				}

				drmModeFreeConnector(connector);
			}
		}

		/* if no connected connector found, try to enforce the use of the last valid one */
		if (i == resources->count_connectors) {
			if (lastValidConnectorIndex > -1) {
				ALOGD("no connected connector found, enforcing the use of valid connector %d", lastValidConnectorIndex);
				drmModeConnectorPtr connector = drmModeGetConnector(kms_fd, resources->connectors[lastValidConnectorIndex]);
				init_with_connector(&primary_output, connector);
				drmModeFreeConnector(connector);
			}
			else {
				ALOGE("failed to find a valid crtc/connector/mode combination");
				drmModeFreeResources(resources);
				resources = NULL;

				return -EINVAL;
			}
		}
	}

	first_post = 1;
	first_post2 = 1;
	return 0;
}

hwc_context::hwc_context() {
    char path[PROPERTY_VALUE_MAX];
    property_get("gralloc.drm.kms", path, "/dev/dri/card0");

    fps = 60.0;
    kms_fd = open(path, O_RDWR|O_CLOEXEC);
   	if (kms_fd > 0) {
   		int error = init_kms();
   	    if (error != 0) {
   	        ALOGE("failed hwc_init_kms() %d", error);
   	    } else {
   	        width = (uint32_t)primary_output.mode.hdisplay;
   	        height = (uint32_t)primary_output.mode.vdisplay;
   	        fps = (float)primary_output.mode.vrefresh;
                format = HAL_PIXEL_FORMAT_RGBA_8888;
   	        xdpi = (float)primary_output.xdpi;
   	        ydpi = (float)primary_output.ydpi;
   	    }
    } else {
        ALOGE("hwc_context() failed to open %s", path);
    }
}

} // namespace aidl::android::hardware::graphics::composer3::impl

