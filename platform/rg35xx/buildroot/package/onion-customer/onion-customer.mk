################################################################################
#
# onion-customer
#
################################################################################

# Exo 2 (googlefonts/Exo-2.0) is the main source, WenQuanYi Micro Hei an extra download
ONION_CUSTOMER_VERSION = f83ea8a02d3e1d6963ab6e910038521f27e283a2
ONION_CUSTOMER_SITE = $(call github,googlefonts,Exo-2.0,$(ONION_CUSTOMER_VERSION))
ONION_CUSTOMER_WQY_SOURCE = wqy-microhei-0.2.0-beta.tar.gz
ONION_CUSTOMER_EXTRA_DOWNLOADS = https://downloads.sourceforge.net/wqy/$(ONION_CUSTOMER_WQY_SOURCE)
ONION_CUSTOMER_LICENSE = OFL-1.1 (Exo 2), Apache-2.0 or GPL-3.0 with font exception (WenQuanYi Micro Hei)
ONION_CUSTOMER_LICENSE_FILES = OFL.txt

define ONION_CUSTOMER_EXTRACT_WQY
	$(call suitable-extractor,$(ONION_CUSTOMER_WQY_SOURCE)) \
		$(ONION_CUSTOMER_DL_DIR)/$(ONION_CUSTOMER_WQY_SOURCE) | \
		$(TAR) -C $(@D) $(TAR_OPTIONS) -
endef
ONION_CUSTOMER_POST_EXTRACT_HOOKS += ONION_CUSTOMER_EXTRACT_WQY

define ONION_CUSTOMER_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/fonts/ttf/Exo2-BoldItalic.ttf \
		$(TARGET_DIR)/customer/app/Exo-2-Bold-Italic.ttf
	$(INSTALL) -D -m 0644 $(@D)/wqy-microhei/wqy-microhei.ttc \
		$(TARGET_DIR)/customer/app/wqy-microhei.ttc
	$(INSTALL) -D -m 0755 $(ONION_CUSTOMER_PKGDIR)/jsonval \
		$(TARGET_DIR)/customer/app/jsonval
	mkdir -p $(TARGET_DIR)/appconfigs
endef

$(eval $(generic-package))
