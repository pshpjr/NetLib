#include "Logger.h"

#include <clocale>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <fmt/xchar.h>
#include "stdafx.h"


Logger::Logger(std::wstring_view fileName, LogLevel level)
{
    _extraFileName = fileName;
    _logLevel = level;
    setlocale(LC_ALL, "");
}

void Logger::WriteInternal(const std::wstring_view type, const LogLevel level, const std::wstring_view message)
{
    const time_t timer = time(nullptr);
    tm t{};
#ifdef _WIN32
    localtime_s(&t, &timer);
#else
    localtime_r(&timer, &t);
#endif

    const auto fileName = fmt::format(L"{}-{}-{}_{}_{}.txt",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, _extraFileName, type);

    std::ofstream stream(std::filesystem::path(fileName), std::ios::app | std::ios::binary);
    if (!stream)
    {
        printf("Cannot Open File!\n");
        return;
    }

    stream << psh::util::WToS(fmt::format(L"[{}] [{}-{}-{} {}:{}:{} / {}]\n {}\n",
        type,
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
        t.tm_hour, t.tm_min, t.tm_sec,
        _dic[static_cast<int>(level)],
        message));
}
