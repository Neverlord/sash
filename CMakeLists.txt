cmake_minimum_required(VERSION 2.8)
project(sash C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# extract version from VERSION file
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SASH_VERSION LIMIT_COUNT 1)
string(REPLACE "." " " version_numbers ${SASH_VERSION})
separate_arguments(version_numbers)
list(GET version_numbers 0 SASH_VERSION_MAJOR)
list(GET version_numbers 1 SASH_VERSION_MINOR)
list(GET version_numbers 2 SASH_VERSION_PATCH)

# prohibit in-source builds
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR "In-source builds are not allowed. Please use "
                        "./configure to choose a build directory and "
                        "initialize the build configuration.")
endif ()

# process cmake_uninstall.cmake.in
configure_file("${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
               "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
               IMMEDIATE @ONLY)

# add uninstall target
add_custom_target(uninstall
                  COMMAND ${CMAKE_COMMAND} -P
                  ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

# set binary output path
if ("${EXECUTABLE_OUTPUT_PATH}" STREQUAL "")
  set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
endif ()

# set library output path to binary output path for Xcode projects
if ("${CMAKE_GENERATOR}" STREQUAL "Xcode")
  set(LIBRARY_OUTPUT_PATH "${EXECUTABLE_OUTPUT_PATH}")
endif ()

# check for g++ >= 4.7 or clang++ > = 3.2
try_run(ProgramResult
        CompilationSucceeded
  ${CMAKE_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/get_compiler_version.cpp
        RUN_OUTPUT_VARIABLE CompilerVersion)
if (NOT CompilationSucceeded OR NOT ProgramResult EQUAL 0)
  message(FATAL_ERROR "Cannot determine compiler version")
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
  if (CompilerVersion VERSION_GREATER 4.6)
    message(STATUS "Found g++ version ${CompilerVersion}")
  else ()
    message(FATAL_ERROR "g++ >= 4.7 required (found: ${CompilerVersion}.")
  endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  if (CompilerVersion VERSION_GREATER 3.1)
    message(STATUS "Found clang++ version ${CompilerVersion}")
  else ()
    message(FATAL_ERROR "clang++ >= 3.2 required (found: ${CompilerVersion}.")
  endif ()
  if (NOT CXXFLAGS_PROVIDED)
    #message(STATUS "NOTE: Automatically added -stdlib=libc++ flag, "
    #               "you can override this by defining CMAKE_CXX_FLAGS "
    #               "(see 'configure --help')")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
  endif ()
  if (ENABLE_ADDRESS_SANITIZER)
    set(CXX_FLAGS "${CXX_FLAGS} -fsanitize=address -g")
  endif ()
else ()
  message(FATAL_ERROR "Your C++ compiler does not support C++11 "
                      "or is not supported")
endif ()

# check if the user provided CXXFLAGS on the command line
if (CMAKE_CXX_FLAGS)
  set(CXXFLAGS_PROVIDED true)
  set(CMAKE_CXX_FLAGS_DEBUG          "")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "")
  set(CMAKE_CXX_FLAGS_RELEASE        "")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
else ()
  set(CXXFLAGS_PROVIDED false)
  if (MORE_CLANG_WARNINGS AND "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-std=c++11 -pedantic -Weverything -Wno-c++98-compat -Wno-padded -Wno-documentation-unknown-command -Wno-exit-time-destructors -Wno-global-constructors -Wno-missing-prototypes -Wno-c++98-compat-pedantic -Wno-unused-member-function -Wno-unused-const-variable")
  else ()
    set(CMAKE_CXX_FLAGS "-std=c++11 -Wextra -Wall -pedantic")
  endif ()
  if (MINGW)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32")
    include (GenerateExportHeader)
  endif(MINGW)
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
endif (CMAKE_CXX_FLAGS)

if (ENABLE_DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
  set(CXX_FLAGS "${CXX_FLAGS} -DSASH_DEBUG")
endif ()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif ()

# get header files; only needed by CMake generators,
# e.g., for creating proper Xcode projects
file(GLOB SASH_HDRS "${CMAKE_SOURCE_DIR}/sash/*.hpp")

set(SASH_INCLUDE_DIRS .)
set(SASH_LD_DIRS)
set(SASH_LD_FLAGS ${CMAKE_LD_LIBS})

if (APPLE)
  # -pthread is ignored on MacOSX
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
elseif (UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
elseif (MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
  set(SASH_LD_FLAGS "ws2_32 -liphlpapi")
endif ()

include_directories(${SASH_INCLUDE_DIRS})
link_directories(${SASH_LD_DIRS})

set(sash_libraries ${CMAKE_DL_LIBS} ${PTHREAD_LIBRARIES})

find_package(Editline)
if (EDITLINE_FOUND)
  if (EDITLINE_VERSION_MAJOR LESS 2 OR EDITLINE_VERSION_MINOR LESS 11)
    message(FATAL_ERROR "libedit too old: ${EDITLINE_VERSION}, need >= 2.11")
  endif ()
  set(SASH_HAVE_EDITLINE true)
  set(sash_libraries ${sash_libraries} ${EDITLINE_LIBRARIES})
  include_directories(${EDITLINE_INCLUDE_DIR})
endif ()

# check for doxygen and add custom "doc" target to Makefile
find_package(Doxygen)
if (DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
                 ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
                 @ONLY)
  add_custom_target(doc ${DOXYGEN_EXECUTABLE} ${CMAKE_HOME_DIRECTORY}/Doxyfile
                    WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}
                    COMMENT "Generating API documentation with Doxygen"
                    VERBATIM)
endif (DOXYGEN_FOUND)

add_custom_target(all_examples)

macro(add name)
  add_executable(${name} examples/${name}.cpp ${ARGN} ${SASH_HDRS})
  if (${CMAKE_SYSTEM_NAME} MATCHES "Window")
    target_link_libraries(${name} ${sash_libraries} -lws2_32)
  else ()
    target_link_libraries(${name} ${sash_libraries})
  endif ()
  add_dependencies(${name} all_examples)
endmacro()

add(simple_shell)

# install includes
install(DIRECTORY sash/ DESTINATION include/sash FILES_MATCHING PATTERN "*.hpp")

macro(display test desc summary)
  if (${test})
    set(${summary} ${desc})
  else ()
    set(${summary} no)
  endif()
endmacro()

display(ENABLE_DEBUG yes debug_summary)
display(DOXYGEN_FOUND yes doxygen_summary)
display(EDITLINE_FOUND ${EDITLINE_INCLUDE_DIR} editline_summary)
display(ENABLE_ADDRESS_SANITIZER yes asan_summary)

message(
  "====================|  Build Summary  |===================="
  "\n"
  "\nVersion:              ${SASH_VERSION}"
  "\n"
  "\nDebug mode:           ${debug_summary}"
  "\nBuild type:           ${CMAKE_BUILD_TYPE}"
  "\nSource directory:     ${CMAKE_SOURCE_DIR}"
  "\nBuild directory:      ${CMAKE_BINARY_DIR}"
  "\nInstall prefix:       ${CMAKE_INSTALL_PREFIX}"
  "\nExecutable path:      ${EXECUTABLE_OUTPUT_PATH}"
  "\n"
  "\nDoxygen:              ${doxygen_summary}"
  "\nEditline:             ${editline_summary}"
  "\n"
  "\nUse AddressSanitizer: ${asan_summary}"
  "\n"
  "\nCC:                   ${CMAKE_C_COMPILER}"
  "\nCXX:                  ${CMAKE_CXX_COMPILER}"
  "\nCXXFLAGS:             ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${build_type}}"
  "\n"
  "\n===========================================================\n")
