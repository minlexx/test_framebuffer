cmake_minimum_required(VERSION 3.0)

project(test_framebuffer LANGUAGES C)

add_executable(test_framebuffer main.c)

install(TARGETS test_framebuffer RUNTIME DESTINATION bin)
