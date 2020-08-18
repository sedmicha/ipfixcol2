cmake_minimum_required(VERSION 3.1.0)
project(ipfix-s3-input)

include(CMakeModules/install_dirs.cmake)

# Create a linkable module
add_library(ipfix-s3-input MODULE
    src/ipfix.cpp
    src/Config.hpp
    src/Config.cpp
    src/S3Downloader.hpp
    src/S3Downloader.cpp
    src/Statistics.hpp
)

# becuase of AWS SDK
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Include custom FindXXX modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

find_package(IPFIXcol2 2.0.0 REQUIRED)
find_package(LibFds REQUIRED)
find_package(AWSSDK REQUIRED COMPONENTS s3)

# Build using the C++ standard version 11.
target_compile_features(ipfix-s3-input PUBLIC cxx_std_11)

# Header files for source code building
include_directories(
    "${IPFIXCOL2_INCLUDE_DIRS}"  # IPFIXcol2 header files
    "${FDS_INCLUDE_DIRS}"        # libfds header files
)

target_link_libraries(ipfix-s3-input ${AWSSDK_LINK_LIBRARIES})

install(
    TARGETS ipfix-s3-input
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-ipfix-s3-input.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-ipfix-s3-input.7")

    add_custom_command(TARGET ipfix-s3-input PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
