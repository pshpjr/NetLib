#include "SendBufferImpl.h"
#include "stdafx.h"

#include <cstring>
#include <cwchar>


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
    const auto size = CanPopSize();
    if (size > dst.CanPushSize()) return false;
    if (this != &dst)
    {
        std::memcpy(dst.GetDataPtr(), GetFront(), size);
    }

    dst.MoveWritePos(size);

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


SendBufferImpl& SendBufferImpl::operator<<(wchar_t* const value)
{
    //insert null terminated string to buffer
    const size_t strlen = std::wcslen(value) + 1;
    CanPush(strlen * sizeof(wchar_t));
    std::memcpy(_rear, value, strlen * sizeof(wchar_t));
    _rear += strlen * sizeof(wchar_t);
    return *this;
}

SendBufferImpl& SendBufferImpl::operator<<(const psh::LPCWSTR value)
{
    //insert null terminated string to buffer
    SetWstr(value, static_cast<int>(value.size()));
    operator<<(wchar_t{});
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
    std::memcpy(_rear, arr.data(), size * sizeof(wchar_t));
    _rear += size * sizeof(wchar_t);
}

void SendBufferImpl::SetCstr(const psh::LPCSTR arr, const int size)
{
    CanPush(size);
    std::memcpy(_rear, arr.data(), size);
    _rear += size;
}
