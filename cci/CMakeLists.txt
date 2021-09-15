#
#  Copyright 2008 Search Solution Corporation
#  Copyright 2016 CUBRID Corporation
# 
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
# 
#       http://www.apache.org/licenses/LICENSE-2.0
# 
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
# 
#

set(CASCCI_SOURCES
  ${BASE_DIR}/porting.c
  ${CCI_DIR}/cas_cci.c
  ${CCI_DIR}/cci_util.c
  ${CCI_DIR}/cci_query_execute.c
  ${CCI_DIR}/cci_net_buf.c
  ${CCI_DIR}/cci_network.c
  ${CCI_DIR}/cci_handle_mng.c
  ${CCI_DIR}/cci_t_set.c
  ${CCI_DIR}/cci_t_lob.c
  ${CCI_DIR}/cci_common.c
  ${CCI_DIR}/cci_log.cpp
  ${CCI_DIR}/cci_map.cpp
  ${CCI_DIR}/cci_properties.c
  ${CCI_DIR}/cci_ssl.c
  )
if(WIN32)
  list(APPEND CASCCI_SOURCES ${CCI_DIR}/cci_wsa_init.c)
  list(APPEND CASCCI_SOURCES ${BASE_DIR}/rand.c)
endif(WIN32)

SET_SOURCE_FILES_PROPERTIES(
  ${CASCCI_SOURCES}
  PROPERTIES LANGUAGE CXX
  )

GET_FILENAME_COMPONENT(CCI_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../ ABSOLUTE)
GET_FILENAME_COMPONENT(VERSION_FILE_PATH ${CCI_ROOT_PATH}/BUILD_NUMBER ABSOLUTE)
GET_FILENAME_COMPONENT(VERSION_DIST_FILE_PATH ${CCI_ROOT_PATH}/CCI-VERSION-DIST ABSOLUTE)

# Version info For CCI
if(EXISTS ${VERSION_FILE_PATH})
  set(VERSION_FILE BUILD_NUMBER)
elseif(EXISTS ${VERSION_DIST_FILE_PATH})
  message(WARNING "Could not find a BUILD_NUMBER file. so used CCI_VERSION_DIST:${VERSION_DIST_FILE_PATH}")
  set(VERSION_FILE_PATH ${VERSION_DIST_FILE_PATH})
  set(VERSION_FILE CCI-VERSION-DIST)
else(EXISTS ${VERSION_FILE_PATH})
  message(WARNING "Could not find a BUILD_NUMBER file")
endif(EXISTS ${VERSION_FILE_PATH})
message(STATUS "Get version information from ${VERSION_FILE_PATH}")
# Generate the same file in other directory to trigger cmake re-configure when the file changes
# configure_file(${VERSION_FILE_PATH} ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/${VERSION_FILE}.check_cache COPYONLY)
file(STRINGS ${VERSION_FILE_PATH} VERSION_STR)
if(VERSION_STR MATCHES "[0-9]+\\.[0-9]+\\.[0-9]+(\\.[0-9]+)?(-[a-zA-Z0-9_]+)?")
  string(REGEX MATCHALL "[0-9]+|-([A-Za-z0-9_]+)" VERSION_MATCHES ${VERSION_STR})
  list(LENGTH VERSION_MATCHES VERSION_MATCHES_LENGTH)
  list(GET VERSION_MATCHES 0 CCI_MAJOR_VERSION)
  list(GET VERSION_MATCHES 1 CCI_MINOR_VERSION)
  list(GET VERSION_MATCHES 2 CCI_PATCH_VERSION)
else()
  message(FATAL_ERROR "Failed to parse a version string from ${VERSION_FILE} file")
endif()

if(VERSION_MATCHES_LENGTH GREATER 3)
  list(GET VERSION_MATCHES 3 CCI_EXTRA_VERSION)
  if(VERSION_MATCHES_LENGTH GREATER 4)
    list(GET VERSION_MATCHES 4 CCI_HASH_TAG)
  endif(VERSION_MATCHES_LENGTH GREATER 4)
else(VERSION_MATCHES_LENGTH GREATER 3)
  find_package(Git)
  if(EXISTS "${CCI_ROOT_PATH}/.git" AND GIT_FOUND)
    set(CCI_VERSION_START_DATE "2021-07-14")
    set(CCI_VERSION_SRC_LIST "BUILD_NUMBER cci/ cmake/ CMakeLists.txt external/ include/ src/base src/cci win/")
    execute_process(COMMAND ${GIT_EXECUTABLE} --version
    COMMAND awk "{ printf \"%s\", $3 }"
    OUTPUT_VARIABLE GIT_VERSION
    WORKING_DIRECTORY ${CCI_ROOT_PATH})
    if(GIT_VERSION VERSION_GREATER 1.7.6)
      execute_process(COMMAND ${GIT_EXECUTABLE} rev-list --count --after ${CCI_VERSION_START_DATE} HEAD
        COMMAND awk "{ printf \"%04d\", $1 }"
        OUTPUT_VARIABLE commit_count RESULT_VARIABLE git_result
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${CCI_ROOT_PATH})
    else(GIT_VERSION VERSION_GREATER 1.7.6)
      # If you have trouble with execute_process on Windows, it is likely an issue caused by git installation.
      # <git-installation-directory>/bin has two files, bash.exe and sh.exe, that messes up CMake's execute_process
      #
      # There are two possible fixes for this issue:
      # 1. Add <git-installation-directory>/usr/bin to %PATH% environment variable
      # 2. Remove <git-installation-directory>/bin from %PATH% environment variable
      # You may prefer the first option if you want to use git command in command prompt.
      #
      # If both fixes fail, just replace execute_process(...) with set (commit_count 0). It is not a critical step to generate the project.
      # Also you may send us a message at https://www.cubrid.org/contact
      execute_process(COMMAND ${GIT_EXECUTABLE} log --after ${CCI_VERSION_START_DATE} --oneline
        COMMAND wc -l
        COMMAND awk "{ printf \"%04d\", $1 }"
        OUTPUT_VARIABLE commit_count RESULT_VARIABLE git_result
        ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
        WORKING_DIRECTORY ${CCI_ROOT_PATH})
    endif(GIT_VERSION VERSION_GREATER 1.7.6)
    if(git_result)
      message(FATAL_ERROR "Could not get count information from Git")
    endif(git_result)
    set(CCI_EXTRA_VERSION ${commit_count})
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short=7 HEAD
      OUTPUT_VARIABLE commit_hash RESULT_VARIABLE git_result
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
      WORKING_DIRECTORY ${CCI_ROOT_PATH})
    if(git_result)
      message(FATAL_ERROR "Could not get hash information from Git")
    endif(git_result)
    set(CCI_HASH_TAG ${commit_hash})
    # Generate the same file in other directory to trigger cmake re-configure when the HEAD changes
    # configure_file("${CCI_ROOT_PATH}/.git/HEAD" ${CMAKE_CURRENT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/HEAD.check_cache COPYONLY)
  else(EXISTS "${CCI_ROOT_PATH}/.git" AND GIT_FOUND)
    set(CUBRID_EXTRA_VERSION "0000")
  endif(EXISTS "${CCI_ROOT_PATH}/.git" AND GIT_FOUND)
endif(VERSION_MATCHES_LENGTH GREATER 3)
set(CCI_BUILD_NUMBER ${CCI_MAJOR_VERSION}.${CCI_MINOR_VERSION}.${CCI_PATCH_VERSION}.${CCI_EXTRA_VERSION}-${CCI_HASH_TAG})
message(STATUS "CCI_BUILD_NUMBER:[${CCI_BUILD_NUMBER}]")
# TODO: for only cci version (cci_version.h)
configure_file(${CCI_ROOT_PATH}/cmake/version_cci.h.cmake ${CMAKE_BINARY_DIR}/cci_version.h)

if(WIN32)
execute_process(COMMAND CMD /c echo ${CCI_BUILD_NUMBER}>CCI-VERSION-DIST
        WORKING_DIRECTORY ${CCI_ROOT_PATH})
else(WIN32)
execute_process(COMMAND sh -c "echo ${CCI_BUILD_NUMBER}>CCI-VERSION-DIST"
        WORKING_DIRECTORY ${CCI_ROOT_PATH})
endif(WIN32)

add_library(cascci SHARED ${CASCCI_SOURCES})
set_target_properties(cascci PROPERTIES SOVERSION "${CCI_MAJOR_VERSION}.${CCI_MINOR_VERSION}")
set_target_properties(cascci PROPERTIES PUBLIC_HEADER "${CCI_DIR}/cas_cci.h;${CCI_DIR}/compat_dbtran_def.h;${CCI_DIR}/broker_cas_error.h")
target_include_directories(cascci PRIVATE ${EP_INCLUDES})
target_link_libraries(cascci LINK_PRIVATE ${EP_LIBS} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(cascci ${CMAKE_DL_LIBS})

if(NOT WITH_CCI)
  add_library(cascci_static STATIC ${CASCCI_SOURCES})
  target_include_directories(cascci_static PRIVATE ${EP_INCLUDES})
  target_link_libraries(cascci_static LINK_PRIVATE ${EP_LIBS} ${CMAKE_THREAD_LIBS_INIT})
  target_link_libraries(cascci_static ${CMAKE_DL_LIBS})
  set_target_properties(cascci_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
  set_target_properties(cascci_static PROPERTIES OUTPUT_NAME cas_cci)
  if(WIN32)
    target_compile_options(cascci_static PRIVATE 
    "$<$<CONFIG:Debug>:/MTd>"
    "$<$<CONFIG:MinSizeRel>:/MT>"
    "$<$<CONFIG:Release>:/MT>"
    "$<$<CONFIG:RelWithDebInfo>:/MT>"
    )
    if(TARGET_PLATFORM_BITS EQUAL 32)
      target_compile_definitions(cascci_static PRIVATE _USE_32BIT_TIME_T)
    endif(TARGET_PLATFORM_BITS EQUAL 32)
    if(FOR_OTHER_DRIVER)
      target_compile_definitions(cascci_static PRIVATE CCI_OLEDB)
      target_compile_definitions(cascci_static PRIVATE CAS_CCI)
    else(FOR_OTHER_DRIVER)
      target_compile_definitions(cascci_static PRIVATE CAS_CCI_DL)
    endif(FOR_OTHER_DRIVER)
    set_target_properties(cascci_static PROPERTIES LINK_FLAGS_RELEASE "/NODEFAULTLIB:libcmt.lib" LINK_FLAGS_DEBUG "/NODEFAULTLIB:msvcrt.lib")
    target_link_libraries(cascci_static LINK_PRIVATE ws2_32)
  endif(WIN32)
  
  if(DEFINED EP_TARGETS)
    add_dependencies(cascci_static ${EP_TARGETS})
  endif(DEFINED EP_TARGETS)

  install(TARGETS cascci_static
    LIBRARY DESTINATION ${CUBRID_STATIC_LIBDIR} COMPONENT STATIC_CCI
    ARCHIVE DESTINATION ${CUBRID_STATIC_LIBDIR} COMPONENT STATIC_CCI
  )
endif(NOT WITH_CCI)

if(WIN32)
  if(TARGET_PLATFORM_BITS EQUAL 32)
    target_compile_definitions(cascci PRIVATE _USE_32BIT_TIME_T)
  endif(TARGET_PLATFORM_BITS EQUAL 32)
  if(FOR_OTHER_DRIVER)
    target_compile_definitions(cascci PRIVATE CCI_OLEDB)
    target_compile_definitions(cascci PRIVATE CAS_CCI)
  else(FOR_OTHER_DRIVER)
    target_compile_definitions(cascci PRIVATE CAS_CCI_DL)
  endif(FOR_OTHER_DRIVER)
  set_target_properties(cascci PROPERTIES LINK_FLAGS "/DEF:\"${CMAKE_SOURCE_DIR}/win/cascci/cascci.def\"" LINK_FLAGS_RELEASE "/NODEFAULTLIB:libcmt.lib" LINK_FLAGS_DEBUG "/NODEFAULTLIB:msvcrt.lib")
  target_link_libraries(cascci LINK_PRIVATE ws2_32)
endif(WIN32)

if(DEFINED EP_TARGETS)
  add_dependencies(cascci ${EP_TARGETS})
endif(DEFINED EP_TARGETS)

install(TARGETS cascci
  RUNTIME DESTINATION ${CUBRID_CCIDIR}/bin COMPONENT CCI
  LIBRARY DESTINATION ${CUBRID_CCIDIR}/lib COMPONENT CCI
  ARCHIVE DESTINATION ${CUBRID_CCIDIR}/lib COMPONENT CCI
  PUBLIC_HEADER DESTINATION ${CUBRID_CCIDIR}/include COMPONENT CCI
  )

# install pdb files for debugging on windows
if(WIN32)
  install(DIRECTORY
    ${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/
    DESTINATION ${CUBRID_CCIDIR}/bin COMPONENT Debuginfo
    FILES_MATCHING PATTERN "*.pdb"
    )
endif(WIN32)
