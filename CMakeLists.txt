cmake_minimum_required(VERSION 3.0)

project(test_framebuffer LANGUAGES C)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

find_package(FreeImage REQUIRED)

add_executable(test_framebuffer main.c)

target_link_libraries(test_framebuffer FreeImage::FreeImage)

install(TARGETS test_framebuffer RUNTIME DESTINATION bin)
