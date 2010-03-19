#
# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# This lists the aspects that are unique to HTC but shared between all
# HTC devices

# Sets an HTC-specific device-agnostic overlay
DEVICE_PACKAGE_OVERLAYS := device/htc/common/overlay

# Sets copy files for all HTC-specific device
PRODUCT_COPY_FILES += device/htc/common/ecclist_for_mcc.conf:system/etc/ecclist_for_mcc.conf

# Get additional product configuration from the non-open-source
# counterpart to this file, if it exists
$(call inherit-product-if-exists, vendor/htc/common/common-vendor.mk)

# Override - we don't want to use any inherited value
PRODUCT_MANUFACTURER := HTC
