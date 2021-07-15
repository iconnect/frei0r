set (SOURCES deface.cpp)
set (TARGET deface)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

if (MSVC)
  set (SOURCES ${SOURCES} ${FREI0R_DEF})
endif (MSVC)

add_library (${TARGET}  MODULE ${SOURCES})
set_target_properties (${TARGET} PROPERTIES PREFIX "")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Now we initalised deface as a library, and we can link extra stuff after this point.

if(LINUX)
    message(STATUS ">>> Building on Linux <<<")

    include_directories(/home/centos/.pyenv/versions/3.6-dev/include/python3.6m)
    link_directories(/home/centos/.pyenv/versions/3.6-dev/lib)

    add_library(python3.6m SHARED IMPORTED)
    set_target_properties(python3.6m PROPERTIES
        IMPORTED_LOCATION "/home/centos/.pyenv/versions/3.6-dev/lib/libpython3.6m.so"
        INTERFACE_INCLUDE_DIRECTORIES "/home/centos/.pyenv/versions/3.6-dev/include/python3.6m"
        )

    set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "-export-dynamic")
    target_link_libraries(${TARGET} python3.6m)
    target_link_libraries(${TARGET} ${CMAKE_DL_LIBS})

else()
    message(STATUS ">>> Building on Mac <<<")

    # this doesn't work, path hardcoded for now, fix later.
    # execute_process(COMMAND python-config --libraries OUTPUT_VARIABLE PYTHON_CONFIG_LIBS_RAW)
    # execute_process(COMMAND echo ${PYTHON_CONFIG_LIBS_RAW} | sed "s/-I//g" OUTPUT_VARIABLE PYTHON_CONFIG_LIBS)
    # include_directories(${PYTHON_CONFIG_LIBS})

    #
    # Include dirs
    #

    # fix later.
    include_directories(/Users/adinapoli/.pyenv/versions/3.6-dev/include/python3.6m)
    link_directories(/Users/adinapoli/.pyenv/versions/3.6-dev/lib)

    #
    # Linker directives.
    #

    add_library(python3.6m SHARED IMPORTED)
    set_target_properties(python3.6m PROPERTIES
      IMPORTED_LOCATION "/Users/adinapoli/.pyenv/versions/3.6-dev/lib/libpython3.6m.dylib"
      INTERFACE_INCLUDE_DIRECTORIES "/Users/adinapoli/.pyenv/versions/3.6-dev/include/python3.6m"
    )
    #find_library(python3.6m
    #    NAMES python3.6m
    #    NO_DEFAULT_PATH
    #    PATHS "/Users/adinapoli/.pyenv/versions/3.6-dev/lib"
    #    REQUIRED
    #)

    add_library (intl STATIC IMPORTED)
    set_target_properties(intl PROPERTIES
      IMPORTED_LOCATION "/usr/local/lib/libintl.8.dylib"
      INTERFACE_INCLUDE_DIRECTORIES "/usr/local/include"
    )

    add_library (iconv STATIC IMPORTED)
    set_target_properties(iconv PROPERTIES
      IMPORTED_LOCATION "/usr/local/Cellar/libiconv/1.16/lib/libiconv.a"
      INTERFACE_INCLUDE_DIRECTORIES "/usr/local/Cellar/libiconv/1.16/include"
    )

    target_link_libraries(${TARGET} "-framework CoreFoundation")
    set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "-Wl,-F/Library/Frameworks")

    target_link_libraries(${TARGET} python3.6m)
    target_link_libraries(${TARGET} iconv)
    target_link_libraries(${TARGET} intl)
    target_link_libraries(${TARGET} ${CMAKE_DL_LIBS})

endif()

install (TARGETS ${TARGET} LIBRARY DESTINATION ${LIBDIR})
