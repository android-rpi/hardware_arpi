ifeq ($(TARGET_BOARD_PLATFORM),bcm2711)
include $(call first-makefiles-under,$(call my-dir))
endif
