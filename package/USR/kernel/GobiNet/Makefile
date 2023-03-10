#
# Copyright (C) 2008 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=usb-net-GobiNet
PKG_VERSION:=
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(KERNEL_BUILD_DIR)/$(PKG_NAME)
include $(INCLUDE_DIR)/package.mk

define KernelPackage/usb-net-GobiNet
  CATEGORY:=Kernel modules
  SUBMENU:=USB Support
  TITLE:=Support for Quectel 4G Module
  DEPENDS:=@USB_SUPPORT +kmod-usb-core +kmod-usb-net
  FILES:=$(PKG_BUILD_DIR)/GobiNet.ko
  AUTOLOAD:=$(call AutoProbe,GobiNet)
  $(call AddDepends/usb-net)
endef

define KernelPackage/usb-net-GobiNet/description
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		$(KERNEL_MAKE_FLAGS) \
		M="$(PKG_BUILD_DIR)" \
		EXTRA_CFLAGS="$(BUILDFLAGS)" \
		modules
endef

$(eval $(call KernelPackage,usb-net-GobiNet))
