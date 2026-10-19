cmake_minimum_required(VERSION 4.0)

project(rtweekend C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(thirdparty/glm)

add_executable(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/ray.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interval.hpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/image.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/camera.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/camera.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/hittable/hittable.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hittable/hittable_list.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hittable/hittable_list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hittable/sphere.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hittable/sphere.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl/stb.cpp
)

target_include_directories(${PROJECT_NAME}
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/stb
)

target_link_libraries(${PROJECT_NAME}
PRIVATE
    glm
)

target_precompile_headers(${PROJECT_NAME}
PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core.hpp
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        NOMINMAX
    )
endif()