cmake_minimum_required(VERSION 3.28)

# 프로젝트 설정
project(NetLib)
# C++ 표준 설정
set(CMAKE_CXX_STANDARD 20)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(-translateInclude)
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG")
endif ()

# 유니코드 매크로 정의
add_compile_definitions(_UNICODE UNICODE)

# PDB 출력 경로 설정
set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/")

# 소스 파일 추가
add_library(NetLib STATIC
        src/CpuUsage.cpp
        src/Logger.cpp
        src/CoreGlobal.cpp
        src/CrashDump.cpp
        src/RecvBuffer.cpp
        src/RingBuffer.cpp
        src/SendBufferImpl.cpp
        src/DBConnection.cpp
        src/Group.cpp
        src/GroupExecutable.cpp
        src/GroupManager.cpp
        src/HardwareMonitor.cpp
        src/LockFreeStack.cpp
        src/LockPool.cpp
        src/LockStack.cpp
        src/MemoryPool.cpp
        src/MemoryUsage.cpp
        src/MultiThreadMemoryPool.cpp
        src/NormalIOCP.cpp
        src/NormalSocket.cpp
        src/PostSendExecutable.cpp
        src/ProcessMonitor.cpp
        src/RecvExecutable.cpp
        src/RedisConnection.cpp
        src/ReleaseExecutable.cpp
        src/RUDPSocket.cpp
        src/SendBuffer.cpp
        src/SendExecutable.cpp
        src/Session.cpp
        src/SettingParser.cpp
        src/SingleThreadObjectPool.cpp
        src/SpinLock.cpp
        src/SpinLockPool.cpp
        src/SubnetManager.cpp
        src/TLSLockFreeQueue.cpp
        src/TLSLockFreeQueue2.cpp
        src/TLSLockFreeStack.cpp
        src/TLSPool.cpp
        src/Utility.cpp
        src/WebHook.cpp
        src/stdafx.cpp
        ThreadPool.cpp
        ThreadPool.h
        "include/IOCP.h"
        "include/Container.h")

# 빌드 타입에 따른 매크로 정의
if (CMAKE_CONFIGURATION_TYPES)
    target_compile_definitions(NetLib PRIVATE $<$<CONFIG:Debug>:DEBUG> $<$<CONFIG:Release>:RELEASE>)
else ()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(NetLib PRIVATE DEBUG)
    elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(NetLib PRIVATE RELEASE)
    endif ()
endif ()

# 라이브러리 링크 설정 (공용)
target_link_libraries(NetLib PUBLIC
        ws2_32
        wsock32
        Winmm
        Synchronization
        Dbghelp
        Psapi
        kernel32
)

# Profiling is optional and its public macros must match consumers.
option(NETLIB_ENABLE_TRACY "Enable Tracy profiler integration" OFF)
if(NETLIB_ENABLE_TRACY)
    find_package(Tracy CONFIG REQUIRED)
    target_compile_definitions(NetLib PUBLIC NETLIB_ENABLE_TRACY)
    target_link_libraries(NetLib PUBLIC Tracy::TracyClient)
endif()

find_package(fmt CONFIG REQUIRED)
target_link_libraries(NetLib PUBLIC fmt::fmt)
find_package(CURL REQUIRED)
find_package(Iconv REQUIRED)
find_path(TOMLPLUSPLUS_INCLUDE_DIR NAMES toml++/toml.hpp REQUIRED)
target_include_directories(NetLib PRIVATE ${TOMLPLUSPLUS_INCLUDE_DIR})
target_compile_definitions(NetLib PRIVATE TOML_HEADER_ONLY=1)
find_package(unofficial-libmysql CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
target_link_libraries(NetLib PRIVATE Iconv::Iconv CURL::libcurl unofficial::libmysql::libmysql redis++::redis++)

# 외부 패키지 링크 설정s

# 인클루드 디렉토리 설정
target_include_directories(NetLib PUBLIC include)
target_include_directories(NetLib PUBLIC include/externalHeader)

# 출력 파일 이름과 경로 설정
set_target_properties(NetLib PROPERTIES
        OUTPUT_NAME_DEBUG NetLib_d
        OUTPUT_NAME_RELEASE NetLib
        COMPILE_PDB_NAME "NetLib"
        LINK_PDB_NAME "NetLib"
)

# 테스트 설정
#enable_testing()
#add_subdirectory(tests)
