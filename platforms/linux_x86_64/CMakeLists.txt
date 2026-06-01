# linux_x86_64 platform shim — re-exports the shared OpenCV camera
add_library(kameret_platform_linux_x86_64 INTERFACE)
target_include_directories(kameret_platform_linux_x86_64 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/..   # exposes opencv_camera.hpp
)
target_link_libraries(kameret_platform_linux_x86_64 INTERFACE
    kameret_core
    ${OpenCV_LIBS}
)
