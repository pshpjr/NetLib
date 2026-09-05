#include "Logger.h"

#include <clocale>
#include <time.h>
#include <fmt/xchar.h>
#include "stdafx.h"


Logger::Logger(psh::LPCWSTR fileName, LogLevel level)
{
    _extraFileName = fileName;
    _logLevel = level;
    setlocale(LC_ALL, "");
}

void Logger::WriteInternal(const std::wstring_view type, const LogLevel level, const std::wstring_view message)
{
    const time_t timer = time(nullptr);
    tm t;
    localtime_s(&t, &timer);

    const auto fileName = fmt::format(L"{}-{}-{}_{}_{}.txt",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, _extraFileName, type);

    FILE* stream = _wfsopen(fileName.c_str(), L"a+", _SH_DENYNO);
    if (stream == nullptr)
    {
        printf("Cannot Open File!\n");
        return;
    }

    fmt::print(stream, L"[{}] [{}-{}-{} {}:{}:{} / {}]\n {}\n",
        type,
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
        t.tm_hour, t.tm_min, t.tm_sec,
        _dic[static_cast<int>(level)],
        message);

    fclose(stream);
}
