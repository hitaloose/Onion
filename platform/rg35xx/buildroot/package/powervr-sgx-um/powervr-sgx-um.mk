################################################################################
#
# powervr-sgx-um
#
################################################################################

POWERVR_SGX_UM_VERSION = 989041df5c242b4d114ab796d1a8f9504822a7af
POWERVR_SGX_UM_SITE = $(call github,rg35xx-cfw,cubieboard6_ubuntu_gpu_drivers,$(POWERVR_SGX_UM_VERSION))
POWERVR_SGX_UM_LICENSE = Actions (Zhuhai) terms of use (redistributable)
POWERVR_SGX_UM_LICENSE_FILES = TERMS_OF_USE.TXT
POWERVR_SGX_UM_INSTALL_STAGING = YES
POWERVR_SGX_UM_DEPENDENCIES = libdrm
POWERVR_SGX_UM_PROVIDES = libegl libgles

POWERVR_SGX_UM_DDK = 1.14.3625561
# Libraries installed as lib<name>.so.$(DDK) with .so.1 and .so links, as the
# DDK's install.sh does; the DRI window system and desktop GL need X11
POWERVR_SGX_UM_LIBS = EGL IMGegl GLES_CM GLESv2 glslcompiler usc srv_um srv_init \
	pvr2d pvrPVR2D_LINUXFBWSEGL pvrPVR2D_FRONTWSEGL pvrPVR2D_FLIPWSEGL \
	pvrPVR2D_BLITWSEGL
POWERVR_SGX_UM_BINS = pvrsrvctl sgx_init_test services_test sgx_blit_test \
	sgx_flip_test pvr2d_test

# $(1): destination root
define POWERVR_SGX_UM_INSTALL_LIBS
	$(foreach lib,$(POWERVR_SGX_UM_LIBS),
		$(INSTALL) -D -m 0644 $(@D)/v2/lib$(lib).so \
			$(1)/usr/lib/lib$(lib).so.$(POWERVR_SGX_UM_DDK)
		ln -sf lib$(lib).so.$(POWERVR_SGX_UM_DDK) $(1)/usr/lib/lib$(lib).so.1
		ln -sf lib$(lib).so.$(POWERVR_SGX_UM_DDK) $(1)/usr/lib/lib$(lib).so
	)
	ln -sf libGLESv2.so.$(POWERVR_SGX_UM_DDK) $(1)/usr/lib/libGLESv2.so.2
	ln -sf libGLES_CM.so.$(POWERVR_SGX_UM_DDK) $(1)/usr/lib/libGLESv1_CM.so.1
	ln -sf libGLES_CM.so.$(POWERVR_SGX_UM_DDK) $(1)/usr/lib/libGLESv1_CM.so
endef

define POWERVR_SGX_UM_INSTALL_STAGING_CMDS
	$(call POWERVR_SGX_UM_INSTALL_LIBS,$(STAGING_DIR))
	for d in EGL GLES GLES2 KHR; do \
		mkdir -p $(STAGING_DIR)/usr/include/$$d; \
		cp $(@D)/include/$$d/*.h $(STAGING_DIR)/usr/include/$$d/; \
	done
	$(INSTALL) -D -m 0644 $(POWERVR_SGX_UM_PKGDIR)/egl.pc \
		$(STAGING_DIR)/usr/lib/pkgconfig/egl.pc
	$(INSTALL) -D -m 0644 $(POWERVR_SGX_UM_PKGDIR)/glesv2.pc \
		$(STAGING_DIR)/usr/lib/pkgconfig/glesv2.pc
endef

define POWERVR_SGX_UM_INSTALL_TARGET_CMDS
	$(call POWERVR_SGX_UM_INSTALL_LIBS,$(TARGET_DIR))
	$(foreach bin,$(POWERVR_SGX_UM_BINS),
		$(INSTALL) -D -m 0755 $(@D)/v2/$(bin) $(TARGET_DIR)/usr/bin/$(bin)
	)
	$(INSTALL) -D -m 0644 $(POWERVR_SGX_UM_PKGDIR)/powervr.ini \
		$(TARGET_DIR)/etc/powervr.ini
endef

$(eval $(generic-package))
