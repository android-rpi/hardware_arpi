/*
 * Copyright (C) 2010-2011 Chia-I Wu <olvaffe@gmail.com>
 * Copyright (C) 2010-2011 LunarG Inc.
 * Copyright (C) 2016 Linaro, Ltd., Rob Herring <robh@kernel.org>
 * Copyright (C) 2022-2023 Android-RPi Project
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

#define LOG_TAG "GRALLOC-GBM"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include <cutils/properties.h>
#include <sys/errno.h>

#include <hardware/gralloc1.h>

#include <gbm.h>

#include "gbm_gralloc.h"
#include "drm_handle.h"

#include <unordered_map>

static std::unordered_map<buffer_handle_t, struct gbm_bo *> gbm_bo_handle_map;

struct bo_data_t {
	void *map_data;
	int lock_count;
	int locked_for;
};

void gralloc_gbm_destroy_user_data(struct gbm_bo *bo, void *data)
{
	struct bo_data_t *bo_data = (struct bo_data_t *)data;
	delete bo_data;

	(void)bo;
}

static struct bo_data_t *gbm_bo_data(struct gbm_bo *bo) {
	return (struct bo_data_t *)gbm_bo_get_user_data(bo);
}


static uint32_t get_gbm_format(int format)
{
	uint32_t fmt;

	switch (format) {
	case HAL_PIXEL_FORMAT_RGBA_8888:
		fmt = GBM_FORMAT_ABGR8888;
		break;
	case HAL_PIXEL_FORMAT_RGBX_8888:
		fmt = GBM_FORMAT_XBGR8888;
		break;
	case HAL_PIXEL_FORMAT_RGB_565:
		fmt = GBM_FORMAT_RGB565;
		break;
	case HAL_PIXEL_FORMAT_BGRA_8888:
		fmt = GBM_FORMAT_ARGB8888;
		break;
	case HAL_PIXEL_FORMAT_YV12:
	case HAL_PIXEL_FORMAT_YCBCR_420_888:
	case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
		/* YV12 is planar, but must be a single buffer so ask for GR88 */
		fmt = GBM_FORMAT_GR88;
		break;
	case HAL_PIXEL_FORMAT_BLOB:
		fmt = GBM_FORMAT_R8;
		break;
	case HAL_PIXEL_FORMAT_YCbCr_422_SP:
	case HAL_PIXEL_FORMAT_YCrCb_420_SP:
	default:
		fmt = 0;
		break;
	}

	return fmt;
}

int gralloc_gbm_get_bpp(int format)
{
	int bpp;

	switch (format) {
	case HAL_PIXEL_FORMAT_RGBA_8888:
	case HAL_PIXEL_FORMAT_RGBX_8888:
	case HAL_PIXEL_FORMAT_BGRA_8888:
		bpp = 4;
		break;
	case HAL_PIXEL_FORMAT_RGB_565:
	case HAL_PIXEL_FORMAT_YCbCr_422_I:
	case HAL_PIXEL_FORMAT_YCBCR_420_888:
	case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
		bpp = 2;
		break;
	/* planar; only Y is considered */
	case HAL_PIXEL_FORMAT_YV12:
	case HAL_PIXEL_FORMAT_YCbCr_422_SP:
	case HAL_PIXEL_FORMAT_YCrCb_420_SP:
	case HAL_PIXEL_FORMAT_BLOB:
		bpp = 1;
		break;
	default:
		bpp = 0;
		break;
	}

	return bpp;
}

static unsigned int get_pipe_bind(uint64_t usage)
{
	unsigned int bind = 0;

	if (usage & (GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN
		     | GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN))
		bind |= GBM_BO_USE_LINEAR;
	if (usage & (GRALLOC1_PRODUCER_USAGE_GPU_RENDER_TARGET
		     | GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE))
		bind |= GBM_BO_USE_RENDERING;
	if (usage & GRALLOC1_CONSUMER_USAGE_CLIENT_TARGET)
		bind |= GBM_BO_USE_SCANOUT;
	if (usage & GRALLOC1_CONSUMER_USAGE_HWCOMPOSER)
		bind |= GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;

	return bind;
}

static struct gbm_bo *gbm_import(struct gbm_device *gbm,
		buffer_handle_t _handle)
{
	struct gbm_bo *bo;
	struct private_handle_t *handle = private_handle(_handle);
	struct gbm_import_fd_data data;

	int format = get_gbm_format(handle->format);
	if (handle->fd < 0)
		return NULL;

	memset(&data, 0, sizeof(data));
	data.width = handle->width;
	data.height = handle->height;
	data.format = format;
	/* Adjust the width and height for a GBM GR88 buffer */
	if (handle->format == HAL_PIXEL_FORMAT_YV12) {
		data.width /= 2;
		data.height += handle->height / 2;
	}

	data.fd = handle->fd;
	data.stride = handle->stride;
	bo = gbm_bo_import(gbm, GBM_BO_IMPORT_FD, &data, 0);

	return bo;
}

static struct gbm_bo *gbm_alloc(struct gbm_device *gbm,
		buffer_handle_t _handle)
{
	struct gbm_bo *bo;
	struct private_handle_t *handle = private_handle(_handle);
	int format = get_gbm_format(handle->format);
	int gbm_usage = get_pipe_bind(handle->usage);
	int width, height;

	width = handle->width;
	height = handle->height;

	/*
	 * For YV12, we request GR88, so halve the width since we're getting
	 * 16bpp. Then increase the height by 1.5 for the U and V planes.
	 */
	if (handle->format == HAL_PIXEL_FORMAT_YV12) {
		width /= 2;
		height += handle->height / 2;
	}

	ALOGV("create BO, size=%dx%d, fmt=%d, gbm_usage=%x",
	      handle->width, handle->height, handle->format, gbm_usage);
	bo = gbm_bo_create(gbm, width, height, format, gbm_usage);
	if (!bo) {
		ALOGE("failed to create BO, size=%dx%d, fmt=%d, gbm_usage=%x",
		      handle->width, handle->height, handle->format, gbm_usage);
		return NULL;
	}

	handle->fd = gbm_bo_get_fd(bo);
	handle->stride = gbm_bo_get_stride(bo);

	return bo;
}

void gbm_free(buffer_handle_t handle)
{
	struct gbm_bo *bo = gralloc_gbm_bo_from_handle(handle);

	if (!bo)
		return;

	gbm_bo_handle_map.erase(handle);
	gbm_bo_destroy(bo);
}

/*
 * Return the bo of a registered handle.
 */
struct gbm_bo *gralloc_gbm_bo_from_handle(buffer_handle_t handle)
{
	return gbm_bo_handle_map[handle];
}

static int gbm_map(buffer_handle_t handle, int x, int y, int w, int h,
		int enable_write, void **addr)
{
	int err = 0;
	int flags = GBM_BO_TRANSFER_READ;
	struct private_handle_t *gbm_handle = private_handle(handle);
	struct gbm_bo *bo = gralloc_gbm_bo_from_handle(handle);
	struct bo_data_t *bo_data = gbm_bo_data(bo);
	uint32_t stride;

	if (bo_data->map_data)
		return -EINVAL;

	if (gbm_handle->format == HAL_PIXEL_FORMAT_YV12) {
		if (x || y)
			ALOGE("can't map with offset for planar %p", bo);
		w /= 2;
		h += h / 2;
	}

	if (enable_write)
		flags |= GBM_BO_TRANSFER_WRITE;

	*addr = gbm_bo_map(bo, 0, 0, x + w, y + h, flags, &stride, &bo_data->map_data);
	ALOGV("mapped bo %p (%d, %d)-(%d, %d) at %p", bo, x, y, w, h, *addr);
	if (*addr == NULL)
		return -ENOMEM;

	assert(stride == gbm_bo_get_stride(bo));

	return err;
}

static void gbm_unmap(struct gbm_bo *bo)
{
	struct bo_data_t *bo_data = gbm_bo_data(bo);

	gbm_bo_unmap(bo, bo_data->map_data);
	bo_data->map_data = NULL;
}

/*
 * Register a buffer handle.
 */
int gbm_register(struct gbm_device *gbm, buffer_handle_t _handle)
{
	struct gbm_bo *bo;

	if (!_handle)
		return -EINVAL;

	if (gbm_bo_handle_map.count(_handle))
		return -EINVAL;

	bo = gbm_import(gbm, _handle);
	if (!bo)
		return -EINVAL;

	gbm_bo_handle_map.emplace(_handle, bo);

	return 0;
}

/*
 * Unregister a buffer handle.  It is no-op for handles created locally.
 */
int gbm_unregister(buffer_handle_t handle)
{
	gbm_free(handle);
	return 0;
}

/*
 * Create a bo.
 */
buffer_handle_t gralloc_gbm_bo_create(struct gbm_device *gbm,
		int width, int height, int format, uint64_t usage, int *stride)
{
	struct gbm_bo *bo;
	native_handle_t *handle;

	handle = new private_handle_t(width, height, format, usage);
	if (!handle)
		return NULL;

	bo = gbm_alloc(gbm, handle);
	if (!bo) {
		native_handle_delete(handle);
		return NULL;
	}

	gbm_bo_handle_map.emplace(handle, bo);

	/* in pixels */
	*stride = private_handle(handle)->stride / gralloc_gbm_get_bpp(format);

	return handle;
}

/*
 * Lock a bo.  XXX thread-safety?
 */
int gbm_lock(buffer_handle_t handle,
		uint64_t usage, int x, int y, int w, int h,
		void **addr)
{
	struct private_handle_t *gbm_handle = private_handle(handle);
	struct gbm_bo *bo = gralloc_gbm_bo_from_handle(handle);
	struct bo_data_t *bo_data;

	if (!bo)
		return -EINVAL;

	if ((gbm_handle->usage & usage) != usage) {
		/* make FB special for testing software renderer with */

		if (!(gbm_handle->usage & GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN) &&
				!(gbm_handle->usage & GRALLOC1_CONSUMER_USAGE_CLIENT_TARGET) &&
				!(gbm_handle->usage & GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE)) {
			ALOGE("bo.usage:x%lx/usage:x%lx is not GRALLOC1_CONSUMER_USAGE_CLIENT_TARGET or GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE",
				gbm_handle->usage, usage);
			return -EINVAL;
		}
	}

	bo_data = gbm_bo_data(bo);
	if (!bo_data) {
		bo_data = new struct bo_data_t();
		gbm_bo_set_user_data(bo, bo_data, gralloc_gbm_destroy_user_data);
	}

	ALOGV("lock bo %p, cnt=%d, usage=%lx", bo, bo_data->lock_count, usage);

	/* allow multiple locks with compatible usages */
	if (bo_data->lock_count && (bo_data->locked_for & usage) != usage)
		return -EINVAL;

	usage |= bo_data->locked_for;

	/*
	 * Some users will lock with an null crop rect.
	 * Interpret this as no-crop (full buffer WxH).
	 */
	if (w == 0 && h == 0) {
		w = gbm_handle->width;
		h = gbm_handle->height;
	}

	if (usage & (GRALLOC1_PRODUCER_USAGE_CPU_WRITE | GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN |
		     GRALLOC1_CONSUMER_USAGE_CPU_READ | GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN)) {
		/* the driver is supposed to wait for the bo */
	  int write = !!(usage & (GRALLOC1_PRODUCER_USAGE_CPU_WRITE |
				  GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN));
		int err = gbm_map(handle, x, y, w, h, write, addr);
		if (err)
			return err;
	}
	else {
		/* kernel handles the synchronization here */
	}

	bo_data->lock_count++;
	bo_data->locked_for |= usage;

	return 0;
}

/*
 * Unlock a bo.
 */
int gbm_unlock(buffer_handle_t handle)
{
	struct gbm_bo *bo = gralloc_gbm_bo_from_handle(handle);
	struct bo_data_t *bo_data;
	if (!bo)
		return -EINVAL;

	bo_data = gbm_bo_data(bo);

	int mapped = bo_data->locked_for &
	      (GRALLOC1_PRODUCER_USAGE_CPU_WRITE | GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN |
	       GRALLOC1_CONSUMER_USAGE_CPU_READ | GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN);

	if (!bo_data->lock_count)
		return 0;

	if (mapped)
		gbm_unmap(bo);

	bo_data->lock_count--;
	if (!bo_data->lock_count)
		bo_data->locked_for = 0;

	return 0;
}

#define GRALLOC_ALIGN(value, base) (((value) + ((base)-1)) & ~((base)-1))

int gbm_lock_ycbcr(buffer_handle_t handle,
		uint64_t usage, int x, int y, int w, int h,
		struct android_ycbcr *ycbcr)
{
	struct private_handle_t *hnd = private_handle(handle);
	int ystride, cstride;
	void *addr = 0;
	int err;

	ALOGV("handle %p, hnd %p, usage 0x%lx", handle, hnd, usage);

	err = gbm_lock(handle, usage, x, y, w, h, &addr);
	if (err)
		return err;

	memset(ycbcr->reserved, 0, sizeof(ycbcr->reserved));

	switch (hnd->format) {
	case HAL_PIXEL_FORMAT_YCbCr_420_888:
	case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
		ystride = cstride = GRALLOC_ALIGN(hnd->width, 16);
		ycbcr->y = addr;
		ycbcr->cb = (unsigned char *)addr + ystride * hnd->height;
		ycbcr->cr = (unsigned char *)ycbcr->cb + 1;
		ycbcr->ystride = ystride;
		ycbcr->cstride = cstride;
		ycbcr->chroma_step = 2;
		break;
	case HAL_PIXEL_FORMAT_YCrCb_420_SP:
		ystride = cstride = GRALLOC_ALIGN(hnd->width, 16);
		ycbcr->y = addr;
		ycbcr->cr = (unsigned char *)addr + ystride * hnd->height;
		ycbcr->cb = (unsigned char *)addr + ystride * hnd->height + 1;
		ycbcr->ystride = ystride;
		ycbcr->cstride = cstride;
		ycbcr->chroma_step = 2;
		break;
	case HAL_PIXEL_FORMAT_YV12:
		ystride = hnd->width;
		cstride = GRALLOC_ALIGN(ystride / 2, 16);
		ycbcr->y = addr;
		ycbcr->cr = (unsigned char *)addr + ystride * hnd->height;
		ycbcr->cb = (unsigned char *)addr + ystride * hnd->height + cstride * hnd->height / 2;
		ycbcr->ystride = ystride;
		ycbcr->cstride = cstride;
		ycbcr->chroma_step = 1;
		break;
	default:
		ALOGE("Can not lock buffer, invalid format: 0x%x", hnd->format);
		return -EINVAL;
	}

	return 0;
}



struct gbm_device *gbm_init(void)
{
	struct gbm_device *gbm;
	char path[PROPERTY_VALUE_MAX];
	int fd;

	property_get("gralloc.drm.kms", path, "/dev/dri/card0");
	fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		ALOGE("failed to open %s", path);
		return nullptr;
	}

	gbm = gbm_create_device(fd);
	if (!gbm) {
		ALOGE("failed to create gbm device");
		close(fd);
		return nullptr;
	}

	return gbm;
}

void gbm_destroy(struct gbm_device *gbm)
{
	int fd = gbm_device_get_fd(gbm);

	gbm_device_destroy(gbm);
	close(fd);
}

int gbm_alloc(struct gbm_device *gbm, int w, int h, int format, uint64_t usage,
		buffer_handle_t *handle, int *stride) {
    int err = 0;
    int bpp = gralloc_gbm_get_bpp(format);
    if (!bpp) return -EINVAL;

    *handle = gralloc_gbm_bo_create(gbm, w, h, format, usage, stride);
    if (!*handle)
        err = -errno;

    ALOGV("buffer %p usage = %016lx", *handle, usage);
    return err;
}

