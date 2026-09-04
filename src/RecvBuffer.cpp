#include "RecvBuffer.h"
#include <stdexcept>
#include "Macro.h"
#include "RingBuffer.h"


constexpr int RECV_INIT_SIZE = 2000;
constexpr int RECV_INIT_MULTIPLIER = 3;


//TlsPool<RecvBuffer, 0> RecvBuffer::_pool(RECV_INIT_SIZE, RECV_INIT_MULTIPLIER);

RecvBuffer& RecvBuffer::operator>>(String& value)
{
    psh::uint16 len = 0;
    operator >>(len);

    const unsigned int popBytes = len * sizeof(psh::WCHAR);
    CanPop(popBytes);
    value.resize(len);

    _buffer->Peek(reinterpret_cast<char*>(value.data()), popBytes);
    _buffer->Dequeue(popBytes);

    _size -= popBytes;
    return *this;
}

void RecvBuffer::Clear()
{
    _buffer = nullptr;
    _size = 0;
}


int RecvBuffer::CanPopSize(void) const
{
    return _size;
}


void RecvBuffer::GetWstr(const psh::LPWSTR arr, const int strLen)
{
    CanPop(strLen);
    _buffer->Peek(reinterpret_cast<char*>(arr), strLen * sizeof(psh::WCHAR));
    _buffer->Dequeue(strLen * sizeof(psh::WCHAR));

    _size -= strLen * sizeof(psh::WCHAR);
}

void RecvBuffer::GetCstr(const psh::LPSTR arr, const int size)
{
    CanPop(size);
    _buffer->Peek(arr, size);
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
