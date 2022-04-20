# cmake file for building PandoraSDK
#-------------------------------------------------------------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
cmake_policy(SET CMP0076 NEW)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "PandoraSDK requires an out-of-source build.")
endif()

project(PandoraSDK VERSION 03.04.01 LANGUAGES CXX)

add_library(PandoraSDK SHARED "")
add_library(Pandora::SDK ALIAS PandoraSDK)
add_subdirectory(src)

target_include_directories(PandoraSDK
    PUBLIC include
    PRIVATE src
)
target_compile_definitions(PandoraSDK
    PRIVATE SRC_DIR=${PROJECT_SOURCE_DIR}/src
)
#if(CMAKE_COMPILER_IS_GNUCXX)
target_compile_options(PandoraSDK
    PUBLIC -fno-elide-constructors
)
#endif()
target_compile_features(PandoraSDK
    PUBLIC
        cxx_auto_type
        cxx_range_for
    PRIVATE
        cxx_variadic_templates
)

install(TARGETS PandoraSDK DESTINATION ${PROJECT_SOURCE_DIR}/lib COMPONENT Runtime)
install(DIRECTORY include/ DESTINATION ${PROJECT_SOURCE_DIR}/include COMPONENT Development FILES_MATCHING PATTERN "*.h")

