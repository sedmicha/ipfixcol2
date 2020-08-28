# Create a linkable module

add_library(printer-output MODULE
    src/Config.hpp
    src/Config.cpp
    src/PrinterOutput.cpp
    src/Plugin.cpp
)

install(
	TARGETS printer-output
	LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-printer-output.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-printer-output.7")

    add_custom_command(TARGET printer-output PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
    )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${CMAKE_INSTALL_FULL_MANDIR}/man7"
    )
endif()
