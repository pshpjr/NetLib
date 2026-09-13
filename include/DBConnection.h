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
    std::chrono::microseconds QueryFormat(std::string_view query, Args&&... args);

    template <typename... Args>
    std::chrono::microseconds Query(std::string_view query, const Args&... args)
    {
        return ExecuteQuery(fmt::sprintf(fmt::string_view(query.data(), query.size()), args...));
    }

    //저장 프로시저 호출용
    template<typename... Args>
    std::chrono::microseconds CallFormat(psh::LPCSTR func, Args&&... args );

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
    std::chrono::microseconds ExecuteQuery(const std::string& queryString);

    struct Imple;
    std::unique_ptr<Imple> pImple;
};

template <typename ... Args>
std::chrono::microseconds DBConnection::QueryFormat(std::string_view query, Args&&... args)
{
    auto formatArgs = std::make_format_args(std::forward<Args>(args)...);
    std::string queryString = std::vformat(query, formatArgs);

    return ExecuteQuery(queryString);
}

template <typename ... Args>
std::chrono::microseconds DBConnection::CallFormat(psh::LPCSTR func, Args&&... args)
{
    throw std::runtime_error("DBConnection::CallFormat: Not Implemented");
}
