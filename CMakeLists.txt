cmake_minimum_required (VERSION 2.6)
if (COMMAND cmake_policy)
  cmake_policy (SET CMP0003 NEW)
endif (COMMAND cmake_policy)


project (rs)
set (${PROJECT_NAME}_MAJOR_VERSION 0)
set (${PROJECT_NAME}_MINOR_VERSION 2)
set (${PROJECT_NAME}_PATCH_LEVEL 0)


add_definitions ("-ggdb3 -O2 -Wall -Wextra -Werror -D_FILE_OFFSET_BITS=64")
include_directories ("/usr/include/apr-1/")

add_library (RS RS.cpp)
add_library (Http Http.cpp)

add_executable (rsmanager rsmanager.cpp)
add_executable (test_http test_http.cpp)
add_executable (test_rs test_rs.cpp)

target_link_libraries (rsmanager RS Http boost_thread boost_regex boost_filesystem curl pthread)
target_link_libraries (test_http Http curl)
target_link_libraries (test_rs RS Http boost_thread boost_regex boost_filesystem curl pthread)

