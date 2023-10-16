cmake_minimum_required(VERSION 3.8.2)

project(3d_software_renderer)

set(CMAKE_EXPORT_COMPILE_COMMANDS on)

find_package(SDL2 REQUIRED)

add_executable(${PROJECT_NAME} main.cc
  game.cc)

include_directories(${PROJECT_NAME}, ${SDL2_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE
  ${SDL2_LIBRARIES}
  cglm_headers)

add_subdirectory(vendor/cglm/ EXCLUDE_FROM_ALL)
