#pragma once
#include "SendBufferImpl.h"

class SendBuffer
{
public:
    SendBuffer(SendBufferImpl& other);

    ~SendBuffer();

    SendBuffer(SendBuffer&& other) noexcept;

    SendBuffer(const SendBuffer& other);

    friend void swap(SendBuffer& fst, SendBuffer& snd) noexcept;


    SendBuffer& operator=(SendBuffer other);


    static SendBuffer Alloc();

    int Size() const;

    SendBufferImpl* getBuffer() const;

    template<typename T>
    SendBuffer & operator<<(const T &value)
    {
        *buffer << value;
        return *this;
    }
    void SetWstr(psh::LPCWSTR arr, int size);

    void SetCstr(LPCSTR arr, int size);

    int CanPushSize();


private:
    SendBufferImpl* buffer;
};
