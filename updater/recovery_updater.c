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

Value* UpdateFn(const char* name, State* state, int argc, Expr* argv[]) {
    if (argc != 6) {
        return ErrorAbort(state, "%s() expects 6 args, got %d", name, argc);
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
    if (ReadValueArgs(state, argv, 6, &image,
                 &width_string, &height_string, &bpp_string,
                 &busy, &fail) < 0) {
        return NULL;
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

    install_firmware_update(
        type,
        image->data,
        image->size,
        width, height, bpp,
        busy->size > 0 ? busy->data : NULL,
        fail->size > 0 ? fail->data : NULL,
        "/tmp/recovery.log");
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
