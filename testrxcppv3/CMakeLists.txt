cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

get_filename_component(SAMPLE_PROJECT "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

project(${SAMPLE_PROJECT} LANGUAGES C CXX)

include(./shared.cmake)

# define the sources
set(SAMPLE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/context.cpp
)
add_executable(${SAMPLE_PROJECT} ${SAMPLE_SOURCES})
add_executable(rxcppv3::examples::${SAMPLE_PROJECT} ALIAS ${SAMPLE_PROJECT})

target_compile_options(${SAMPLE_PROJECT} PUBLIC ${RX_COMPILE_OPTIONS})
target_compile_features(${SAMPLE_PROJECT} PUBLIC ${RX_COMPILE_FEATURES})

target_compile_definitions(${SAMPLE_PROJECT} PRIVATE RX_INFO=0 RX_SKIP_TESTS=0 RX_SLOW=0 RX_DEFER_IMMEDIATE=0)

target_link_libraries(${SAMPLE_PROJECT} ${CMAKE_THREAD_LIBS_INIT})

set_target_properties(${SAMPLE_PROJECT} PROPERTIES FOLDER "Examples")
