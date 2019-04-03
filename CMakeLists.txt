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
target_link_libraries(extract-sequences meta-io meta-stats ${LIBXML2_LIBRARIES})
target_include_directories(extract-sequences PRIVATE
    ${LIBXML2_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/deps/date
    ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(extract-sequences PRIVATE ${LIBXML2_DEFINITIONS})

add_executable(extract-health src/extract_health.cpp)
target_link_libraries(extract-health meta-io meta-stats ${LIBXML2_LIBRARIES})
target_include_directories(extract-health PRIVATE
    ${LIBXML2_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/deps/date
    ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(extract-health PRIVATE ${LIBXML2_DEFINITIONS})

add_executable(extract-tags-and-votes src/extract_tags_and_votes.cpp)
target_link_libraries(extract-tags-and-votes meta-io meta-stats ${LIBXML2_LIBRARIES})
target_include_directories(extract-tags-and-votes PRIVATE
    ${LIBXML2_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/deps/date
    ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(extract-tags-and-votes PRIVATE ${LIBXML2_DEFINITIONS})

add_executable(cluster-sequences src/cluster_sequences.cpp)
target_link_libraries(cluster-sequences meta-sequence meta-hmm)

add_executable(dmmm-gibbs src/dm_mixture_model.cpp)
target_link_libraries(dmmm-gibbs cpptoml meta-io meta-sequence)
target_include_directories(dmmm-gibbs PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(session-counts src/session_counts.cpp)
target_link_libraries(session-counts cpptoml meta-io meta-sequence)
target_include_directories(session-counts PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(dmmm-to-csv src/dmmm_to_csv.cpp)
target_link_libraries(dmmm-to-csv meta-io)
target_include_directories(dmmm-to-csv PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(print-hmm src/print_hmm.cpp)
target_link_libraries(print-hmm meta-sequence meta-hmm)
target_include_directories(print-hmm PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(print-sequences src/print_sequences.cpp)
target_link_libraries(print-sequences meta-io)
target_include_directories(print-sequences PRIVATE ${PROJECT_SOURCE_DIR}/include)
