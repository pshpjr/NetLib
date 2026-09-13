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
    mysql_init(&pImple->conn);

    pImple->connection = mysql_real_connect(&pImple->conn, ip, id, pass, db, port, nullptr, 0);
    if (pImple->connection == nullptr)
    {
        const char* err = nullptr;
        err = mysql_error(&pImple->conn);
        const auto num = mysql_errno(&pImple->conn);
        throw DBErr(err, num, std::chrono::milliseconds::zero(), "Error connecting to database");

        printf("%d \n", num);
    }

    SetReconnect(true);
}

void DBConnection::LibInit()
{
    mysql_library_init(0, nullptr, nullptr);
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


    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
}


bool DBConnection::next()
{
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
    mysql_free_result(pImple->sql_result);
}

void DBConnection::Close()
{
    mysql_close(pImple->connection);
}

void DBConnection::SetReconnect(const bool useReconnect)
{
    const bool reconnect = useReconnect;
    mysql_options(&pImple->conn, MYSQL_OPT_RECONNECT, &reconnect);
}
