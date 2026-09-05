#pragma once
#include <chrono>
#include <fmt/printf.h>
#include "Types.h"

class DBConnection
{
public:
    DBConnection(psh::LPCSTR ip, psh::uint32 port, psh::LPCSTR id, psh::LPCSTR pass, psh::LPCSTR db);

    static void LibInit();

    ~DBConnection();


    template <typename... Args>
    std::chrono::milliseconds Query(const psh::LPCSTR format, Args... args)
    {
        return QueryInternal(fmt::vsprintf(
            fmt::string_view(format.data(), format.size()),
            fmt::make_printf_args(args...)));
    }

    bool next();

    [[nodiscard]] char* getString(int index) const;

    [[nodiscard]] char getChar(int index) const;

    [[nodiscard]] int getInt(int index) const;

    [[nodiscard]] float getFloat(int index) const;

    [[nodiscard]] double getDouble(int index) const;

    void reset();

    void Close();

    void SetReconnect(bool useReconnect);

private:
    std::chrono::milliseconds QueryInternal(std::string_view query);

    struct Imple;
    std::unique_ptr<Imple> pImple;
};
