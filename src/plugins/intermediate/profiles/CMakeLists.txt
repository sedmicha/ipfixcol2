add_library(profiles-intermediate MODULE
    config.h
    config.c
    profiles.c
)

install(
    TARGETS profiles-intermediate
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

