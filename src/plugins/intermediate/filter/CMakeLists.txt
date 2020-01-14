add_library(filter-intermediate MODULE
    filter.c
    config.c
    config.h
)

install(
    TARGETS filter-intermediate
    LIBRARY DESTINATION "${INSTALL_DIR_LIB}/ipfixcol2/"
)

