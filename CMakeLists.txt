cmake_minimum_required(VERSION 3.28)
project(NetLib)

#cmake 관련 속성 설정
set(CMAKE_CXX_STANDARD 20)

add_library(NetLib INTERFACE)
target_sources(NetLib INTERFACE
        src/Utility.cpp
        src/CCpuUsage.cpp
        src/CLogger.cpp
        src/CoreGlobal.cpp
        src/CrashDump.cpp
        src/CRecvBuffer.cpp
        src/CRingBuffer.cpp
        src/CSendBuffer.cpp
        src/DBConnection.cpp
        src/Group.cpp
        src/GroupExecutable.cpp
        src/GroupManager.cpp
        src/HardwareMonitor.cpp
        src/LockFreeStack.cpp
        src/LockGuard.cpp
        src/LockPool.cpp
        src/LockStack.cpp
        src/MemoryPool.cpp
        src/MemoryUsage.cpp
        src/MultiThreadMemoryPool.cpp
        src/NormalIOCP.cpp
        src/NormalSocket.cpp
        src/PostSendExecutable.cpp
        src/ProcessMonitor.cpp
        src/Profiler.cpp
        src/RecvExecutable.cpp
        src/RedisConnection.cpp
        src/ReleaseExecutable.cpp
        src/RUDPSocket.cpp
        src/SendBuffer.cpp
        src/SendExecutable.cpp
        src/Session.cpp
        src/SettingParser.cpp
        src/SingleThreadObjectPool.cpp
        src/SingleThreadQ.cpp
        src/SpinLock.cpp
        src/SpinLockPool.cpp
        src/stdafx.cpp
        src/SubnetManager.cpp
        src/TLSLockFreeQueue.cpp
        src/TLSLockFreeQueue2.cpp
        src/TLSLockFreeStack.cpp
        src/TLSPool.cpp
        src/WebHook.cpp
        include/GroupTypes.h
        include/SocketTypes.h
)

#define 추가
#디버그 매크로 설정
# 디버그 및 릴리스 매크로 설정
target_compile_features(NetLib INTERFACE cxx_std_20)
target_compile_definitions(NetLib INTERFACE _UNICODE UNICODE)
if(MSVC)
    target_compile_options(NetLib INTERFACE -translateInclude /utf-8)
endif()

if (CMAKE_CONFIGURATION_TYPES)  # Multi-config generators
    target_compile_definitions(NetLib INTERFACE $<$<CONFIG:Debug>:DEBUG> $<$<CONFIG:Release>:RELEASE>)
else()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(NetLib INTERFACE DEBUG)
    elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_definitions(NetLib INTERFACE RELEASE)
    endif()
endif()


#인클루드 설정

target_include_directories(NetLib INTERFACE include include/externalHeader)

# 플랫폼별 시스템 라이브러리
if(WIN32)
    target_link_libraries(NetLib INTERFACE Ws2_32 Dbghelp Kernel32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # MemoryUsage는 /proc를 사용하므로 별도 시스템 라이브러리가 필요하지 않습니다.
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()


find_package(CURL REQUIRED)
find_package(Iconv REQUIRED)
find_package(unofficial-libmysql CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
target_link_libraries(NetLib INTERFACE
        CURL::libcurl
        Iconv::Iconv
        unofficial::libmysql::libmysql
)
target_link_libraries(NetLib INTERFACE redis++::redis++)

option(NETLIB_BUILD_TESTS "Build NetLib tests" ${PROJECT_IS_TOP_LEVEL})
if(NETLIB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
