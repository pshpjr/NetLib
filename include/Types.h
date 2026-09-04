#pragma once
#include <functional>
#include <string>
#include <memory>
#include "Utility.h"

namespace psh
{

    // Windows API types
    using LPWSTR = std::wstring_view;
    using LPCWSTR = const std::wstring_view;
    using LPWCH = wchar_t*;
    using LPSTR = std::string_view;
    using LPCSTR = const std::string_view;
    using WCHAR = wchar_t;


    using HANDLE = void*;
    using HWND = HANDLE;
    using HINSTANCE = HANDLE;
    using HMODULE = HANDLE;
    using HDC = HANDLE;
    using HBITMAP = HANDLE;
    using HBRUSH = HANDLE;
    using HCURSOR = HANDLE;
    using HICON = HANDLE;
    using HMENU = HANDLE;
    using HMONITOR = HANDLE;



    // Fixed-width integer types
    using int8 = std::int8_t;
    using uint8 = std::uint8_t;
    using int16 = std::int16_t;
    using uint16 = std::uint16_t;
    using int32 = std::int32_t;
    using uint32 = std::uint32_t;
    using int64 = std::int64_t;
    using uint64 = std::uint64_t;

    // Windows-specific integral types
    using DWORD = uint32;
    using WORD = psh::uint16;
    using BYTE = uint8;
    using BOOL = psh::int32; // Note: BOOL is typically defined as int in Windows API
    using LONG = psh::int32;
    using LONGLONG = psh::int64;
    using ULONGLONG = uint64;
    using UINT = uint32;
    using UINT_PTR = uintptr_t;
    using ULONG = uint32;

    // Pointer types
    using LPVOID = void*;
    using LPCVOID = const void*;
    using LPLONG = LONG*;
    using LPULONG = ULONG*;
    using LPBOOL = BOOL*;
    using LPUINT = UINT*;

    using LPVOID = void*;
}


template <typename T> using  PoolPtr = std::unique_ptr<T,std::function<void(T*)>>;


using String = std::wstring;
using StringView = std::wstring_view;

template <typename T>
using WSAResult = psh::util::Result<T, int>;

