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

cmake_minimum_required(VERSION 2.8)

if(WITH_CCI)
message(STATUS "======== WITH_CCI ==========")
add_subdirectory(cci)
add_subdirectory(tools)
else(WITH_CCI)
message(STATUS "======== CCI REPO MAKE ==========") 
#also searches for modules onto cubrid/cmake
# add other Find[LIBRARY].cmake scripts in cubrid/cmake
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No build type selected, default to Debug")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type (default Debug)")
endif()

project(CUBRID)

#TODO: see why the lines below have no effect
#list(APPEND CMAKE_CXX_SOURCE_FILE_EXTENSIONS "c")
#list(REMOVE_ITEM CMAKE_C_SOURCE_FILE_EXTENSIONS "c")

if(UNIX)
  message(STATUS "  OS = UNIX")
elseif(WIN32)
  message(STATUS "  OS = WIN32")
else()
  message(FATAL_ERROR "OS not supported")
endif()

# options
if(UNIX)
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif(UNIX)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
message(STATUS "  CMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

if(WIN32)
  set(WINDOWS_BASE_DIR ${CMAKE_SOURCE_DIR}/win)
  set(WINDOWS_EXTERNAL_DIR ${WINDOWS_BASE_DIR}/external)
endif(WIN32)

# FIXME: linux 32bit build mode not working now
option(ENABLE_32BIT "Build for 32-bit banaries (on 64-bit platform)" OFF)

# options for external libraries (BUNDLED, EXTERAL or SYSTEM)
# openssl library sources URL
set(WITH_LIBOPENSSL_URL "https://www.openssl.org/source/old/1.1.1/openssl-1.1.1f.tar.gz")
set(WITH_EXTERNAL_PREFIX "EXTERNAL" CACHE STRING "External library prefix (default: EXTERNAL)")
set(WITH_BUNDLED_PREFIX "BUNDLED" CACHE STRING "Bundled library prefix (default: BUNDLED)")
set(WITH_SYSTEM_PREFIX "SYSTEM" CACHE STRING "System library prefix (default: SYSTEM)")

set(WITH_LIBNCURSES   "${WITH_SYSTEM_PREFIX}" CACHE STRING "Build with ncurses library (default: SYSTEM)")
set(WITH_LIBOPENSSL   "${WITH_BUNDLED_PREFIX}" CACHE STRING "Build with openssl library (default: BUNDLED)")

message(STATUS "Build with Curses library: ${WITH_LIBNCURSES}")
message(STATUS "Build with openssl library: ${WITH_LIBOPENSSL}")

# Language setting
#enable_language(C CXX)

# System check
if(UNIX)
  if(CMAKE_COMPILER_IS_GNUCC)
    if(ENABLE_32BIT AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.3.4)
      message(SEND_ERROR "Cannot compile 32-bit program with gcc version (${CMAKE_CXX_COMPILER_VERSION})")
      message(FATAL_ERROR "4.3.4 or higher version is required. (ref. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=16185)")
    endif(ENABLE_32BIT AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.3.4)

    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4.7)
      message(SEND_ERROR "Cannot compile with gcc version ${CMAKE_CXX_COMPILER_VERSION}. Require at least 4.4.7.")
    endif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.4.7)

    # C flags for both debug and release build
    set(CMAKE_C_COMMON_FLAGS "-ggdb -fno-omit-frame-pointer")
    # C++ flags for both debug and release build
    set(CMAKE_CXX_COMMON_FLAGS "${CMAKE_C_COMMON_FLAGS} -std=c++0x")

    # C flags for debug build
    set(CMAKE_C_DEBUG_FLAGS "-Wall -fno-inline ${CMAKE_C_COMMON_FLAGS}")
    # C++ flags for debug build
    set(CMAKE_CXX_DEBUG_FLAGS "-Wall -fno-inline ${CMAKE_CXX_COMMON_FLAGS}")

    # C flags for release build
    set(CMAKE_C_RELEASE_FLAGS "-O2 -DNDEBUG -finline-functions ${CMAKE_C_COMMON_FLAGS}")
    # C++ flags for release build
    set(CMAKE_CXX_RELEASE_FLAGS "-O2 -DNDEBUG -finline-functions ${CMAKE_CXX_COMMON_FLAGS}")

    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${CMAKE_C_DEBUG_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${CMAKE_CXX_DEBUG_FLAGS}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${CMAKE_C_RELEASE_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${CMAKE_CXX_RELEASE_FLAGS}")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${CMAKE_C_RELEASE_FLAGS}")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${CMAKE_CXX_RELEASE_FLAGS}")

    if(WITH_SOURCES)
      set(DEBUG_PREFIX_MAP_FLAG "-fdebug-prefix-map=${CMAKE_SOURCE_DIR}/src=${CMAKE_INSTALL_PREFIX}/src")

      set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_PREFIX_MAP_FLAG}")
      set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${DEBUG_PREFIX_MAP_FLAG}")
      set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${DEBUG_PREFIX_MAP_FLAG}")
      set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${DEBUG_PREFIX_MAP_FLAG}")
      set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${DEBUG_PREFIX_MAP_FLAG}")
      set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${DEBUG_PREFIX_MAP_FLAG}")
    endif(WITH_SOURCES)

    # set has-style
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--hash-style=both")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--hash-style=both")
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--hash-style=both")

  else(CMAKE_COMPILER_IS_GNUCC)
    message(FATAL_ERROR "We currently do not support ${CMAKE_CXX_COMPILER_ID} compiler")
  endif(CMAKE_COMPILER_IS_GNUCC)
else(UNIX)
  # turn off LinkIncremental option for release build

  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replaceIncFlag ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/INCREMENTAL:NO ${replaceIncFlag}" )

  string(REPLACE "INCREMENTAL:YES" "INCREMENTAL:NO" replaceIncFlag ${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO})
  set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/INCREMENTAL:NO ${replaceIncFlag}" )

  #set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX /wd4820 /showIncludes" )
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /wd4820" )
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W3" )
  set(CMAKE_CXX_STANDARD 11)
endif(UNIX)

# check target platform
# Test 32/64 bits
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(TARGET_PLATFORM_BITS 64)
else()
  if(WIN32)
  set(TARGET_PLATFORM_BITS 32)
  else(WIN32)
  message(FATAL_ERROR "Target platform 32bit is no longer supported")
  endif(WIN32)
endif()

# FIXME: remove unused checking rule
#include(CheckSymbolExists)
#if(WIN32)
#  check_symbol_exists("_M_AMD64" "" TARGET_ARCH_X64)
#  if(NOT TARGET_ARCH_X64)
#    check_symbol_exists("_M_IX86" "" TARGET_ARCH_X86)
#  endif(NOT TARGET_ARCH_X64)
#else(WIN32)
#  check_symbol_exists("__i386__" "" TARGET_ARCH_X86)
#  check_symbol_exists("__x86_64__" "" TARGET_ARCH_X64)
#endif(WIN32)

message(STATUS "Build ${PROJECT_NAME} ${CUBRID_VERSION} ${TARGET_PLATFORM_BITS}bit ${CMAKE_BUILD_TYPE} on ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")

# source directories
set(BASE_DIR                 ${CMAKE_SOURCE_DIR}/src/base)
set(CCI_DIR                  ${CMAKE_SOURCE_DIR}/src/cci)
set(TOOLS_DIR                ${CMAKE_SOURCE_DIR}/src/tools)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(
  include
  src/base
  src/cci
  src/tools
  )

include(CheckFunctionExists)
check_function_exists(asprintf HAVE_ASPRINTF)
check_function_exists(vasprintf HAVE_VASPRINTF)
check_function_exists(basename HAVE_BASENAME)
check_function_exists(dirname HAVE_DIRNAME)
check_function_exists(ctime_r HAVE_CTIME_R)
check_function_exists(localtime_r HAVE_LOCALTIME_R)
check_function_exists(drand48_r HAVE_DRAND48_R)
check_function_exists(gethostbyname_r HAVE_GETHOSTBYNAME_R)
check_function_exists(getopt_long HAVE_GETOPT_LONG)
check_function_exists(open_memstream HAVE_OPEN_MEMSTREAM)
check_function_exists(strdup HAVE_STRDUP)
check_function_exists(strlcpy HAVE_STRLCPY)

if(WIN32)
  set(NOMINMAX 1)
endif(WIN32)

include(CheckIncludeFile)
check_include_file(err.h HAVE_ERR_H)
check_include_file(getopt.h HAVE_GETOPT_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)
check_include_file(libgen.h HAVE_LIBGEN_H)
check_include_file(limits.h HAVE_LIMITS_H)
if(NOT HAVE_LIMITS_H)
  set(PATH_MAX 512)
  set(NAME_MAX 255)
  set(LINE_MAX 2048)
endif(NOT HAVE_LIMITS_H)
if(WIN32)
  set(LINE_MAX 2048)
endif(WIN32)

check_include_file(memory.h HAVE_MEMORY_H)
check_include_file(nl_types.h HAVE_NL_TYPES_H)
check_include_file(regex.h HAVE_REGEX_H)
check_include_file(rpc/des_crypt.h HAVE_RPC_DES_CRYPT_H)
check_include_file(stdbool.h HAVE_STDBOOL_H)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stdlib.h HAVE_STDLIB_H)
check_include_file(stddef.h HAVE_STDDEF_H)
check_include_file(string.h HAVE_STRING_H)
check_include_file(strings.h HAVE_STRINGS_H)
check_include_file(sys/param.h HAVE_SYS_PARAM_H)
check_include_file(sys/stat.h HAVE_SYS_STAT_H)
check_include_file(sys/types.h HAVE_SYS_TYPES_H)
check_include_file(unistd.h HAVE_UNISTD_H)
if(HAVE_STDLIB_H AND HAVE_STDDEF_H)
  set(STDC_HEADERS 1)
endif(HAVE_STDLIB_H AND HAVE_STDDEF_H)

include(CheckTypeSize)
check_type_size(char SIZEOF_CHAR)
check_type_size(short SIZEOF_SHORT)
check_type_size(int SIZEOF_INT)
check_type_size(long SIZEOF_LONG)
check_type_size("long long" SIZEOF_LONG_LONG)
check_type_size("void *" SIZEOF_VOID_P)
check_type_size(byte_t BYTE_T)
check_type_size(int8_t INT8_T)
check_type_size(int16_t INT16_T)
check_type_size(int32_t INT32_T)
check_type_size(int64_t INT64_T)
check_type_size(intptr_t INTPTR_T)
check_type_size(uint8_t UINT8_T)
check_type_size(uint16_t UINT16_T)
check_type_size(uint32_t UINT32_T)
check_type_size(uint64_t UINT64_T)
check_type_size(uintptr_t UINTPTR_T)
check_type_size("off_t" SIZEOF_OFF_T)
if(NOT SIZEOF_OFF_T)
  set(off_t "long int")
endif()
check_type_size("size_t" SIZEOF_SIZE_T)
if(NOT SIZEOF_SIZE_T)
  set(size_t "unsinged int")
endif()
check_type_size("pid_t" SIZEOF_PID_T)
if(NOT SIZEOF_PID_T)
  # TODO: util_sa.c:79 remove typedef int pid_t
  if(NOT WIN32)
    set(pid_t "int")
  endif(NOT WIN32)
endif()
set(CMAKE_REQUIRED_FLAGS "-D_LARGEFILE64_SOURCE -finline-functions")
check_type_size("off64_t" SIZEOF_OFF64_T)
unset(CMAKE_REQUIRED_FLAGS)

include(CheckPrototypeDefinition)
if(HAVE_GETHOSTBYNAME_R)
  check_prototype_definition(gethostbyname_r
    "int gethostbyname_r(const char *name, struct hostent *hp, char *buf, size_t buflen, struct hostent **result, int *herr)"
    "0"
    "netdb.h"
    HAVE_GETHOSTBYNAME_R_GLIBC)
  check_prototype_definition(gethostbyname_r
    "struct hostent *gethostbyname_r(const char *name, struct hostent *hp, char *buf, size_t buflen, int *herr)"
    "NULL"
    "netdb.h"
    HAVE_GETHOSTBYNAME_R_SOLARIS)
  check_prototype_definition(gethostbyname_r
    "int gethostbyname_r(const char *name, struct hostent *hp, struct hostent_data *hdata)"
    "0"
    "netdb.h"
    HAVE_GETHOSTBYNAME_R_HOSTENT_DATA)
endif(HAVE_GETHOSTBYNAME_R)

include(CheckSymbolExists)
if(NOT HAVE_STDBOOL_H)
  check_symbol_exists( "_Bool" stdbool.h HAVE__BOOL)
endif(NOT HAVE_STDBOOL_H)

include(CheckCSourceCompiles)
check_c_source_compiles("#include <sys/time.h>\n#include <time.h>\nmain(){}" TIME_WITH_SYS_TIME)
check_c_source_compiles("#include <stdint.h>
int main() {
  uint64_t i, j, r;
  i = 100; j = 200; r = __sync_bool_compare_and_swap(&i, 100, j);
  if (r == 0 || i != 200) { return(1); }
  i = 100; j = 200; r = __sync_bool_compare_and_swap(&i, 101, j);
  if (r != 0 || i != 100) { return(1); }
  i = 100; j = 200; r = __sync_add_and_fetch(&i, j);
  if (r != 300 || i != 300) { return(1); }
  i = 100; r = __sync_lock_test_and_set(&i, 200);
  if (r != 100 || i != 200) { return(1); }
  return(0);
}"
  HAVE_GCC_ATOMIC_BUILTINS)

include(TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)

if(UNIX)
  include(FindThreads)
endif(UNIX)


configure_file(cmake/config.h.cmake config.h)

# platform specifie configurations
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  # Definitions for system
  add_definitions(-DGCC -DLINUX -D_GNU_SOURCE -DI386 -DX86)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wwrite-strings -Wno-cast-qual -Wmissing-prototypes -Wredundant-decls -Wextra -Wno-unused -Wno-format-security -pthread")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wwrite-strings -Wno-cast-qual -Wredundant-decls -Wextra -Wno-unused -Wno-unused-parameter -Wno-format-security -pthread")

  if(SIZEOF_OFF64_T)
    add_definitions( -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64)
  endif(SIZEOF_OFF64_T)

  if(ENABLE_32BIT AND ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
    set(CMAKE_C_COMPILER_ARG1 "-m32 -march=i686")
    set(CMAKE_CXX_COMPILER_ARG1 "-m32 -march=i686")
    #set(CMAKE_C_FLAGS "-m32 ${CMAKE_C_FLAGS}")
    #set(CMAKE_CXX_FLAGS "-m32 ${CMAKE_CXX_FLAGS}")
  endif()
elseif(CMAKE_SYSTEM_NAME MATCHES "AIX")
  # Definitions for system
  add_definitions(-DGCC -DAIX -D_GNU_SOURCE -DPPC -fsigned-char -D_THREAD_SAFE)

  if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
    add_definitions(-DAIX_PPC64 -D__WORDSIZE=64)
    set(CMAKE_C_COMPILER_ARG1 "-pthread -maix64")
    set(CMAKE_CXX_COMPILER_ARG1 "-pthread -maix64")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-bbigtoc -Wl,-brtl -Wl,-bnortllib -Wl,-bnosymbolic -Wl,-bnoautoexp -Wl,-bexpall -Wl,-bmaxdata:0x200000000 -Wl,-bnoipath" CACHE INTERNAL "" FORCE)
  else()
    add_definitions(-D_LARGE_FILES -DAIX_PPC32 -D__WORDSIZE=32)
    set(CMAKE_C_COMPILER_ARG1 "-pthread -maix32")
    set(CMAKE_C_COMPILER_ARG1 "-pthread -maix32")
    set(CMAKE_EXE_LINKER_FLAGS "-Wl,-bbigtoc -Wl,-brtl -Wl,-bnortllib -Wl,-bnosymbolic -Wl,-bnoautoexp -Wl,-bexpall -Wl,-bmaxdata:0x80000000 -Wl,-bnoipath" CACHE INTERNAL "" FORCE)
  endif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DWIN -DWINDOWS -D_CRT_SECURE_NO_WARNINGS /wd4274 /wd4996)
  if(TARGET_PLATFORM_BITS EQUAL 64)
    string(REPLACE "/DWIN32" "/D_WIN64" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  endif(TARGET_PLATFORM_BITS EQUAL 64)
  if(MSVC_VERSION GREATER 1400)
    # experimental option (it can lead the build system to a halt or creating a bad object files)
    set(PARALLEL_JOBS "0" CACHE STRING "Specifies the number of jots to build parallel. 0 means OFF")
    if(PARALLEL_JOBS GREATER $ENV{NUMBER_OF_PROCESSORS})
      add_definitions(/MP)
    elseif(PARALLEL_JOBS GREATER 0)
      add_definitions(/MP${PARALLEL_JOBS})
    endif(PARALLEL_JOBS GREATER $ENV{NUMBER_OF_PROCESSORS})
  endif(MSVC_VERSION GREATER 1400)
endif(CMAKE_SYSTEM_NAME MATCHES "Linux")

# common definitions
set(COMMON_DEFS SYSV MAXPATHLEN=1024 -D_REENTRANT)

# For CUBRID environment variable option
  set(CUBRID_CCIDIR cci)
  set(CUBRID_STATIC_LIBDIR static_lib)
  set(CUBRID_BINDIR bin)
  set(CUBRID_LIBDIR lib)
  set(CUBRID_SRCDIR src)
  set(CUBRID_INCLUDEDIR include)

# For external library options
include(ExternalProject)
set_property(DIRECTORY PROPERTY EP_BASE "${CMAKE_BINARY_DIR}/external")
set(DEFAULT_CONFIGURE_OPTS <SOURCE_DIR>/configure --prefix=${CMAKE_CURRENT_BINARY_DIR}/external --enable-static --disable-shared --with-pic CFLAGS=-w CXXFLAGS=-w)


# WITH_LIBNCURSES can have multiple values with different meanings
# on Linux:
# * "SYSTEM"   - uses ncurses installed in the system (for more info check https://cmake.org/cmake/help/v3.10/module/FindCurses.html)
set(LIBNCURSES_TARGET libncurses)
if(UNIX)
  if(WITH_LIBNCURSES STREQUAL "SYSTEM")
    include(FindCurses)
    find_package(Curses REQUIRED)
    CHECK_LIBRARY_EXISTS("${CURSES_LIBRARIES}" wtimeout "" LIBNCURSES_HAS_WTIMEOUT)
    set(LIBNCURSES_INCLUDES "${CURSES_INCLUDE_DIRS}")
    set(LIBNCURSES_LIBS "${CURSES_LIBRARIES}")

    if(NOT LIBNCURSES_HAS_WTIMEOUT)
      message(STATUS "search for tinfo library")
      find_library(TINFO_LIBRARY NAMES tinfo)
      if(TINFO_LIBRARY)
        message(STATUS "found tinfo library")
        CHECK_LIBRARY_EXISTS("${TINFO_LIBRARY}" wtimeout "" TINFO_HAS_WTIMEOUT)
        if(TINFO_HAS_WTIMEOUT)
          set(CURSES_LIBRARIES ${CURSES_LIBRARIES} ${TINFO_LIBRARY})
        else(TINFO_HAS_WTIMEOUT)
          message(FATAL_ERROR "tinfo library does not have wtimeout")
        endif(TINFO_HAS_WTIMEOUT)
      else(TINFO_LIBRARY)
        message(FATAL_ERROR "no tinfo library")
      endif(TINFO_LIBRARY)
    endif(NOT LIBNCURSES_HAS_WTIMEOUT)
  else(WITH_LIBNCURSES STREQUAL "SYSTEM")
    message(FATAL_ERROR "Invalid WITH_LIBNCURSES option value ${WITH_LIBNCURSES}")
  endif(WITH_LIBNCURSES STREQUAL "SYSTEM")
endif(UNIX)



# WITH_LOBOPENSSL can have multiple values with different meanings
# on Linux:
# * "EXTERNAL" - (default) builds openssl library from URL stored in ${WITH_LIBOPENSSL_URL} uses the library created by the build
# * "SYSTEM"   - use openssl library located in:
#                  ${WITH_EXTERNAL_LIBS_PATH}
#                  ${WITH_LIBOPENSSL_ROOT_PATH}/lib
#                use openssl headers located in:
#                  ${WITH_EXTERNAL_HEADERS_PATH}
#                  ${WITH_LIBOPENSSL_ROOT_PATH}/include
# on Windows:
# * "EXTERNAL" - (default) uses the prebuilt library from cubrid/win/external
if(WITH_LIBOPENSSL STREQUAL "EXTERNAL")
  set(LIBOPENSSL_TARGET libopenssl)
  if(UNIX)
    #compile OpenSSL library given an internet url to a OpenSSL archive
    #e.g. https://www.openssl.org/source/openssl-1.1.1f.tar.gz
    externalproject_add(${LIBOPENSSL_TARGET}
      URL                  ${WITH_LIBOPENSSL_URL}
      TIMEOUT              600
      DOWNLOAD_NO_PROGRESS 1
      CONFIGURE_COMMAND    <SOURCE_DIR>/config --prefix=${CMAKE_CURRENT_BINARY_DIR}/external no-shared # ${DEFAULT_CONFIGURE_OPTS}
      BUILD_COMMAND        make all AUTOCONF=: AUTOHEADER=: AUTOMAKE=: ACLOCAL=:
      INSTALL_COMMAND      make install_sw AUTOCONF=: AUTOHEADER=: AUTOMAKE=: ACLOCAL=:
      )
    set(LIBOPENSSL_LIBS ${CMAKE_CURRENT_BINARY_DIR}/external/lib/libssl.a ${CMAKE_CURRENT_BINARY_DIR}/external/lib/libcrypto.a)
    set(LIBOPENSSL_INCLUDES ${CMAKE_CURRENT_BINARY_DIR}/external/include)
    list(APPEND EP_TARGETS ${LIBOPENSSL_TARGET})
  else(UNIX)
    set(LIBOPENSSL_LIBS ${WINDOWS_EXTERNAL_DIR}/openssl/lib/libssl.lib)
    list(APPEND LIBOPENSSL_LIBS ${WINDOWS_EXTERNAL_DIR}/openssl/lib/libcrypto.lib)
    list(APPEND LIBOPENSSL_LIBS Crypt32 Ws2_32)
    set(LIBOPENSSL_INCLUDES ${WINDOWS_EXTERNAL_DIR}/openssl/include)
  endif(UNIX)
elseif(WITH_LIOPENSSL STREQUAL "SYSTEM")
  if(UNIX)
    find_library(LIBOPENSSL_LIBS NAMES libssl.a libcrypto.a PATHS ${WITH_EXTERNAL_LIBS_PATH} ${WITH_LIBOPENSSL_ROOT_PATH}/lib REQUIRED)
    find_path(LIBOPENSSL_INCLUDES NAMES openssl/conf.h openssl/evp.h PATHS ${WITH_EXTERNAL_HEADERS_PATH} ${WITH_LIBOPENSSL_ROOT_PATH}/include REQUIRED)
    message(STATUS "Found openssl library: ${LIBOPENSSL_LIBS}")
    message(STATUS "Found openssl headers: ${LIBOPENSSL_INCLUDES}")
  endif(UNIX)
else(WITH_LIOPENSSL STREQUAL "BUNDLED")
  if(UNIX)
    set(LIBOPENSSL_TARGET libopenssl)
    set(LIBOPENSSL_LIBS ${CMAKE_SOURCE_DIR}/external/openssl/lib/libssl.a ${CMAKE_SOURCE_DIR}/external/openssl/lib/libcrypto.a)
    set(LIBOPENSSL_INCLUDES ${CMAKE_SOURCE_DIR}/external/openssl/include)
    list(APPEND EP_TARGETS ${LIBOPENSSL_TARGET})
    message(STATUS "built-in openssl library: ${LIBOPENSSL_LIBS}")
    message(STATUS "built-in openssl headers: ${LIBOPENSSL_INCLUDES}")
  else(UNIX)
    if(TARGET_PLATFORM_BITS EQUAL 32)
      if(CMAKE_GENERATOR MATCHES "Visual Studio 11 2012")
        set(LIBOPENSSL_LIBS ${WINDOWS_EXTERNAL_DIR}/openssl/lib_v110/libssl.lib)
        list(APPEND LIBOPENSSL_LIBS ${WINDOWS_EXTERNAL_DIR}/openssl/lib_v110/libcrypto.lib)
      else(CMAKE_GENERATOR MATCHES "Visual Studio 11 2012")
        set(LIBOPENSSL_LIBS ${WINDOWS_EXTERNAL_DIR}/openssl/lib_v140/libssl.lib)
        list(APPEND LIBOPENSSL_LIBS ${WINDOWS_EXTERNAL_DIR}/openssl/lib_v140/libcrypto.lib)
      endif(CMAKE_GENERATOR MATCHES "Visual Studio 11 2012")
    else(TARGET_PLATFORM_BITS EQUAL 32)
      if(CMAKE_GENERATOR MATCHES "Visual Studio 11 2012")
        set(LIBOPENSSL_LIBS ${WINDOWS_EXTERNAL_DIR}/openssl/lib64_v110/libssl.lib)
        list(APPEND LIBOPENSSL_LIBS ${WINDOWS_EXTERNAL_DIR}/openssl/lib64_v110/libcrypto.lib)
      else(CMAKE_GENERATOR MATCHES "Visual Studio 11 2012")
        set(LIBOPENSSL_LIBS ${WINDOWS_EXTERNAL_DIR}/openssl/lib64_v140/libssl.lib)
        list(APPEND LIBOPENSSL_LIBS ${WINDOWS_EXTERNAL_DIR}/openssl/lib64_v140/libcrypto.lib)
      endif(CMAKE_GENERATOR MATCHES "Visual Studio 11 2012")
    endif(TARGET_PLATFORM_BITS EQUAL 32) 
    list(APPEND LIBOPENSSL_LIBS Crypt32 Ws2_32)
    set(LIBOPENSSL_INCLUDES ${WINDOWS_EXTERNAL_DIR}/openssl/include)
  endif(UNIX)
endif()
list(APPEND EP_INCLUDES ${LIBOPENSSL_INCLUDES})
list(APPEND EP_LIBS ${LIBOPENSSL_LIBS})

# include subdirectories
add_subdirectory(cci)
add_subdirectory(tools)

# for packaging info
if(UNIX)
  set(CPACK_GENERATOR "STGZ;TGZ")
  set(CPACK_SOURCE_GENERATOR "TGZ")
else(UNIX)
  set(CPACK_GENERATOR "ZIP")
endif(UNIX)

set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL CCI)
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CUBRID Open Source RDBMS")
set(CPACK_PACKAGE_VENDOR "CUBRID Corporation")
set(CPACK_PACKAGE_FILE_NAME "${CUBRID_CCI_PACKAGE_NAME}")
set(CPACK_ARCHIVE_CCI_FILE_NAME "${CUBRID_CCI_PACKAGE_NAME}")


include(CPack)

endif(WITH_CCI)
