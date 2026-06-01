add_library(kameret_core STATIC src/placeholder.cpp)

target_include_directories(kameret_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Eigen is header-only; fmt and opencv pulled in by consumers
target_link_libraries(kameret_core PUBLIC
    Eigen3::Eigen
    fmt::fmt
)
