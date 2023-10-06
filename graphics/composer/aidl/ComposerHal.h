/*
 * Copyright 2023 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <memory>
#include <unordered_set>

#include "include/IComposerHal.h"

#include "Hwc2Device.h"

namespace aidl::android::hardware::graphics::composer3::impl {

class ComposerHal : public IComposerHal {
  public:
    ComposerHal(std::unique_ptr<Hwc2Device> device);
    virtual ~ComposerHal() = default;

    void dumpDebugInfo(std::string *output) override;

    void registerEventCallback(EventCallback* callback);
    void unregisterEventCallback();

    int32_t createLayer(int64_t display, int64_t* outLayer);
    int32_t destroyLayer(int64_t display, int64_t layer);
    int32_t getDisplayAttribute(int64_t display, int32_t config,
                              DisplayAttribute attribute, int32_t* outValue) override;
    int32_t getDisplayName(int64_t display, std::string* outName)override ;
    int32_t getDisplayVsyncPeriod(int64_t display, int32_t* outVsyncPeriod) override;
    int32_t setVsyncEnabled(int64_t display, bool enabled);
    int32_t setClientTarget(int64_t display, buffer_handle_t target,
                            const ndk::ScopedFileDescriptor& fence, common::Dataspace dataspace,
                            const std::vector<common::Rect>& damage) override;  
    int32_t validateDisplay(int64_t display, std::vector<int64_t>* outChangedLayers,
                            std::vector<Composition>* outCompositionTypes,
                            uint32_t* outDisplayRequestMask,
                            std::vector<int64_t>* outRequestedLayers,
                            std::vector<int32_t>* outRequestMasks,
                            ClientTargetProperty* outClientTargetProperty,
                            DimmingStage* outDimmingStage) override;
    int32_t presentDisplay(int64_t display, ndk::ScopedFileDescriptor& outPresentFence,
                           std::vector<int64_t>* outLayers,
                           std::vector<ndk::ScopedFileDescriptor>* outReleaseFences) override;
  
    int32_t acceptDisplayChanges(int64_t display);

    int32_t setLayerCompositionType(int64_t display, int64_t layer, Composition type) override;

  private:

    static void hotplugHook(hwc2_callback_data_t callbackData, hwc2_display_t display,
                            int32_t connected) {
        auto hal = static_cast<ComposerHal*>(callbackData);
        hal->mEventCallback->onHotplug(display,
                                       connected == HWC2_CONNECTION_CONNECTED);
    }

    static void vsyncHook(hwc2_callback_data_t callbackData, hwc2_display_t display,
                          int64_t timestamp, hwc2_vsync_period_t vsyncPeriodNanos) {
        auto hal = static_cast<ComposerHal*>(callbackData);
        hal->mEventCallback->onVsync(display, timestamp, vsyncPeriodNanos);
    }

    std::unique_ptr<Hwc2Device> mDevice;

    std::unordered_set<hwc2_capability_t> mCapabilities;

    EventCallback* mEventCallback = nullptr;
    std::atomic<bool> mMustValidateDisplay{true};
};

} // namespace aidl::android::hardware::graphics::composer3::impl
