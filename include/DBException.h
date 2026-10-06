#pragma once
#include <chrono>
#include <exception>
#include <utility>

class DBErr : public std::exception
{
public:
    DBErr(std::string errStr, const unsigned int errNo, const std::chrono::milliseconds dur, std::string query)
        : _query{std::move(query)}
        , _errString(std::format("ErrNo : {} Err : {} Query : {}, duration : {}", errNo, errStr, _query, dur.count()))
        , _errNo(errNo)
        , _dur(dur) {}

    [[nodiscard]] const char* what() const noexcept override
    {
        return _errString.c_str();
    }

    std::string& getErrStr()
    {
        return _errString;
    }

    [[nodiscard]] unsigned int getErrCode() const
    {
        return _errNo;
    }

private:
    std::string _query;
    std::string _errString;
    unsigned int _errNo = 0;
    std::chrono::milliseconds _dur;

};
