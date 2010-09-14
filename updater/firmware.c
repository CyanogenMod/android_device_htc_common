/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "bootloader.h"
#include "common.h"
#include "firmware.h"
#include "mtdutils/mtdutils.h"
#include "mincrypt/sha.h"

#include <errno.h>
#include <string.h>
#include <sys/reboot.h>

/* Bootloader / Recovery Flow
 *
 * On every boot, the bootloader will read the bootloader_message
 * from flash and check the command field.  The bootloader should
 * deal with the command field not having a 0 terminator correctly
 * (so as to not crash if the block is invalid or corrupt).
 *
 * The bootloader will have to publish the partition that contains
 * the bootloader_message to the linux kernel so it can update it.
 *
 * if command == "boot-recovery" -> boot recovery.img
 * else if command == "update-radio" -> update radio image (below)
 * else if command == "update-hboot" -> update hboot image (below)
 * else -> boot boot.img (normal boot)
 *
 * Radio/Hboot Update Flow
 * 1. the bootloader will attempt to load and validate the header
 * 2. if the header is invalid, status="invalid-update", goto #8
 * 3. display the busy image on-screen
 * 4. if the update image is invalid, status="invalid-radio-image", goto #8
 * 5. attempt to update the firmware (depending on the command)
 * 6. if successful, status="okay", goto #8
 * 7. if failed, and the old image can still boot, status="failed-update"
 * 8. write the bootloader_message, leaving the recovery field
 *    unchanged, updating status, and setting command to
 *    "boot-recovery"
 * 9. reboot
 *
 * The bootloader will not modify or erase the cache partition.
 * It is recovery's responsibility to clean up the mess afterwards.
 */

#undef LOGE
#define LOGE(...) fprintf(stderr, "E:" __VA_ARGS__)


int verify_image(const uint8_t* expected_sha1) {
    MtdPartition* part = mtd_find_partition_by_name(CACHE_NAME);
    if (part == NULL) {
        printf("verify image: failed to find cache partition\n");
        return -1;
    }

    size_t block_size;
    if (mtd_partition_info(part, NULL, &block_size, NULL) != 0) {
        printf("verify image: failed to get cache partition block size\n");
        return -1;
    }
    printf("block size is 0x%x\n", block_size);

    char* buffer = malloc(block_size);
    if (buffer == NULL) {
        printf("verify image: failed to allocate memory\n");
        return -1;
    }

    MtdReadContext* ctx = mtd_read_partition(part);
    if (ctx == NULL) {
        printf("verify image: failed to init read context\n");
        return -1;
    }

    size_t pos = 0;
    if (mtd_read_data(ctx, buffer, block_size) != block_size) {
        printf("verify image: failed to read header\n");
        return -1;
    }
    pos += block_size;

    if (strncmp(buffer, "MSM-RADIO-UPDATE", 16) != 0) {
        printf("verify image: header missing magic\n");
        return -1;
    }

    unsigned image_offset = *(unsigned*)(buffer+24);
    unsigned image_length = *(unsigned*)(buffer+28);
    printf("image offset 0x%x length 0x%x\n", image_offset, image_length);
    mtd_read_skip_to(ctx, image_offset);

    FILE* f = fopen("/tmp/read-radio.dat", "wb");
    if (f == NULL) {
        printf("verify image: failed to open temp file\n");
    }

    SHA_CTX sha_ctx;
    SHA_init(&sha_ctx);

    size_t total = 0;
    while (total < image_length) {
        size_t to_read = image_length - total;
        if (to_read > block_size) to_read = block_size;
        ssize_t read = mtd_read_data(ctx, buffer, to_read);
        if (read < 0) {
            printf("verify image: failed reading image (read 0x%x so far)\n",
                   total);
            return -1;
        }
        if (f) {
            fwrite(buffer, 1, read, f);
        }
        SHA_update(&sha_ctx, buffer, read);
        total += read;
    }

    if (f) fclose(f);

    free(buffer);
    mtd_read_close(ctx);

    const uint8_t* sha1 = SHA_final(&sha_ctx);
    if (memcmp(sha1, expected_sha1, SHA_DIGEST_SIZE) != 0) {
        printf("verify image: sha1 doesn't match\n");
        return -1;
    }

    printf("verify image: verification succeeded\n");

    return 0;
}

int install_firmware_update(const char *update_type,
                            const char *update_data,
                            size_t update_length,
                            int width, int height, int bpp,
                            const char* busy_image,
                            const char* fail_image,
                            const char *log_filename,
                            const uint8_t* expected_sha1) {
    if (update_data == NULL || update_length == 0) return 0;

    mtd_scan_partitions();

    /* We destroy the cache partition to pass the update image to the
     * bootloader, so all we can really do afterwards is wipe cache and reboot.
     * Set up this instruction now, in case we're interrupted while writing.
     */

    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n--wipe_cache\n", sizeof(boot.command));
    if (set_bootloader_message(&boot)) return -1;

    if (write_update_for_bootloader(
            update_data, update_length,
            width, height, bpp, busy_image, fail_image, log_filename)) {
        LOGE("Can't write %s image\n(%s)\n", update_type, strerror(errno));
        return -1;
    }

    if (verify_image(expected_sha1) == 0) {
        /* The update image is fully written, so now we can instruct
         * the bootloader to install it.  (After doing so, it will
         * come back here, and we will wipe the cache and reboot into
         * the system.)
         */
        snprintf(boot.command, sizeof(boot.command), "update-%s", update_type);
        if (set_bootloader_message(&boot)) {
            return -1;
        }

        reboot(RB_AUTOBOOT);

        // Can't reboot?  WTF?
        LOGE("Can't reboot\n");
    }
    return -1;
}
