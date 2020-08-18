cmake_minimum_required(VERSION 3.1.0)
project(ipfix-s3-output)

include(CMakeModules/install_dirs.cmake)

# Create a linkable module
add_library(ipfix-s3-output MODULE
    src/IPFIXOutputPlugin.cpp
    src/IPFIXOutput.cpp
    src/IPFIXOutput.hpp
    src/Config.cpp
    src/Config.hpp
    src/S3Output.cpp
    src/S3Output.hpp
    src/S3Common.cpp
    src/S3Common.hpp
    src/WrapperStream.hpp
    src/Statistics.hpp
    src/BufferPool.hpp
)

# becuase of AWS SDK
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

# Include custom FindXXX modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules")

find_package(IPFIXcol2 2.0.0 REQUIRED)
find_package(LibFds REQUIRED)
find_package(AWSSDK REQUIRED COMPONENTS s3)

# Build using the C++ standard version 11.
target_compile_features(ipfix-s3-output PUBLIC cxx_std_11)

# Header files for source code building
include_directories(
    "${IPFIXCOL2_INCLUDE_DIRS}"  # IPFIXcol2 header files
    "${FDS_INCLUDE_DIRS}"        # libfds header files
)

# The libraries used by your executable.
target_link_libraries(ipfix-s3-output ${AWSSDK_LINK_LIBRARIES})

install(
    TARGETS ipfix-s3-output
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-ipfix-s3-output.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-ipfix-s3-output.7")

    add_custom_command(TARGET ipfix-s3-output PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()
