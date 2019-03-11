# Create a linkable module
add_library(report-output MODULE
    src/plugin.cpp
    src/report.cpp
    src/report.hpp
    src/utils.cpp
    src/utils.hpp
    src/output.hpp
    src/output.cpp
)

install(
    TARGETS report-output
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

#if (ENABLE_DOC_MANPAGE)
#    # Build a manual page
#    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-report-output.7.rst")
#    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-report-output.7")
#
#    add_custom_command(TARGET dummy-output PRE_BUILD
#        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
#        DEPENDS ${SRC_FILE}
#        VERBATIM
#    )
#
#    install(
#        FILES "${DST_FILE}"
#        DESTINATION "${INSTALL_DIR_MAN}/man7"
#    )
#endif()
