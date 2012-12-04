## Android Makefile for kernel module
## by Kirby.Wu, 20090729, mediatek.inc
##
## this android makefile is (currently) used to build all kernel modules 
## and put them into android platform output directory. 
##
## to build kernel modules into system.img,
##   config build/target/board/<target>/BoardConfig.mk:
##     KERNEL_CONFIG_FILE := <your desired config file> # use .config if omit
##     TARGET_MODULES     := true                       # do make modules
##

#ifeq ($(MTK_PROJECT), gw616)
#  KERNEL_CONFIG_FILE := config-mt6516-phone
#else
#  ifeq ($(MTK_PROJECT), ds269)
#    KERNEL_CONFIG_FILE := config-mt6516-gemini
#  else
#    ifeq ($(MTK_PROJECT), oppo)
#      KERNEL_CONFIG_FILE := config-mt6516-oppo
#    else
#      ifeq ($(MTK_PROJECT), mt6516_evb)
#        KERNEL_CONFIG_FILE := config-mt6516-evb
#      else
#        ifeq ($(MTK_PROJECT), mt6573_evb)
#          KERNEL_CONFIG_FILE := config-mt6573-evb
#        else
#          ifeq ($(MTK_PROJECT), zte73v1)
#            KERNEL_CONFIG_FILE := config-mt6573-zte73v1
#          else
#            KERNEL_CONFIG_FILE := config-mt6516-$(MTK_PROJECT)
#          endif
#        endif
#      endif
#    endif
#  endif
#endif

$(info using $(KERNEL_CONFIG_FILE) .... )
ifeq ($(TARGET_KMODULES),true)
ALL_PREBUILT += $(TARGET_OUT)/lib/modules/modules.order
$(BUILT_SYSTEMIMAGE): kernel_modules
$(TARGET_OUT)/lib/modules/modules.order: kernel_modules
kernel_modules:
	@echo "building linux kernel modules..."
#ifneq (,$(KERNEL_CONFIG_FILE))
#	@cat kernel/$(KERNEL_CONFIG_FILE) > kernel/.config
#endif
	make MTK_PROJECT=$(MTK_PROJECT) -C  kernel modules
	INSTALL_MOD_STRIP=1 MTK_PROJECT=$(MTK_PROJECT) INSTALL_MOD_PATH=../$(TARGET_OUT) INSTALL_MOD_DIR=../$(TARGET_OUT) make -C kernel android_modules_install
endif
