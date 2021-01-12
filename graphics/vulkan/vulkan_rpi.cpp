/*
 * Copyright 2021 Android-RPi Project
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

#include <stdlib.h>
#include <hardware/hwvulkan.h>
extern "C" {
#include "v3dv_entrypoints.h"
}

static int v3dv_hal_close(struct hw_device_t *) {
   return -1;
}

static int v3dv_hal_open(const struct hw_module_t *mod, const char*, struct hw_device_t** dev) {
   hwvulkan_device_t *hal_dev = (hwvulkan_device_t *)malloc(sizeof(*hal_dev));
   if (!hal_dev)
      return -1;

   *hal_dev = (hwvulkan_device_t) {
      .common = {
         .tag = HARDWARE_DEVICE_TAG,
         .version = HWVULKAN_DEVICE_API_VERSION_0_1,
         .module = (struct hw_module_t *)mod,
         .close = v3dv_hal_close,
      },
     .EnumerateInstanceExtensionProperties = v3dv_EnumerateInstanceExtensionProperties,
     .CreateInstance = v3dv_CreateInstance,
     .GetInstanceProcAddr = v3dv_GetInstanceProcAddr,
   };

   *dev = &hal_dev->common;
   return 0;
}

static struct hw_module_methods_t v3dv_module_methods = {
      .open = v3dv_hal_open
};

struct hwvulkan_module_t HAL_MODULE_INFO_SYM = {
   .common = {
      .tag = HARDWARE_MODULE_TAG,
      .module_api_version = HWVULKAN_MODULE_API_VERSION_0_1,
      .hal_api_version = HARDWARE_MAKE_API_VERSION(1, 0),
      .id = HWVULKAN_HARDWARE_MODULE_ID,
      .name = "arpi vulkan",
      .author = "Android-Rpi",
      .methods = &v3dv_module_methods,
   },
};
