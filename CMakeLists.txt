cmake_minimum_required(VERSION 3.16)

project(mini_supervisor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(mini_supervisor
    src/main.cpp
    src/config.cpp
    src/process.cpp
    src/service.cpp
    src/supervisor.cpp
    src/http_server.cpp
    src/file_descriptor.cpp
)

target_include_directories(mini_supervisor PRIVATE
    include
    json/single_include
)

target_compile_options(mini_supervisor PRIVATE
    -Wall
    -Wextra
    -pedantic
)

add_executable(mini_supervisorctl
    src/mini_supervisorctl.cpp
)

target_include_directories(mini_supervisorctl PRIVATE
    include
    json/single_include
)

target_compile_options(mini_supervisorctl PRIVATE
    -Wall
    -Wextra
    -pedantic
)

add_executable(worker_logger
    workers/logger.cpp
)

add_executable(worker_crash
    workers/crash_worker.cpp
)

add_executable(worker_ignore_term
    workers/ignore_term.cpp
)

add_executable(worker_memory
    workers/memory_worker.cpp
)

target_compile_options(worker_logger PRIVATE -Wall -Wextra -pedantic)
target_compile_options(worker_crash PRIVATE -Wall -Wextra -pedantic)
target_compile_options(worker_ignore_term PRIVATE -Wall -Wextra -pedantic)
target_compile_options(worker_memory PRIVATE -Wall -Wextra -pedantic)
