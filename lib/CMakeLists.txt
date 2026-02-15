add_subdirectory(SeggerRTT)
add_subdirectory(nanopb)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/nanopb/extra)
find_package(Nanopb REQUIRED)