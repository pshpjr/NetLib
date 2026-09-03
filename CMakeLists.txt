cmake_minimum_required(VERSION 3.28)

# 프로젝트 설정
project(NetLib)
# C++ 표준 설정
set(CMAKE_CXX_STANDARD 20)
#
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
file(GLOB PUB_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/lib/*.lib")
foreach (lib ${PUB_LIBS})
    target_link_libraries(NetLib INTERFACE ${lib})
endforeach ()


# 빌드 타입에 따른 외부 라이브러리 링크
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB EXTERNAL_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/lib/debug/*.lib")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    file(GLOB EXTERNAL_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/lib/release/*.lib")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    file(GLOB EXTERNAL_LIBS "${CMAKE_CURRENT_SOURCE_DIR}/lib/release/*.lib")
endif ()

foreach (lib ${EXTERNAL_LIBS})
    target_link_libraries(NetLib INTERFACE ${lib})
endforeach ()

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
