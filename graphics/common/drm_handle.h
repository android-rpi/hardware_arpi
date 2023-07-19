/*
 * Copyright (C) 2022 Android-RPi Project
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
#include <sys/errno.h>

struct private_handle_t : public native_handle {

    // file-descriptors
    int     prime_fd;

    uint32_t magic; /* differentiate between allocator impls */

    uint32_t width; /* width of buffer in pixels */
    uint32_t height; /* height of buffer in pixels */
    uint32_t format; /* pixel format (Android) */
    uint32_t stride; /* the stride in bytes */
    uint32_t fb_id;

    uint64_t usage __attribute__((aligned(8))); /* gralloc1 usage flags */

    static inline int sNumInts() {
        return (((sizeof(private_handle_t) - sizeof(native_handle_t))/sizeof(int)) - sNumFds);
    }
    static const int sNumFds = 1;
    static const int sMagic = 0x3141592;

    private_handle_t(int fd) :
        prime_fd(fd), magic(sMagic),
        fb_id(0)
    {
        version = sizeof(native_handle);
        numInts = sNumInts();
        numFds = sNumFds;
    }


    private_handle_t(int32_t width, int32_t height, int32_t hal_format,
		     int64_t usage) :
  	prime_fd(-1), magic(sMagic), width(width), height(height), format(hal_format),
        fb_id(0), usage(usage)
    {
        version = sizeof(native_handle);
        numInts = sNumInts();
        numFds = sNumFds;
    }

    ~private_handle_t() {
        magic = 0;
    }

    static int validate(const native_handle* h) {
        const private_handle_t* hnd = (const private_handle_t*)h;
        if (!h || h->version != sizeof(native_handle) ||
                h->numInts != sNumInts() || h->numFds != sNumFds ||
                hnd->magic != sMagic)
        {
            ALOGE("invalid gralloc handle (at %p)", h);
            return -EINVAL;
        }
        return 0;
    }

};

static inline struct private_handle_t *private_handle(buffer_handle_t handle)
{
    return (struct private_handle_t *)handle;
}
