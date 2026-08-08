# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

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

include(CheckIPOSupported)
check_ipo_supported(
    RESULT IPO_SUPPORTED
    OUTPUT IPO_ERROR
    LANGUAGES C CXX
)
if(NOT IPO_SUPPORTED)
    message(FATAL_ERROR "IPO/LTO is not supported: ${IPO_ERROR}")
endif()
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

# Keep third-party source trees out of the repository and pin the revisions that
# match the snapshots previously kept in-tree.
include(FetchContent)
find_package(Git REQUIRED)

FetchContent_Declare(
    cmsis_dsp
    GIT_REPOSITORY https://github.com/ARM-software/CMSIS-DSP.git
    GIT_TAG 17440e5ee9a7e6f22167fa618ba3c5d443fe70b7
)

# CMSIS-DSP's own CMake project builds the whole library. This firmware selects
# only the kernels it uses, so populate the sources without adding that project.
FetchContent_GetProperties(cmsis_dsp)
if(NOT cmsis_dsp_POPULATED)
    if(POLICY CMP0169)
        cmake_policy(PUSH)
        cmake_policy(SET CMP0169 OLD)
    endif()
    FetchContent_Populate(cmsis_dsp)
    if(POLICY CMP0169)
        cmake_policy(POP)
    endif()
endif()

# LUFA normally exposes these definitions through its AVR USB stack. The patch
# keeps only the descriptor headers standalone, matching the previous snapshot.
FetchContent_Declare(
    lufa
    GIT_REPOSITORY https://github.com/abcminiuser/lufa.git
    GIT_TAG 550745aa15d9d764319580d2a4259b1762d3f1a3
    PATCH_COMMAND
        ${CMAKE_COMMAND}
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -DPATCH_SOURCE_DIR=<SOURCE_DIR>
        -DPATCH_FILE=${CMAKE_CURRENT_LIST_DIR}/cmake/patches/lufa-standalone-descriptors.patch
        -P ${CMAKE_CURRENT_LIST_DIR}/cmake/apply-git-patch.cmake
)
if(COMMAND FetchContent_MakeAvailable)
    FetchContent_MakeAvailable(lufa)
else()
    FetchContent_GetProperties(lufa)
    if(NOT lufa_POPULATED)
        FetchContent_Populate(lufa)
    endif()
endif()

FetchContent_Declare(
    pico_extras
    GIT_REPOSITORY https://github.com/raspberrypi/pico-extras.git
    GIT_TAG ed98c7acb694757715ede81c044a7404e1762499
    PATCH_COMMAND
        ${CMAKE_COMMAND}
        -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
        -DPATCH_SOURCE_DIR=<SOURCE_DIR>
        -DPATCH_FILE=${CMAKE_CURRENT_LIST_DIR}/cmake/patches/pico-extras-usb-buffer-size.patch
        -P ${CMAKE_CURRENT_LIST_DIR}/cmake/apply-git-patch.cmake
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
