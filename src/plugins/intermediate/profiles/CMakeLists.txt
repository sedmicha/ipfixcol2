add_library(profiles-intermediate MODULE
    config.h
    config.c
    profiles.c
)

install(
    TARGETS profiles-intermediate
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

if (ENABLE_DOC_MANPAGE)
    # Build a manual page
    set(SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/doc/ipfixcol2-profiles-inter.7.rst")
    set(DST_FILE "${CMAKE_CURRENT_BINARY_DIR}/ipfixcol2-profiles-inter.7")

    add_custom_command(TARGET profiles-intermediate PRE_BUILD
        COMMAND ${RST2MAN_EXECUTABLE} --syntax-highlight=none ${SRC_FILE} ${DST_FILE}
        DEPENDS ${SRC_FILE}
        VERBATIM
        )

    install(
        FILES "${DST_FILE}"
        DESTINATION "${INSTALL_DIR_MAN}/man7"
    )
endif()