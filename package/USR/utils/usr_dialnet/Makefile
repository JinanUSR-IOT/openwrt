##############################################
# OpenWrt Makefile for helloworld program
#
#
# Most of the variables used here are defined in
# the include directives below. We just need to
# specify a basic description of the package,
# where to build our program, where to find
# the source files, and where to install the
# compiled program on the router.
#
# Be very careful of spacing in this file.
# Indents should be tabs, not spaces, and
# there should be no trailing whitespace in
# lines that are not commented.
#
##############################################

include $(TOPDIR)/rules.mk

# Name and release number of this package
PKG_NAME:=usr_dialnet
PKG_RELEASE:=1

# This specifies the directory where we're going to build the program.
# The root build directory, $(BUILD_DIR), is by default the build_mipsel
# directory in your OpenWrt SDK directory
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

# Specify package information for this program.
# The variables defined here should be self explanatory.
define Package/usr_dialnet
	SECTION:=utils
	CATEGORY:=Utilities
	SUBMENU:=USR Applications
	TITLE:=usr_dialnet -- for 2/3/4/5G dialnet app
	DEPENDS:=+libuci +libpthread +libc +usbutils +quectel-CM
endef

# Specify what needs to be done to prepare for building the package.
# In our case, we need to copy the source files to the build directory.
# This is NOT the default.  The default uses the PKG_SOURCE_URL and the
# PKG_SOURCE which is not defined here to download the source from the web.
# In order to just build a simple program that we have just written, it is
# much easier to do it this way.
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

# We do not need to define Build/Configure or Build/Compile directives
# The defaults are appropriate for compiling a simple program such as this one

# Specify where and how to install the program. Since we only have one file,
# the helloworld executable, install it by copying it to the /bin directory on
# the router. The $(1) variable represents the root directory on the router running
# OpenWrt. The $(INSTALL_DIR) variable contains a command to prepare the install
# directory if it does not already exist.  Likewise $(INSTALL_BIN) contains the
# command to copy the binary file from its current location (in our case the build
# directory) to the install directory.
# define Package/usr_dialnet/install 
# 	$(INSTALL_DIR) $(1)/bin $(1)/bin/
# 	$(INSTALL_BIN) $(PKG_BUILD_DIR)/usr_dialnet $(1)/bin/
# endef
define Package/usr_dialnet/install 
	$(INSTALL_DIR) $(1)/bin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/usr_dialnet $(1)/bin/
	$(INSTALL_BIN) ./files/usr_dialnet_service $(1)/etc/init.d/
	$(CP) ./files/cellular_config $(1)/etc/config/
endef

define Package/$(PKG_NAME)/postinst
#!/bin/sh
# check if we are on real system
	if [ -z "$${IPKG_INSTROOT}" ]; then
		echo "Enabling rc.d symlink for usr_dialnet"
		/etc/init.d/usr_dialnet enable
	fi
	exit 0
endef

define Package/$(PKG_NAME)/prerm
#!/bin/sh
# check if we are on real system
	if [ -z "$${IPKG_INSTROOT}" ]; then
		echo "Removing rc.d symlink for usr_dialnet"
		/etc/init.d/usr_dialnet disable
	fi
	exit 0
endef

# This line executes the necessary commands to compile our program.
# The above define directives specify all the information needed, but this
# line calls BuildPackage which in turn actually uses this information to
# build a package.
$(eval $(call BuildPackage,usr_dialnet))

