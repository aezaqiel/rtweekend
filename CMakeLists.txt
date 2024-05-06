cmake_minimum_required(VERSION 3.25)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "" FORCE)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)

project(rtweekend C)

add_subdirectory(vendor/cglm)

add_executable(${PROJECT_NAME}
    src/main.c
)

target_include_directories(${PROJECT_NAME}
PRIVATE
    src
    vendor/stb
    vendor/cglm/include
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    cglm
)
