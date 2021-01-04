cmake_minimum_required(VERSION 3.8)

# set the project name
project(Liquid VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g")

FILE(GLOB CPPSources src/*.cpp)

add_library( liquid ${CPPSources})
target_link_libraries( liquid )

FILE(GLOB HSources src/*.h)
include(GNUInstallDirs)

add_definitions(-DLIQUID_INCLUDE_WEB_DIALECT -DLIQUID_INCLUDE_RAPIDJSON_VARIABLE)

install(TARGETS liquid DESTINATION lib)
install(FILES ${HSources} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/liquid)
