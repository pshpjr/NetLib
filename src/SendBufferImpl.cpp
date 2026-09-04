#include "SendBufferImpl.h"
#include "stdafx.h"
#include <mutex>


TlsPool<SendBufferImpl, 0> SendBufferImpl::_pool(SERIAL_INIT_SIZE, SERIAL_INIT_MULTIPLIER);

void SendBufferImpl::WriteLanHeader()
{
    *(reinterpret_cast<psh::uint16*>(_front) - 1) = GetDataSize();
    _head = (char*)(reinterpret_cast<psh::uint16*>(_front) - 1);
}

void SendBufferImpl::WriteNetHeader(const psh::uint8 code) const
{
    const char* checksumIndex = GetDataPtr();

    NetHeader tmpHeader;
    tmpHeader.len = GetDataSize();
    const int checkLen = tmpHeader.len;
    unsigned char checkSum = 0;

    for (int i = 0; i < checkLen; i++)
    {
        checkSum += checksumIndex[i];
    }

    tmpHeader.checkSum = checkSum;
    tmpHeader.code = code;
    tmpHeader.randomKey = 0x31;
    //head->randomKey = rand()%256;

    *reinterpret_cast<NetHeader*>(GetHead()) = tmpHeader;
}

void SendBufferImpl::TryEncode(const char staticKey)
{
    if (!isEncrypt)
    {
        std::lock_guard lock(_encodeLock);
        if (!isEncrypt)
        {
            WriteNetHeader(dfPACKET_CODE);

            Encode(staticKey);
        }
    }
}

SendBufferImpl::SendBufferImpl()
    : _data(_buffer + sizeof(NetHeader))
    , _head(_buffer)
    , _front(_data)
    , _rear(_data)
{
}

bool SendBufferImpl::CopyData(SendBufferImpl& dst) const
{
    memcpy_s(dst.GetDataPtr(), dst.CanPushSize(), GetFront(), CanPopSize());

    dst.MoveWritePos(CanPopSize());

    return true;
}

void SendBufferImpl::Encode(const char staticKey)
{
    using Header = NetHeader;

    const auto head = (Header*)GetHead();
    const unsigned char randomKey = head->randomKey;

    unsigned char* encodeData = &head->checkSum;
    const int encodeLen = head->len + 1;

    char p = 0;
    char e = 0;

    for (short i = 1; i <= encodeLen; i++)
    {
        p = *encodeData ^ p + randomKey + i;
        e = p ^ (e + staticKey + i);
        *encodeData = e;
        encodeData++;
    }

    isEncrypt++;
}


SendBufferImpl& SendBufferImpl::operator<<(const LPWSTR value)
{
    //insert null terminated string to buffer
    const size_t strlen = std::wcslen(value) + 1;
    CanPush(strlen);
    wcscpy_s(reinterpret_cast<wchar_t*>(_rear), strlen, value);
    _rear += strlen * sizeof(WCHAR);
    return *this;
}

SendBufferImpl& SendBufferImpl::operator<<(const psh::LPCWSTR value)
{
    //insert null terminated string to buffer
    const size_t strlen = std::wcslen(value.data()) + 1;
    SetWstr(value, strlen);
    return *this;
}

SendBufferImpl& SendBufferImpl::operator<<(const String& value)
{
    operator<<(static_cast<psh::uint16>(value.length()));
    SetWstr(value.c_str(), value.length());

    return *this;
}

void SendBufferImpl::SetWstr(const psh::LPCWSTR arr, const int size)
{
    CanPush(size * sizeof(wchar_t));
    std::memcpy(_rear, arr.data(), size);
    _rear += size * sizeof(WCHAR);
}

void SendBufferImpl::SetCstr(const psh::LPCSTR arr, const int size)
{
    CanPush(size);
    std::memcpy(_rear, arr.data(), size);
    _rear += size;
}
