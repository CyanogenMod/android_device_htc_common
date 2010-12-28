# Broadcom FM Radio
PRODUCT_PACKAGES += \
    FM \
    hcitool

BOARD_GLOBAL_CFLAGS += -DHAVE_FM_RADIO
BOARD_HAVE_FM_RADIO := true

