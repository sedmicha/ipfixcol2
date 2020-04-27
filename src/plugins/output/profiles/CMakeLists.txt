add_library(profiles-output MODULE
    profiles.c
)

install(
    TARGETS profiles-output
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

