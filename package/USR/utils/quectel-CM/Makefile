include $(TOPDIR)/rules.mk
 
# Name and release number of this package
PKG_NAME:=quectel-CM
PKG_RELEASE:=1
 
# This specifies the directory where we're going to build the program.
# The root build directory, $(BUILD_DIR), is by default the build_mipsel
# directory in your OpenWrt SDK directory
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)
 
include $(INCLUDE_DIR)/package.mk
 
# Specify package information for this program.
# The variables defined here should be self explanatory.
define Package/quectel-CM
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=quectel dial tool
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/quectel-CM/install 
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/quectel-CM $(1)/usr/bin
endef

$(eval $(call BuildPackage,quectel-CM))
