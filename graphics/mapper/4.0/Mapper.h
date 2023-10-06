/*
 * Copyright 2023 Android-RPi Project
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

namespace android {
namespace hardware {
namespace graphics {
namespace mapper {
namespace V4_0 {
namespace implementation {

using common::V1_0::BufferUsage;
using common::V1_2::PixelFormat;

class Mapper : public IMapper {
  public:
    Mapper();
    ~Mapper();

    Return<void> createDescriptor(const IMapper::BufferDescriptorInfo& descriptorInfo,
        createDescriptor_cb hidl_cb) override;

    Return<void> importBuffer(const hidl_handle& rawHandle,
		IMapper::importBuffer_cb hidl_cb) override;

    Return<Error> freeBuffer(void* buffer) override;

    Return<void> lock(void* buffer, uint64_t cpuUsage, const IMapper::Rect& accessRegion,
                  const hidl_handle& acquireFence, IMapper::lock_cb hidl_cb) override;

    Return<void> unlock(void* buffer, IMapper::unlock_cb hidl_cb) override;

    Return<Error> validateBufferSize(void* buffer, const BufferDescriptorInfo& descriptor,
                  uint32_t stride) override;

    Return<void> getTransportSize(void* buffer, getTransportSize_cb hidl_cb) override;

    Return<void> flushLockedBuffer(void* buffer,flushLockedBuffer_cb hidl_cb) override;

    Return<Error> rereadLockedBuffer(void* buffer) override;

    Return<void> isSupported(const BufferDescriptorInfo& descriptor, isSupported_cb hidl_cb) override;

    Return<void> get(void* buffer, const MetadataType& metadataType, get_cb hidl_cb) override;

    Return<Error> set(void* buffer, const MetadataType& metadataType,
            const hidl_vec<uint8_t>& metadata) override;

    Return<void> getFromBufferDescriptorInfo(const BufferDescriptorInfo& descriptor,
            const MetadataType& metadataType, getFromBufferDescriptorInfo_cb hidl_cb) override;

    Return<void> listSupportedMetadataTypes(listSupportedMetadataTypes_cb hidl_cb) override;

    Return<void> dumpBuffer(void* buffer, dumpBuffer_cb hidl_cb) override;
    Return<void> dumpBuffers(dumpBuffers_cb hidl_cb) override;

    Return<void> getReservedRegion(void* buffer, getReservedRegion_cb hidl_cb) override;


  private:
    struct gbm_device *gbmDevice;
};

extern "C" IMapper* HIDL_FETCH_IMapper(const char* name);

}  // namespace implementation
}  // namespace V4_0
}  // namespace mapper
}  // namespace graphics
}  // namespace hardware
}  // namespace android

