/*
 * Copyright 2023 Android-RPi Project
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

#define LOG_TAG "composer-ComposerHal"
//#define LOG_NDEBUG 0
#include <android-base/logging.h>
#include <utils/Log.h>

#include "ComposerHal.h"

#include <aidl/android/hardware/graphics/composer3/IComposerCallback.h>
#include <android-base/logging.h>
#include <hardware/hwcomposer2.h>

#include "impl/TranslateHwcAidl.h"

namespace aidl::android::hardware::graphics::composer3::impl {

std::unique_ptr<IComposerHal> IComposerHal::create() {
    auto device = std::make_unique<Hwc2Device>();
    if (!device) {
        return nullptr;
    }
    return std::make_unique<ComposerHal>(std::move(device));
}

ComposerHal::ComposerHal(std::unique_ptr<Hwc2Device> device) : mDevice(std::move(device))  {
}

void ComposerHal::dumpDebugInfo(std::string */*output*/) {
    return;
}

void ComposerHal::registerEventCallback(ComposerHal::EventCallback* callback) {
    mMustValidateDisplay = true;
    mEventCallback = callback;

    mDevice->registerCallback(HWC2_CALLBACK_HOTPLUG, this,
                               reinterpret_cast<hwc2_function_pointer_t>(hotplugHook));
    mDevice->registerCallback(HWC2_CALLBACK_VSYNC, this,
                               reinterpret_cast<hwc2_function_pointer_t>(vsyncHook));
}

void ComposerHal::unregisterEventCallback() {
    mDevice->registerCallback(HWC2_CALLBACK_HOTPLUG, this, nullptr);
    mDevice->registerCallback(HWC2_CALLBACK_VSYNC, this, nullptr);

    mEventCallback = nullptr;
}

int32_t ComposerHal::createLayer(int64_t display, int64_t* outLayer) {
  int32_t err = mDevice->createLayer(display, (hwc2_layer_t *)outLayer);
    return err;
}

int32_t ComposerHal::destroyLayer(int64_t display, int64_t layer) {
    int32_t err = mDevice->destroyLayer(display, layer);
    return static_cast<int32_t>(err);
}

int32_t ComposerHal::getDisplayAttribute(int64_t display, int32_t config,
		DisplayAttribute attribute, int32_t* outValue) {
    int32_t err = mDevice->getDisplayAttribute(display, config,
                                       static_cast<int32_t>(attribute), outValue);
    return static_cast<int32_t>(err);
}

  int32_t ComposerHal::getDisplayName(int64_t display, std::string* outName) {
    uint32_t count = 0;
    int32_t err = mDevice->getDisplayName(display, &count, nullptr);
    if (err != HWC2_ERROR_NONE) {
        return err;
    }

    std::vector<char> buf(count + 1);
    err = mDevice->getDisplayName(display, &count, buf.data());
    if (err != HWC2_ERROR_NONE) {
        return err;
    }
    buf.resize(count + 1);
    buf[count] = '\0';

    *outName = buf.data();

    return HWC2_ERROR_NONE;
}

int32_t ComposerHal::getDisplayVsyncPeriod(int64_t display, int32_t* outVsyncPeriod) {
    return mDevice->getDisplayAttribute(display, 0,
            HWC2_ATTRIBUTE_VSYNC_PERIOD, outVsyncPeriod);
}


int32_t ComposerHal::setVsyncEnabled(int64_t display, bool enabled) {
    int32_t err = mDevice->setVsyncEnabled(display, static_cast<int32_t>(enabled));
    return err;
}

int32_t ComposerHal::setClientTarget(int64_t display, buffer_handle_t target,
                                 const ndk::ScopedFileDescriptor& fence,
                                 common::Dataspace dataspace,
   			     const std::vector<common::Rect>& /*damage*/) {

    int32_t hwcFence;
    int32_t hwcDataspace;
    a2h::translate(fence, hwcFence);
    a2h::translate(dataspace, hwcDataspace);
    
    int32_t err =
        mDevice->setClientTarget(display, target, hwcFence, hwcDataspace);
    return err;
}

int32_t ComposerHal::validateDisplay(int64_t display, std::vector<int64_t>* outChangedLayers,
                                 std::vector<Composition>* outCompositionTypes,
                                 uint32_t* outDisplayRequestMask,
                                 std::vector<int64_t>* outRequestedLayers,
   			     std::vector<int32_t>* /*outRequestMasks*/,
				 ClientTargetProperty* /*outClientTargetProperty*/,
				 DimmingStage* /*outDimmingStage*/) {
    uint32_t typesCount = 0;
    uint32_t reqsCount = 0;
    int32_t err = mDevice->validateDisplay(display, &typesCount, &reqsCount);
    mMustValidateDisplay = false;

    if (err != HWC2_ERROR_NONE && err != HWC2_ERROR_HAS_CHANGES) {
        return err;
    }

    err = mDevice->getChangedCompositionTypes(display, &typesCount, nullptr, nullptr);
    if (err != HWC2_ERROR_NONE) {
        return err;
    }

    std::vector<hwc2_layer_t> changedLayers(typesCount);
    std::vector<int32_t> compositionTypes(typesCount);
    err = mDevice->getChangedCompositionTypes(display, &typesCount, changedLayers.data(),
                    compositionTypes.data());
    if (err != HWC2_ERROR_NONE) {
        return err;
    }

    int32_t displayReqs = 0;
    std::vector<int64_t> requestedLayers(0);
    std::vector<uint32_t> requestMasks(0);

    h2a::translate(changedLayers, *outChangedLayers);
    h2a::translate(compositionTypes, *outCompositionTypes);
    *outDisplayRequestMask = displayReqs;
    *outRequestedLayers = std::move(requestedLayers);
    //*outRequestMasks = std::move(requestMasks);

    return err;
}

int32_t ComposerHal::presentDisplay(int64_t display, ndk::ScopedFileDescriptor& outPresentFence,
                       std::vector<int64_t>* outLayers,
                       std::vector<ndk::ScopedFileDescriptor>* outReleaseFences) {
    if (mMustValidateDisplay) {
        return HWC2_ERROR_NOT_VALIDATED;
    }

    int32_t hwcFence = -1;    
    int32_t err = mDevice->presentDisplay(display, &hwcFence);
    if (err != HWC2_ERROR_NONE) {
        return err;
    }

    h2a::translate(hwcFence, outPresentFence);    
    outLayers->resize(0);
    std::vector<int32_t> hwcFences(0);
    h2a::translate(hwcFences, *outReleaseFences);

    return HWC2_ERROR_NONE;
}

int32_t ComposerHal::acceptDisplayChanges(int64_t display) {
    int32_t err = mDevice->acceptDisplayChanges(display);
    return err;
}

int32_t ComposerHal::setLayerCompositionType(int64_t display, int64_t layer, Composition type) {
    int32_t hwcType;
    a2h::translate(type, hwcType);

    int32_t err = mDevice->setLayerCompositionType(display, layer, hwcType);
    return err;
}

} // namespace aidl::android::hardware::graphics::composer3::impl
