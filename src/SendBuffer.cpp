#include "SendBuffer.h"
#include "SendBufferImpl.h"


SendBuffer SendBuffer::Alloc()
{
    return SendBuffer(*SendBufferImpl::Alloc());
}

SendBuffer::SendBuffer(SendBufferImpl &other): buffer(&other)
{

}

SendBuffer::SendBuffer(SendBuffer &&other) noexcept: buffer{nullptr}
{
    swap(*this,other);
}

SendBuffer::SendBuffer(const SendBuffer &other): buffer{other.buffer}
{
    buffer->IncreaseRef(L"BufferCopy");
}

void swap(SendBuffer &fst, SendBuffer &snd) noexcept
{
    std::swap(fst.buffer, snd.buffer);
}

SendBuffer & SendBuffer::operator=(SendBuffer other)
{
    //buf = Alloc(); 코드에서 other는 prvalue가 되고, 이동 생성과 소멸자 호출됨.
    //buf와 other의 버퍼가 교환된 후 other 소멸하며 release 함.
    swap(*this, other);
    return *this;
}

int SendBuffer::Size() const
{
    return buffer->GetDataSize();
}

SendBufferImpl * SendBuffer::getBuffer() const
{
    return buffer;
}

void SendBuffer::SetWstr(psh::LPCWSTR arr, int size)
{
    buffer->SetWstr(arr, size);
}

void SendBuffer::SetCstr(psh::LPCSTR arr, int size)
{
    buffer->SetCstr(arr, size);
}

int SendBuffer::CanPushSize()
{
    return buffer->CanPushSize();
}

SendBuffer::~SendBuffer()
{
    if (buffer != nullptr)
    {
        buffer->Release();
    }
}
