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

#define LOG_TAG "allocator-service"
#include <android/binder_ibinder_platform.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <android/binder_status.h>
#include <log/log.h>

#include "Allocator.h"

using namespace android;

using arpi::allocator::Allocator;

int main() {
    auto service = ndk::SharedRefBase::make<Allocator>();
    auto binder = service->asBinder();

    AIBinder_setMinSchedulerPolicy(binder.get(), SCHED_NORMAL, -20);

    const auto instance = std::string() + Allocator::descriptor + "/default";

    auto status = AServiceManager_addService(binder.get(), instance.c_str());
    if (status != STATUS_OK) {
        ALOGE("Failed to start AIDL gralloc allocator service");
        return -EINVAL;
    }

    ABinderProcess_setThreadPoolMaxThreadCount(4);
    ABinderProcess_startThreadPool();
    ABinderProcess_joinThreadPool();

    return EXIT_FAILURE; // Unreachable
}
