# Enable MCUboot
set(SB_CONFIG_BOOTLOADER_MCUBOOT y CACHE BOOL "Enable MCUboot")

# Add the AppCPU as the second image
ExternalZephyrProject_Add(
    APPLICATION wol_appcpu
    SOURCE_DIR  ${APP_DIR}/appcpu
    BOARD       esp32_devkitc/esp32/appcpu
)

# The main image (procpu) must be configured before appcpu
sysbuild_add_dependencies(CONFIGURE wol_appcpu Wake_On_Lan)
