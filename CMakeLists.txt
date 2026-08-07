# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

# Compiler Optimization
add_compile_options(-Ofast -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -Wall -flto)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

# set boardtype
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(example_auto_set_url.cmake)

project(Pico2UltraHiResUSBDDC C CXX ASM)

# The firmware still uses pico-extras' minimal USB device stack. Keep it out of
# the source tree and pin the revision matching the previously vendored snapshot.
include(FetchContent)
FetchContent_Declare(
    pico_extras
    GIT_REPOSITORY https://github.com/raspberrypi/pico-extras.git
    GIT_TAG ed98c7acb694757715ede81c044a7404e1762499
    PATCH_COMMAND git apply --whitespace=nowarn
        ${CMAKE_CURRENT_LIST_DIR}/cmake/patches/pico-extras-usb-buffer-size.patch
)
if(COMMAND FetchContent_MakeAvailable)
    FetchContent_MakeAvailable(pico_extras)
else()
    FetchContent_GetProperties(pico_extras)
    if(NOT pico_extras_POPULATED)
        FetchContent_Populate(pico_extras)
    endif()
    add_subdirectory(${pico_extras_SOURCE_DIR} ${pico_extras_BINARY_DIR})
endif()

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add Source Directory
add_subdirectory(src)

# Compile Definitions
target_compile_definitions(Pico2UltraHiResUSBDDC PRIVATE
    AUDIO_FREQ_MAX=192000

    # ours are zero based, so say so
    PICO_USBDEV_USE_ZERO_BASED_INTERFACES=1

    # need large descriptor
    PICO_USBDEV_MAX_DESCRIPTOR_SIZE=256


    PICO_USBDEV_ISOCHRONOUS_BUFFER_STRIDE_TYPE=3
    PICO_USBDEV_ENABLE_DEBUG_TRACE
)
