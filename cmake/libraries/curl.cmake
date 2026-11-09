if(NOT USE_HTTP)
    return()
endif()

set(INTERNAL_CURL_DIR ${SOURCE_DIR}/thirdparty/curl-8.15.0/include)

find_package(CURL QUIET)

if(NOT CURL_FOUND)
    set(CURL_DEFINITIONS USE_INTERNAL_CURL_HEADERS)
    set(CURL_INCLUDE_DIRS ${INTERNAL_CURL_DIR})
endif()

# Fixed in OpenMOHAA Plus: HAS_LIBCURL enables loading libcurl at run time (code/sys/sys_curl.c).
# It was defined by the per-directory CMake files that #781 replaced, so curl was never loaded.
list(APPEND CURL_DEFINITIONS HAS_LIBCURL=1)

list(APPEND CLIENT_DEFINITIONS ${CURL_DEFINITIONS})
list(APPEND CLIENT_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})

# For some functions like update checker
list(APPEND SERVER_DEFINITIONS ${CURL_DEFINITIONS})
list(APPEND SERVER_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
