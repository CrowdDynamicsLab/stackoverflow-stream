cmake_minimum_required(VERSION 3.2.0)
project(stackoverflow-stream)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

find_package(MeTA REQUIRED)
find_package(LibArchive REQUIRED)
find_package(LibXml2 REQUIRED)

add_executable(repack src/repack.cpp)
target_link_libraries(repack meta-io ${LibArchive_LIBRARIES})
target_include_directories(repack PRIVATE ${LibArchive_INCLUDE_DIRS})

add_executable(extract-sequences src/extract_sequences.cpp)
target_link_libraries(extract-sequences meta-io ${LIBXML2_LIBRARIES})
target_include_directories(extract-sequences PRIVATE ${LIBXML2_INCLUDE_DIR})
target_compile_definitions(extract-sequences PRIVATE ${LIBXML2_DEFINITIONS})