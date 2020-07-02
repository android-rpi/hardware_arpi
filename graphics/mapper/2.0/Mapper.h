/*
 * Copyright 2018 The Android Open Source Project
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

#include "Fence.h"
#include <android/hardware/graphics/mapper/2.0/IMapper.h>

namespace android {
namespace hardware {
namespace graphics {
namespace mapper {
namespace V2_0 {
namespace implementation {

using common::V1_0::BufferUsage;
using common::V1_0::PixelFormat;
using mapper::V2_0::passthrough::grallocEncodeBufferDescriptor;

class Mapper : public IMapper {
  public:
    Mapper();

    Return<void> createDescriptor(const IMapper::BufferDescriptorInfo& descriptorInfo,
        createDescriptor_cb hidl_cb) override;

    Return<void> importBuffer(const hidl_handle& rawHandle,
		IMapper::importBuffer_cb hidl_cb) override;

    Return<Error> freeBuffer(void* buffer) override;

    Return<void> lock(void* buffer, uint64_t cpuUsage, const IMapper::Rect& accessRegion,
                  const hidl_handle& acquireFence, IMapper::lock_cb hidl_cb) override;

    Return<void> lockYCbCr(void* buffer, uint64_t cpuUsage, const IMapper::Rect& accessRegion,
            const hidl_handle& acquireFence, IMapper::lockYCbCr_cb hidl_cb) override;

    Return<void> unlock(void* buffer, IMapper::unlock_cb hidl_cb) override;

  private:
    struct drm_module_t* mModule;
};

extern "C" IMapper* HIDL_FETCH_IMapper(const char* name);

}  // namespace implementation
}  // namespace V2_0
}  // namespace mapper
}  // namespace graphics
}  // namespace hardware
}  // namespace android

