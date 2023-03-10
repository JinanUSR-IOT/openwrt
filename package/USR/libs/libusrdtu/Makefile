include $(TOPDIR)/rules.mk

PKG_NAME:=libusrdtu
PKG_VERSION:=1
PKG_RELEASE:=$(AUTORELEASE)
PKG_INSTALL:=1
PKG_BUILD_PARALLEL:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

DISABLE_NLS:=

define Package/libusrdtu
  SECTION:=libs
  CATEGORY:=Libraries
  DEPENDS:=+libc
  TITLE:=USR DTU library for USR-G809
endef

define Package/libusrdtu/description
endef

define Package/dtu_demo
$(call Package/libusrdtu)
  SECTION:=utils
  CATEGORY:=Utilities
  SUBMENU:=USR Applications
  DEPENDS:=+libusrdtu +libc
  TITLE:=USR DTU Test Demo for USR-G809
endef

define Package/dtu_demo/description
$(call Package/libusrdtu)
endef

TARGET_CFLAGS += $(FPIC) -flto
TARGET_LDFLAGS += -flto

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/*.h $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/libusrdtu.so $(1)/usr/lib/
endef

define Package/libusrdtu/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/lib
	# $(INSTALL_BIN) files/* $(1)/lib/
	$(CP) $(PKG_BUILD_DIR)/libusrdtu.so $(1)/usr/lib/
endef

define Package/dtu_demo/install
	$(INSTALL_DIR) $(1)/usr/bin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dtu_demo $(1)/usr/bin/
	$(INSTALL_BIN) ./files/usr_dtu_service $(1)/etc/init.d/
	$(INSTALL_BIN) ./files/usr_dtu $(1)/etc/config/
endef

define Package/dtu_demo/postinst
#!/bin/sh
# check if we are on real system
	if [ -z "$${IPKG_INSTROOT}" ]; then
		echo "Enabling rc.d symlink for usr_dtu"
		/etc/init.d/usr_dtu_service enable
	fi
	exit 0
endef

define Package/dtu_demo/prerm
#!/bin/sh
# check if we are on real system
	if [ -z "$${IPKG_INSTROOT}" ]; then
		echo "Removing rc.d symlink for usr_dtu"
		/etc/init.d/usr_dtu_service disable
	fi
	exit 0
endef

$(eval $(call BuildPackage,libusrdtu))
$(eval $(call BuildPackage,dtu_demo))
