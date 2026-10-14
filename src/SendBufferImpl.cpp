#include "SendBufferImpl.h"
#include "stdafx.h"
#include "Utf16.h"

#include <cstring>
#include <cwchar>
#include <limits>


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
    return operator<<(static_cast<const wchar_t*>(value));
}

SendBufferImpl& SendBufferImpl::operator<<(const wchar_t* value)
{
    if (value == nullptr)
        throw std::invalid_argument("Null wide string");
    auto encoded = psh::util::WideToUtf16(value);
    encoded.push_back(u'\0');
    SetUtf16(encoded);
    return *this;
}

SendBufferImpl& SendBufferImpl::operator<<(wchar_t value)
{
    const auto encoded = psh::util::WideToUtf16(std::wstring_view(&value, 1));
    if (encoded.size() != 1)
        throw std::invalid_argument("Wide character requires two UTF-16 code units");
    SetUtf16(encoded);
    return *this;
}

SendBufferImpl& SendBufferImpl::operator<<(const psh::LPCWSTR value)
{
    //insert null terminated string to buffer
    auto encoded = psh::util::WideToUtf16(value);
    encoded.push_back(u'\0');
    SetUtf16(encoded);
    return *this;
}

SendBufferImpl& SendBufferImpl::operator<<(const String& value)
{
    const auto encoded = psh::util::WideToUtf16(value);
    if (encoded.size() > std::numeric_limits<psh::uint16>::max())
        throw std::invalid_argument("UTF-16 string is too long");
    CanPush(sizeof(psh::uint16) + encoded.size() * 2);
    operator<<(static_cast<psh::uint16>(encoded.size()));
    SetUtf16(encoded);

    return *this;
}

void SendBufferImpl::SetWstr(const psh::LPCWSTR arr, const int size)
{
    if (size < 0 || static_cast<std::size_t>(size) > arr.size())
        throw std::invalid_argument("Invalid wide string length");
    SetUtf16(psh::util::WideToUtf16(arr.substr(0, size)));
}

void SendBufferImpl::SetWstr(const wchar_t* arr, int size)
{
    if (arr == nullptr || size < 0)
        throw std::invalid_argument("Invalid wide string buffer");
    SetWstr(std::wstring_view(arr, size), size);
}

void SendBufferImpl::SetUtf16(std::u16string_view value)
{
    CanPush(value.size() * 2);
    psh::util::WriteUtf16LE(_rear, value);
    _rear += value.size() * 2;
}

void SendBufferImpl::SetCstr(const psh::LPCSTR arr, const int size)
{
    CanPush(size);
    std::memcpy(_rear, arr.data(), size);
    _rear += size;
}
