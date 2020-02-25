add_library(profiles-intermediate MODULE
    profiles.c
)

install(
    TARGETS profiles-intermediate
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

