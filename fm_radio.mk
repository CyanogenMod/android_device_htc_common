# Broadcom FM Radio
ifeq ($(BOARD_HAVE_FM_RADIO),true)
PRODUCT_PACKAGES += \
    FM \
    hcitool
endif
