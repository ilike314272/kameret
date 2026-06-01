add_executable(kameret_demo main.cpp)
target_link_libraries(kameret_demo PRIVATE
    kameret_core
    kameret_adapter_mediapipe
    kameret_platform_${KAMERET_PLATFORM}
    ${OpenCV_LIBS}
)
