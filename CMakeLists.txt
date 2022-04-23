# cmake file for building PandoraSDK
#-------------------------------------------------------------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR "PandoraSDK requires an out-of-source build.")
endif()

set(PandoraSDK_VERSION 03.04.01)

project(PandoraSDK VERSION ${PandoraSDK_VERSION} LANGUAGES CXX)

#include(PandoraCMakeSettings)

# - Collect sources - not ideal because you have to keep running CMake to pick up changes
file(GLOB_RECURSE PANDORA_SDK_SRCS RELATIVE ${PROJECT_SOURCE_DIR} "src/*.cc")

add_library(PandoraSDK SHARED ${PANDORA_SDK_SRCS})

target_include_directories(PandoraSDK
    PUBLIC include
    PRIVATE src
)

target_compile_definitions(PandoraSDK
    PRIVATE SRC_DIR=${PROJECT_SOURCE_DIR}/src
)
if(CMAKE_COMPILER_IS_GNUCXX)
target_compile_options(PandoraSDK
    PUBLIC -fno-elide-constructors
)
endif()

target_compile_features(PandoraSDK
    PUBLIC
        cxx_auto_type
        cxx_range_for
    PRIVATE
        cxx_variadic_templates
)

install(TARGETS PandoraSDK DESTINATION ${PROJECT_SOURCE_DIR}/lib COMPONENT Runtime)
install(DIRECTORY include/ DESTINATION ${PROJECT_SOURCE_DIR}/include COMPONENT Development FILES_MATCHING PATTERN "*.h")

if( EXISTS "${PROJECT_SOURCE_DIR}/cmake/PandoraSDKConfig.cmake.in" )
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/PandoraSDKConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/PandoraSDKConfig.cmake" @ONLY
    )
    install(FILES "${PROJECT_BINARY_DIR}/PandoraSDKConfig.cmake" DESTINATION lib/cmake)
endif()

if( EXISTS "${PROJECT_SOURCE_DIR}/cmake/PandoraSDKConfigVersion.cmake.in" )
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/PandoraSDKConfigVersion.cmake.in"
        "${PROJECT_BINARY_DIR}/PandoraSDKConfigVersion.cmake" @ONLY
    )
    install(FILES "${PROJECT_BINARY_DIR}/PandoraSDKConfigVersion.cmake" DESTINATION lib/cmake)
endif()

if( EXISTS "${PROJECT_SOURCE_DIR}/cmake/PandoraSDKLibDeps.cmake.in" )
    configure_file(
        "${PROJECT_SOURCE_DIR}/cmake/PandoraSDKLibDeps.cmake.in"
        "${PROJECT_BINARY_DIR}/PandoraSDKLibDeps.cmake" @ONLY
    )
    #EXPORT_LIBRARY_DEPENDENCIES( "PandoraSDKLibDeps.cmake" )
    install(FILES "${PROJECT_BINARY_DIR}/PandoraSDKLibDeps.cmake" DESTINATION lib/cmake)
endif()

