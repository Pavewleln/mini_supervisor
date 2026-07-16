cmake_minimum_required(VERSION 3.16)

project(mini_supervisor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mini_supervisor
    main.cpp
    config.cpp
    process.cpp
    service.cpp
    supervisor.cpp
    http_server.cpp
    file_descriptor.cpp
)

target_include_directories(mini_supervisor PRIVATE
    json/single_include
)

target_compile_options(mini_supervisor PRIVATE
    -Wall
    -Wextra
    -pedantic
)
