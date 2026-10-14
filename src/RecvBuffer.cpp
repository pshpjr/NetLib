#include "RecvBuffer.h"
#include <stdexcept>
#include "Macro.h"
#include "RingBuffer.h"
#include "Utf16.h"
#include <algorithm>


constexpr int RECV_INIT_SIZE = 2000;
constexpr int RECV_INIT_MULTIPLIER = 3;


//TlsPool<RecvBuffer, 0> RecvBuffer::_pool(RECV_INIT_SIZE, RECV_INIT_MULTIPLIER);

RecvBuffer& RecvBuffer::operator>>(String& value)
{
    psh::uint16 len = 0;
    operator >>(len);

    value = ReadWstr(len);
    return *this;
}

RecvBuffer& RecvBuffer::operator>>(wchar_t& value)
{
    GetWstr(&value, 1);
    return *this;
}

void RecvBuffer::Clear()
{
    _buffer = nullptr;
    _size = 0;
}


int RecvBuffer::CanPopSize() const
{
    return _size;
}


void RecvBuffer::GetWstr(wchar_t *buffer, int strLen)
{
    if (buffer == nullptr || strLen < 0)
        throw std::invalid_argument("Invalid wide string buffer");
    const auto decoded = ReadWstr(strLen);
    std::fill_n(buffer, strLen, L'\0');
    std::copy(decoded.begin(), decoded.end(), buffer);
}

std::wstring RecvBuffer::ReadWstr(int codeUnits)
{
    if (codeUnits < 0)
        throw std::invalid_argument("Invalid UTF-16 length");
    const auto bytes = static_cast<std::size_t>(codeUnits) * 2;
    CanPop(bytes);
    std::string encoded(bytes, '\0');
    _buffer->Peek(encoded.data(), static_cast<unsigned int>(bytes));
    auto decoded = psh::util::Utf16ToWide(psh::util::ReadUtf16LE(encoded));
    _buffer->Dequeue(static_cast<int>(bytes));
    _size -= static_cast<psh::int32>(bytes);
    return decoded;
}

void RecvBuffer::GetCstr(char *buffer, int size)
{
    CanPop(size);
    _buffer->Peek(buffer, size);
    _buffer->Dequeue(size);
    _size -= size;
}

RecvBuffer::RecvBuffer(RingBuffer *buffer, const psh::int32 size): _buffer(buffer)
                                                             , _size(size)
{
}

void RecvBuffer::CanPop(const psh::uint64 popByte) const
{
    if (CanPopSize() < popByte)
    {
        ASSERT_CRASH(false, "CannotPop");
        throw std::invalid_argument{"size is bigger than data in buffer"};
    }
}
