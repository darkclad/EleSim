cmake_minimum_required(VERSION 3.20)
project(EleSim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# For fully static builds: force all library lookups to find .a instead
# of .dll.a, and append transitive dependencies at the end of the link
# line in a group (handles circular deps like harfbuzz <-> freetype).
if(CMAKE_EXE_LINKER_FLAGS MATCHES "-static")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(CMAKE_IMPORT_LIBRARY_SUFFIX ".a")
    set(CMAKE_CXX_STANDARD_LIBRARIES
        "${CMAKE_CXX_STANDARD_LIBRARIES} -Wl,--start-group -lharfbuzz -lfreetype -lbz2 -lbrotlidec -lbrotlicommon -lgraphite2 -lglib-2.0 -lintl -liconv -lpng -lz -ljbig -ljpeg -ltiff -llzma -lzstd -ldeflate -lLerc -lwebp -lwebpdemux -lwebpmux -lsharpyuv -lpcre2-16 -lb2 -lrpcrt4 -lusp10 -Wl,--end-group")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets SvgWidgets Concurrent)

# Eigen (header-only) - try system install first, fallback to bundled
find_package(Eigen3 QUIET)
if(NOT Eigen3_FOUND)
    add_library(Eigen3::Eigen INTERFACE IMPORTED)
    set_target_properties(Eigen3::Eigen PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/third_party/eigen"
    )
endif()

add_subdirectory(src)

# Tests (optional)
option(ELESIM_BUILD_TESTS "Build unit tests" OFF)
if(ELESIM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
