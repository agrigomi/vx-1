include .fg/_defaults_
include .fg/$(PROJECT)/_defaults_


ifeq ($(CONFIG),)
	CONFIG=$(DEFAULT_CONFIG)
endif

ifeq ($(CONFIG),)
	exit
endif


ROOT_DIR=$(DEPLOY_DIR)/$(CONFIG)
ISO_TOOL=$(GRUB_DIR)/grub-mkrescue
TARGET_ISO=$(DEPLOY_DIR)/vx-$(CONFIG).iso

.PHONY: clean

$(TARGET_ISO): $(ROOT_DIR) $(ISO_TOOL)
	$(ISO_TOOL) -o $(TARGET_ISO) --directory=$(GRUB_CORE) --locale-directory=$(LOCALE_DIR) $(ROOT_DIR)

$(ROOT_DIR):
	make kernel

$(ISO_TOOL):
	make grub2
	
clean:
	rm -f $(TARGET_ISO)

