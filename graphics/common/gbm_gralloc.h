/*
 * Copyright (C) 2010-2011 Chia-I Wu <olvaffe@gmail.com>
 * Copyright (C) 2010-2011 LunarG Inc.
 * Copyright (C) 2016 Linaro, Ltd., Rob Herring <robh@kernel.org>
 * Copyright (C) 2023 Android-RPi Project
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
#pragma once

#include <cutils/native_handle.h>

struct gbm_device;
struct gbm_bo;

int gralloc_gbm_get_bpp(int format);
struct gbm_bo *gralloc_gbm_bo_from_handle(buffer_handle_t handle);

struct gbm_device *gbm_init(void);
void gbm_destroy(struct gbm_device *gbm);

int gbm_alloc(struct gbm_device *gbm, int w, int h, int format, uint64_t usage,
	      buffer_handle_t *handle, int *stride);
void gbm_free(buffer_handle_t handle);

int gbm_register(struct gbm_device *gbm, buffer_handle_t handle);
int gbm_unregister(buffer_handle_t handle);

int gbm_lock(buffer_handle_t handle, uint64_t usage, int x, int y, int w, int h, void **ptr);
int gbm_lock_ycbcr(buffer_handle_t handle, uint64_t usage, int x, int y, int w, int h, struct android_ycbcr *ycbcr);
int gbm_unlock(buffer_handle_t handle);

