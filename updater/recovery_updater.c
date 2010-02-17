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

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "edify/expr.h"
#include "firmware.h"

char* UpdateFn(const char* name, State* state, int argc, Expr* argv[]) {
    if (argc != 6) {
        return ErrorAbort(state, "%s() expects 6 args, got %d", name, argc);
    }

    char* type = strrchr(name, '_');
    if (type == NULL || *(type+1) == '\0') {
        return ErrorAbort(state, "%s() couldn't get type from function name",
                          name);
    }
    ++type;

    char* image_string;
    char* width_string;
    char* height_string;
    char* bpp_string;
    char* busy_string;
    char* fail_string;
    if (ReadArgs(state, argv, 6, &image_string,
                 &width_string, &height_string, &bpp_string,
                 &busy_string, &fail_string) < 0) {
        return NULL;
    }

    int width = strtol(width_string, NULL, 10);
    int height = strtol(height_string, NULL, 10);
    int bpp = strtol(bpp_string, NULL, 10);

    long image_size = *(long *)image_string;
    if (image_size < 0) {
        printf("image argument is missing data (length %ld)\n", image_size);
        goto done;
    }
    char* image_data = image_string + sizeof(long);

    long busy_size = *(long *)busy_string;
    char* busy_data;
    if (busy_size > 0) {
        busy_data = busy_string + sizeof(long);
    } else {
        busy_data = NULL;
    }

    long fail_size = *(long *)fail_string;
    char* fail_data;
    if (fail_size > 0) {
        fail_data = fail_string + sizeof(long);
    } else {
        fail_data = NULL;
    }

    install_firmware_update(type, image_data, image_size,
                            width, height, bpp,
                            busy_data, fail_data,
                            "/tmp/recovery.log");
    printf("%s: install_firmware_update returned!\n", name);

  done:
    free(image_string);
    free(width_string);
    free(height_string);
    free(bpp_string);
    free(busy_string);
    free(fail_string);
    // install_firmware_update should reboot.  If it returns, it failed.
    return strdup("");
}

void Register_librecovery_updater_htc() {
    fprintf(stderr, "installing HTC updater extensions\n");

    RegisterFunction("htc.install_radio", UpdateFn);
    RegisterFunction("htc.install_hboot", UpdateFn);
}
