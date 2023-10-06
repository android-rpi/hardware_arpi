/*
 * Copyright (C) 2021 The Android Open Source Project
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
/**
 * Idealy IComposerHal should use aidl NativeHandle not native_handle_t/buffer_handle_t.
 * However current patten is the HWC lib does not own buffer resources (HWC lib
 * does own the fences).
 */
#include <aidl/android/hardware/common/NativeHandle.h>
#include <aidl/android/hardware/graphics/common/BlendMode.h>
#include <aidl/android/hardware/graphics/common/ColorTransform.h>
#include <aidl/android/hardware/graphics/common/Dataspace.h>
#include <aidl/android/hardware/graphics/common/FRect.h>
#include <aidl/android/hardware/graphics/common/PixelFormat.h>
#include <aidl/android/hardware/graphics/common/Point.h>
#include <aidl/android/hardware/graphics/common/Rect.h>
#include <aidl/android/hardware/graphics/common/Transform.h>
#include <aidl/android/hardware/graphics/composer3/Buffer.h>
#include <aidl/android/hardware/graphics/composer3/Capability.h>
#include <aidl/android/hardware/graphics/composer3/ChangedCompositionTypes.h>
#include <aidl/android/hardware/graphics/composer3/ClientTarget.h>
#include <aidl/android/hardware/graphics/composer3/ClientTargetProperty.h>
#include <aidl/android/hardware/graphics/composer3/ClientTargetPropertyWithBrightness.h>
#include <aidl/android/hardware/graphics/composer3/Color.h>
#include <aidl/android/hardware/graphics/composer3/ColorMode.h>
#include <aidl/android/hardware/graphics/composer3/CommandError.h>
#include <aidl/android/hardware/graphics/composer3/CommandResultPayload.h>
#include <aidl/android/hardware/graphics/composer3/Composition.h>
#include <aidl/android/hardware/graphics/composer3/ContentType.h>
#include <aidl/android/hardware/graphics/composer3/DimmingStage.h>
#include <aidl/android/hardware/graphics/composer3/DisplayAttribute.h>
#include <aidl/android/hardware/graphics/composer3/DisplayBrightness.h>
#include <aidl/android/hardware/graphics/composer3/DisplayCapability.h>
#include <aidl/android/hardware/graphics/composer3/DisplayCommand.h>
#include <aidl/android/hardware/graphics/composer3/DisplayConnectionType.h>
#include <aidl/android/hardware/graphics/composer3/DisplayContentSample.h>
#include <aidl/android/hardware/graphics/composer3/DisplayContentSamplingAttributes.h>
#include <aidl/android/hardware/graphics/composer3/DisplayIdentification.h>
#include <aidl/android/hardware/graphics/composer3/DisplayRequest.h>
#include <aidl/android/hardware/graphics/composer3/FormatColorComponent.h>
#include <aidl/android/hardware/graphics/composer3/HdrCapabilities.h>
#include <aidl/android/hardware/graphics/composer3/LayerBrightness.h>
#include <aidl/android/hardware/graphics/composer3/LayerCommand.h>
#include <aidl/android/hardware/graphics/composer3/ParcelableBlendMode.h>
#include <aidl/android/hardware/graphics/composer3/ParcelableComposition.h>
#include <aidl/android/hardware/graphics/composer3/ParcelableDataspace.h>
#include <aidl/android/hardware/graphics/composer3/ParcelableTransform.h>
#include <aidl/android/hardware/graphics/composer3/PerFrameMetadata.h>
#include <aidl/android/hardware/graphics/composer3/PerFrameMetadataBlob.h>
#include <aidl/android/hardware/graphics/composer3/PerFrameMetadataKey.h>
#include <aidl/android/hardware/graphics/composer3/PlaneAlpha.h>
#include <aidl/android/hardware/graphics/composer3/PowerMode.h>
#include <aidl/android/hardware/graphics/composer3/PresentFence.h>
#include <aidl/android/hardware/graphics/composer3/PresentOrValidate.h>
#include <aidl/android/hardware/graphics/composer3/ReadbackBufferAttributes.h>
#include <aidl/android/hardware/graphics/composer3/ReleaseFences.h>
#include <aidl/android/hardware/graphics/composer3/RenderIntent.h>
#include <aidl/android/hardware/graphics/composer3/VirtualDisplay.h>
#include <aidl/android/hardware/graphics/composer3/VsyncPeriodChangeConstraints.h>
#include <aidl/android/hardware/graphics/composer3/VsyncPeriodChangeTimeline.h>
#include <aidl/android/hardware/graphics/composer3/ZOrder.h>
#include <cutils/native_handle.h>

// avoid naming conflict
using AidlPixelFormat = aidl::android::hardware::graphics::common::PixelFormat;
using AidlNativeHandle = aidl::android::hardware::common::NativeHandle;

namespace aidl::android::hardware::graphics::composer3::impl {

// Abstraction of ComposerHal. Returned error code is compatible with AIDL
// IComposerClient interface.
class IComposerHal {
 public:
    static std::unique_ptr<IComposerHal> create();
    virtual ~IComposerHal() = default;

    virtual void dumpDebugInfo(std::string* output) = 0;

    class EventCallback {
      public:
        virtual ~EventCallback() = default;
        virtual void onHotplug(int64_t display, bool connected) = 0;
        virtual void onRefresh(int64_t display) = 0;
        virtual void onVsync(int64_t display, int64_t timestamp, int32_t vsyncPeriodNanos) = 0;
        virtual void onVsyncPeriodTimingChanged(int64_t display,
                                                const VsyncPeriodChangeTimeline& timeline) = 0;
        virtual void onVsyncIdle(int64_t display) = 0;
        virtual void onSeamlessPossible(int64_t display) = 0;
    };
    virtual void registerEventCallback(EventCallback* callback) = 0;
    virtual void unregisterEventCallback() = 0;

    virtual int32_t acceptDisplayChanges(int64_t display) = 0;
    virtual int32_t createLayer(int64_t display, int64_t* outLayer) = 0;
    virtual int32_t destroyLayer(int64_t display, int64_t layer) = 0;
    virtual int32_t getDisplayAttribute(int64_t display, int32_t config,
                                      DisplayAttribute attribute, int32_t* outValue) = 0;

    virtual int32_t getDisplayName(int64_t display, std::string* outName) = 0;
    virtual int32_t getDisplayVsyncPeriod(int64_t display, int32_t* outVsyncPeriod) = 0;
    virtual int32_t presentDisplay(int64_t display, ndk::ScopedFileDescriptor& fence,
                                   std::vector<int64_t>* outLayers,
                                   std::vector<ndk::ScopedFileDescriptor>* outReleaseFences) = 0;
    virtual int32_t setClientTarget(int64_t display, buffer_handle_t target,
                                    const ndk::ScopedFileDescriptor& fence,
                                    common::Dataspace dataspace,
                                    const std::vector<common::Rect>& damage) = 0; // cmd
    virtual int32_t setLayerCompositionType(int64_t display, int64_t layer, Composition type) = 0;
    virtual int32_t setVsyncEnabled(int64_t display, bool enabled) = 0;
    virtual int32_t validateDisplay(int64_t display, std::vector<int64_t>* outChangedLayers,
                                    std::vector<Composition>* outCompositionTypes,
                                    uint32_t* outDisplayRequestMask,
                                    std::vector<int64_t>* outRequestedLayers,
                                    std::vector<int32_t>* outRequestMasks,
                                    ClientTargetProperty* outClientTargetProperty,
                                    DimmingStage* outDimmingStage) = 0;
};

} // namespace aidl::android::hardware::graphics::composer3::detail
