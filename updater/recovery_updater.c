/*
 * Copyright (C) 2009 The Android Open Source Project
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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "edify/expr.h"
#include "firmware.h"
#include "mincrypt/sha.h"
#include "minzip/Zip.h"
#include "mounts.h"
#include "updater/updater.h"

Value* UpdateFn(const char* name, State* state, int argc, Expr* argv[]) {
    if (argc != 7) {
        return ErrorAbort(state, "%s() expects 7 args, got %d", name, argc);
    }

    char* type = strrchr(name, '_');
    if (type == NULL || *(type+1) == '\0') {
        return ErrorAbort(state, "%s() couldn't get type from function name",
                          name);
    }
    ++type;

    Value* image;
    Value* width_string;
    Value* height_string;
    Value* bpp_string;
    Value* busy;
    Value* fail;
    Value* expected_sha1_string;
    if (ReadValueArgs(state, argv, 7, &image,
                      &width_string, &height_string, &bpp_string,
                      &busy, &fail, &expected_sha1_string) < 0) {
        return NULL;
    }

    // close the package
    ZipArchive* za = ((UpdaterInfo*)(state->cookie))->package_zip;
    mzCloseZipArchive(za);
    ((UpdaterInfo*)(state->cookie))->package_zip = NULL;

    // Try to unmount /cache.  If we fail (because we're running in an
    // older recovery that still has the package file open), try to
    // remount it read-only.  If that fails, abort.
    sync();
    scan_mounted_volumes();
    MountedVolume* vol = find_mounted_volume_by_mount_point("/cache");
    int result = unmount_mounted_volume(vol);
    if (result != 0) {
        printf("%s(): failed to unmount cache (%d: %s)\n",
               name, result, strerror(errno));

        result = remount_read_only(vol);
        if (result != 0) {
            printf("%s(): failed to remount cache (%d: %s)\n",
                   name, result, strerror(errno));
            return StringValue(strdup(""));
        } else {
            printf("%s(): remounted cache\n", name);
        }
        sync();
    } else {
        printf("%s(): unmounted cache\n", name);
    }

    int width = 0, height = 0, bpp = 0;

    if (width_string->type != VAL_STRING ||
        (width = strtol(width_string->data, NULL, 10)) == 0) {
        printf("%s(): bad width argument", name);
    }
    if (height_string->type != VAL_STRING ||
        (height = strtol(height_string->data, NULL, 10)) == 0) {
        printf("%s(): bad height argument", name);
    }
    if (bpp_string->type != VAL_STRING ||
        (bpp = strtol(bpp_string->data, NULL, 10)) == 0) {
        printf("%s(): bad bpp argument", name);
    }

    if (image->type != VAL_BLOB) {
        printf("image argument is not blob (is type %d)\n", image->type);
        goto done;
    }

    uint8_t expected_sha1[SHA_DIGEST_SIZE];
    char* data = expected_sha1_string->data;
    if (expected_sha1_string->type != VAL_STRING ||
        strlen(data) != SHA_DIGEST_SIZE*2) {
        printf("%s(): bad expected_sha1 argument", name);
        goto done;
    }
    printf("expected sha1 is: ");
    int i;
    for (i = 0; i < SHA_DIGEST_SIZE; ++i) {
        char temp = data[i*2+2];
        data[i*2+2] = '\0';
        expected_sha1[i] = strtol(data+i*2, NULL, 16);
        data[i*2+2] = temp;
        printf("%02x", expected_sha1[i]);
    }
    printf("\n");

    install_firmware_update(
        type,
        image->data,
        image->size,
        width, height, bpp,
        busy->size > 0 ? busy->data : NULL,
        fail->size > 0 ? fail->data : NULL,
        "/tmp/recovery.log",
        expected_sha1);
    printf("%s: install_firmware_update returned!\n", name);

  done:
    FreeValue(image);
    FreeValue(width_string);
    FreeValue(height_string);
    FreeValue(bpp_string);
    FreeValue(busy);
    FreeValue(fail);
    // install_firmware_update should reboot.  If it returns, it failed.
    return StringValue(strdup(""));
}

void Register_librecovery_updater_htc() {
    fprintf(stderr, "installing HTC updater extensions\n");

    RegisterFunction("htc.install_radio", UpdateFn);
    RegisterFunction("htc.install_hboot", UpdateFn);
}
