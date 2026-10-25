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

    ${CMAKE_CURRENT_SOURCE_DIR}/src/hittable/hittable.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hittable/hittable_list.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hittable/hittable_list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hittable/sphere.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hittable/sphere.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/camera/camera.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/camera/thinlens_camera.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/camera/thinlens_camera.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/sampler/sampler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sampler/independent_sampler.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sampler/independent_sampler.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/integrator/integrator.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/integrator/sampler_integrator.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/integrator/sampler_integrator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/integrator/path_integrator.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/integrator/path_integrator.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/src/progress.hpp

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

# add_custom_command(
#     TARGET ${PROJECT_NAME}
#     POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${CMAKE_CURRENT_BINARY_DIR}/image.png"
#         "${CMAKE_CURRENT_SOURCE_DIR}/res/image.png"
#     COMMENT "copying output image to res"
# )