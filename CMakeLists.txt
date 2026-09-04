cmake_minimum_required(VERSION 3.28)

project(NetLib LANGUAGES CXX)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 20)

# 정적 라이브러리
file(GLOB_RECURSE NETLIB_FILES CONFIGURE_DEPENDS
    src/*.cpp
    include/*.h
    include/*.hpp
)
add_library(NetLib STATIC ${NETLIB_FILES}
        src/ThreadMonitor.cpp
        include/ThreadMonitor.h)

# 컴파일 설정
target_compile_features(NetLib PUBLIC cxx_std_20)

target_compile_definitions(NetLib PUBLIC
    _UNICODE
    UNICODE
)

if(MSVC)
    target_compile_options(NetLib PRIVATE /utf-8)
endif()

# Debug / Release 매크로 설정
if(CMAKE_CONFIGURATION_TYPES)
    # Visual Studio 등의 Multi-config generator
    target_compile_definitions(NetLib PUBLIC
        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Release>:RELEASE>
    )
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(NetLib PUBLIC DEBUG)
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(NetLib PUBLIC RELEASE)
    endif()
endif()

# Include 경로
target_include_directories(NetLib PUBLIC
    include
    include/externalHeader
)

# 플랫폼별 시스템 라이브러리
if(WIN32)
    target_link_libraries(NetLib PRIVATE
        Ws2_32
        Dbghelp
        Kernel32
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # MemoryUsage는 /proc를 사용하므로 별도 시스템 라이브러리가 필요하지 않습니다.
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# 외부 라이브러리
find_package(CURL REQUIRED)
find_package(Iconv REQUIRED)
find_package(unofficial-libmysql CONFIG REQUIRED)
find_package(tomlplusplus CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)

target_link_libraries(NetLib PRIVATE
    CURL::libcurl
    Iconv::Iconv
    unofficial::libmysql::libmysql
    redis++::redis++
)

# 테스트
option(
    NETLIB_BUILD_TESTS
    "Build NetLib tests"
    ${PROJECT_IS_TOP_LEVEL}
)

if(NETLIB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
