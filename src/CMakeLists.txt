# CMSIS-DSP 必要ソースを選択（使う関数に応じて最小限にする）
set(DSP_SRC
        ${cmsis_dsp_SOURCE_DIR}/Source/FilteringFunctions/arm_fir_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/FilteringFunctions/arm_fir_init_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/FilteringFunctions/arm_fir_interpolate_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/FilteringFunctions/arm_fir_interpolate_init_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/FilteringFunctions/arm_biquad_cascade_df1_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/FilteringFunctions/arm_biquad_cascade_df1_init_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/TransformFunctions/arm_cfft_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/TransformFunctions/arm_cfft_init_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/TransformFunctions/arm_cfft_radix8_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/TransformFunctions/arm_rfft_fast_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/TransformFunctions/arm_rfft_fast_init_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/TransformFunctions/arm_bitreversal.c
        ${cmsis_dsp_SOURCE_DIR}/Source/TransformFunctions/arm_bitreversal2.c
        ${cmsis_dsp_SOURCE_DIR}/Source/CommonTables/arm_common_tables.c
        ${cmsis_dsp_SOURCE_DIR}/Source/CommonTables/arm_const_structs.c
        ${cmsis_dsp_SOURCE_DIR}/Source/SupportFunctions/arm_q31_to_float.c
        ${cmsis_dsp_SOURCE_DIR}/Source/BasicMathFunctions/arm_add_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/BasicMathFunctions/arm_scale_f32.c
        ${cmsis_dsp_SOURCE_DIR}/Source/ComplexMathFunctions/arm_cmplx_mult_cmplx_f32.c
        )

# Project-owned firmware sources
set(PROJECT_SRC
        main.c
        general_func.c
        usb_device_control.c
        core1.c
        i2s_pio_interface.c
        transmit_to_dac.c
        upsampling_coef.c
        fft_fir_coef.c
        upsampling.c
        fft_fir_core0.c
        fft_fir_core1.c
        ringbuffer.c
        debug_with_gpio.c
        ess_specific.c
        nonblocking_i2c.c
)

# Keep the selected CMSIS-DSP sources in a separate C11 target so the firmware
# target can use C23 without changing the language mode of dependencies.
add_library(cmsis_dsp_selected STATIC ${DSP_SRC})
set_target_properties(cmsis_dsp_selected PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON
)
target_compile_options(cmsis_dsp_selected PRIVATE
        -ffast-math
)
target_include_directories(cmsis_dsp_selected PRIVATE
        ${cmsis_dsp_SOURCE_DIR}/Include
)
target_link_libraries(cmsis_dsp_selected PRIVATE
        cmsis_core_headers
)

# Compile project-owned sources separately because Pico SDK adds some of its
# own C sources directly to the final executable target.
add_library(firmware_sources OBJECT ${PROJECT_SRC})
target_compile_features(firmware_sources PRIVATE c_std_23)
set_target_properties(firmware_sources PROPERTIES
        C_STANDARD 23
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON
)
target_compile_options(firmware_sources PRIVATE
        -ffast-math
        -Wall
        -Wextra
        -Wpedantic
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wmissing-declarations
        -Wmissing-variable-declarations
        -Wformat=2
        -Wundef
        -Wshadow
        -Werror
)
if(ENABLE_GCC_ANALYZER)
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "ENABLE_GCC_ANALYZER requires GCC")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE)
    set_target_properties(firmware_sources PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION FALSE
            INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE
    )
    target_compile_options(firmware_sources PRIVATE
            -O0
            -fanalyzer
    )
endif()
target_include_directories(firmware_sources PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)
target_include_directories(firmware_sources SYSTEM PRIVATE
        ${cmsis_dsp_SOURCE_DIR}/Include
        ${lufa_SOURCE_DIR}
        ${pico_extras_SOURCE_DIR}/src/rp2_common/usb_device/include
)
target_link_libraries(firmware_sources PRIVATE
        pico_stdlib_headers
        hardware_pio_headers
        usb_common
        pico_multicore_headers
        hardware_clocks_headers
        hardware_dma_headers
        hardware_pwm_headers
        hardware_uart_headers
        hardware_i2c_headers
        hardware_vreg_headers
        hardware_irq_headers
        hardware_sync_headers
        cmsis_core_headers
        pico_float_headers
)

# Add executable. Default name is the project name, version 0.1
add_executable(Pico2UltraHiResUSBDDC
        $<TARGET_OBJECTS:firmware_sources>
)

pico_set_program_name(Pico2UltraHiResUSBDDC "Pico2UltraHiResUSBDDC")
pico_set_program_version(Pico2UltraHiResUSBDDC "0.1")

# Generate PIO header
pico_generate_pio_header(firmware_sources ${CMAKE_CURRENT_LIST_DIR}/i2s.pio)

# Modify the below lines to enable/disable output over UART/USB
#pico_enable_stdio_uart(Pico2UltraHiResUSBDDC 0)
#pico_enable_stdio_usb(Pico2UltraHiResUSBDDC 0)

# Add the standard library to the build
target_link_libraries(Pico2UltraHiResUSBDDC
        pico_stdlib)

# Add any user requested libraries
target_link_libraries(Pico2UltraHiResUSBDDC 
        cmsis_dsp_selected
        hardware_pio 
        usb_device 
        pico_multicore
        hardware_clocks
        hardware_dma
        hardware_pwm
        usb_device
        hardware_uart
        hardware_i2c
        # CMSIS-DSP only needs Core compiler headers; Pico SDK supplies them.
        cmsis_core_headers
        pico_float
)

pico_set_float_implementation(Pico2UltraHiResUSBDDC pico_vfp)

target_link_options(Pico2UltraHiResUSBDDC PRIVATE
        "LINKER:-u,__wrap_printf"
        "LINKER:-u,__wrap_puts"
        "LINKER:-u,__wrap_putchar"
        "LINKER:-u,__wrap_getchar"
        "LINKER:-u,__wrap_vprintf"
        "LINKER:-u,__wrap_powf"
        "LINKER:-u,__wrap_log10f"
)

pico_add_extra_outputs(Pico2UltraHiResUSBDDC)
