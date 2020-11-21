PROGRAM = homekit_universal_remote
PROGRAM_SRC_DIR = main

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
	$(abspath components/ir) \
	$(abspath components/wifi-config) \
	$(abspath components/wolfssl) \
	$(abspath components/cjson) \
	$(abspath components/ping) \
	$(abspath components/homekit)

ESPBAUD = 460800
FLASH_SIZE ?= 32


EXTRA_CFLAGS += -DLWIP_NETIF_HOSTNAME=1
EXTRA_CFLAGS += -DLWIP_RAW=1
EXTRA_CFLAGS += -DDEFAULT_RAW_RECVMBOX_SIZE=5
EXTRA_CFLAGS += -DWIFI_PARAM_SAVE=0

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS


include $(SDK_PATH)/common.mk


monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
