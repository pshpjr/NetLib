#include "DBConnection.h"
#include "DBException.h"
#include "Macro.h"
#include <mysql/mysql.h>

struct DBConnection::Imple
{
    MYSQL conn{};
    MYSQL* connection = nullptr;
    MYSQL_RES* sql_result{};
    MYSQL_ROW sql_row{};
};

DBConnection::DBConnection(const psh::LPCSTR ip, const psh::uint32 port, const psh::LPCSTR id, const psh::LPCSTR pass
                           , const psh::LPCSTR db)
    : pImple(std::make_unique<Imple>())
{
    if (!mysql_init(&pImple->conn))
        throw std::runtime_error("MySQL initialization failed");

    const std::string host(ip), user(id), password(pass), database(db);
    pImple->connection = mysql_real_connect(&pImple->conn, host.c_str(), user.c_str(), password.c_str(), database.c_str(), port, nullptr, 0);
    if (pImple->connection == nullptr)
    {
        const char* err = nullptr;
        err = mysql_error(&pImple->conn);
        const auto num = mysql_errno(&pImple->conn);
        const std::string message(err);
        mysql_close(&pImple->conn);
        throw DBErr(message, num, std::chrono::milliseconds::zero(), "Error connecting to database");
    }

    if (mysql_set_character_set(pImple->connection, "utf8mb4") != 0)
    {
        const std::string message(mysql_error(pImple->connection));
        const auto number = mysql_errno(pImple->connection);
        Close();
        throw DBErr(message, number, {}, "Set character set");
    }
}

void DBConnection::LibInit()
{
    if (mysql_library_init(0, nullptr, nullptr) != 0)
        throw std::runtime_error("MySQL library initialization failed");
}

DBConnection::~DBConnection()
{
    Close();
}

/*
 * 가변인자는 va_list 방식으로 처리
 * mysql 라이브러리가 윈도우 헤더를 쓰기 때문에 헤더 꼬이는 경우가 종종 있었음.
 * cpp에 헤더 숨겨서 쓰기로 결정
 */

std::chrono::microseconds DBConnection::ExecuteQuery(const std::string& queryString)
{
    reset();
    const auto start = std::chrono::steady_clock::now();

    if (const int query_stat = mysql_real_query(pImple->connection, queryString.c_str()
                                                , static_cast<unsigned long>(queryString.length()));
        query_stat != 0)
    {
        const char* err = mysql_error(&pImple->conn);

        switch (const auto num = mysql_errno(&pImple->conn))
        {

        default:
            {
                const auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
                const std::string errStr = std::to_string(dur.count()) + err;
                throw DBErr(errStr, num, dur, queryString);
            }
        }
    }

    pImple->sql_result = mysql_store_result(pImple->connection);
    if (!pImple->sql_result && mysql_field_count(pImple->connection) != 0)
    {
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        throw DBErr(mysql_error(pImple->connection), mysql_errno(pImple->connection), duration, queryString);
    }


    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
}


bool DBConnection::next()
{
    if (!pImple->sql_result) return false;
    pImple->sql_row = mysql_fetch_row(pImple->sql_result);
    return pImple->sql_row != nullptr;
}

char* DBConnection::getString(const int index) const
{
    return pImple->sql_row[index];
}

char DBConnection::getChar(const int index) const
{
    auto value = getInt(index);
    ASSERT_CRASH(-128 <= value && value <= 127, L"NotCharRange");
    return static_cast<char>(value);
}

int DBConnection::getInt(const int index) const
{
    return std::stoi(pImple->sql_row[index]);
}

float DBConnection::getFloat(const int index) const
{
    return std::stof(pImple->sql_row[index]);
}

double DBConnection::getDouble(const int index) const
{
    return std::stod(pImple->sql_row[index]);
}

void DBConnection::reset()
{
    if (pImple->sql_result) mysql_free_result(pImple->sql_result);
    pImple->sql_result = nullptr;
    pImple->sql_row = nullptr;
}

void DBConnection::Close()
{
    reset();
    if (pImple->connection) mysql_close(pImple->connection);
    pImple->connection = nullptr;
}

void DBConnection::SetReconnect(const bool useReconnect)
{
    const bool reconnect = useReconnect;
    mysql_options(&pImple->conn, MYSQL_OPT_RECONNECT, &reconnect);
}
