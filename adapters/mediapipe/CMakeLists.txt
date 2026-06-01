option(EYETRACKER_ENABLE_MEDIAPIPE "Build real MediaPipe estimator" OFF)

add_library(kameret_adapter_mediapipe INTERFACE)
target_include_directories(kameret_adapter_mediapipe INTERFACE include)
target_link_libraries(kameret_adapter_mediapipe INTERFACE kameret_core)

if(EYETRACKER_ENABLE_MEDIAPIPE)
    # Assumes mediapipe was built with Bazel/CMake and its install tree
    # is rooted at external/mediapipe/install/.
    set(MP_ROOT ${CMAKE_SOURCE_DIR}/external/mediapipe/install)
    find_library(MP_LIB mediapipe PATHS ${MP_ROOT}/lib REQUIRED)
    target_include_directories(kameret_adapter_mediapipe INTERFACE ${MP_ROOT}/include)
    target_link_libraries(kameret_adapter_mediapipe INTERFACE ${MP_LIB})
    target_compile_definitions(kameret_adapter_mediapipe INTERFACE KAMERET_MEDIAPIPE=1)
endif()
