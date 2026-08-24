cmake_minimum_required(VERSION 3.13.1)

file(GLOB_RECURSE EI_SOURCES
    "edge-impulse-sdk/*.cpp"
    "edge-impulse-sdk/*.cc"
    "edge-impulse-sdk/*.c"
    "tflite-model/*.cpp"
    "tflite-model/*.cc"
    "model-parameters/*.cpp"
)

list(FILTER EI_SOURCES EXCLUDE REGEX ".*/porting/(arduino|mbed|zephyr|silabs|stm32-cubeai|particle|himax|himax-we2|android|ambiq|infineon-psoc62|ceva-npn|sony|iar|seeed-vision-ai|ti|ethos-core-driver|mingw32|brickml|renesas-ra|raspberry|synaptics)/.*")

list(FILTER EI_SOURCES EXCLUDE REGEX ".*/(tests|test|examples)/.*")

file(GLOB ESP_DSP_INCLUDE_DIRS
    "edge-impulse-sdk/porting/espressif/esp-dsp/modules/*/include"
)

idf_component_register(
    SRCS ${EI_SOURCES}
    INCLUDE_DIRS
        "."
        "edge-impulse-sdk"
        "edge-impulse-sdk/third_party/flatbuffers/include"
        "edge-impulse-sdk/third_party/ruy"
        "edge-impulse-sdk/third_party/gemmlowp"
        "edge-impulse-sdk/tensorflow"
        "model-parameters"
        "tflite-model"
        ${ESP_DSP_INCLUDE_DIRS}
    REQUIRES "esp_timer"
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -DEIDSP_QUANTIZE_FILTERBANK=0
    -DEI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=0
    -Wno-error
    -Wno-error=maybe-uninitialized
)