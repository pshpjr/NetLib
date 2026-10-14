#pragma once
#include "RingBuffer.h"

class RecvBuffer
{

#ifdef SERIAL_DEBUG
	static const int debugSize = 1000;
#endif

    //USE_TLS_POOL(RecvBuffer);
    friend class IOCP;
    friend class Group;

public:
    RecvBuffer() = default;

    RecvBuffer(RingBuffer* buffer, psh::int32 size);

    ~RecvBuffer() = default;

    template <typename T>
    RecvBuffer& operator >>(T& value);

    RecvBuffer& operator >>(String& value);
    RecvBuffer& operator >>(wchar_t& value);

    // size는 UTF-16 코드 단위 수이며, buffer는 최소 size개의 wchar_t를 담아야 한다.
    void GetWstr(wchar_t *buffer, int size);
    void GetCstr(char *buffer, int size);


    void CanPop(psh::uint64 popByte) const;

    //	static RecvBuffer* Alloc(RingBuffer* buffer, psh::int32 size)
    //	{
    //		const auto ret = _pool.Alloc();
    //		ret->Clear();
    //		ret->IncreaseRef(L"AllocInc");
    //		ret->_buffer = buffer;
    //		ret->_size = size;
    //		return ret;
    //	}
    //
    //	long IncreaseRef(LPCWCH cause = L"")
    //	{
    //		auto refIncResult = ++_refCount;
    //
    //#ifdef SERIAL_DEBUG
    //		auto index = ++debugIndex;
    //		release_D[index % debugSize] = { refIncResult,content,0 };
    //#endif
    //
    //		return refIncResult;
    //	}
    //
    //	void Release(LPCWCH cause = L"")
    //	{
    //		auto refResult = --_refCount;
    //
    //#ifdef SERIAL_DEBUG
    //		auto index = ++debugIndex;
    //		release_D[index % debugSize] = { refResult,content,0 };
    //#endif
    //
    //		if ( refResult == 0 )
    //		{
    //			_pool.Free(this);
    //		}
    //	}


    [[nodiscard]] int CanPopSize() const;

private:
#ifdef SERIAL_DEBUG
	//DEBUG
	struct RelastinReleaseEncrypt_D
	{
		long refCount;
		psh::LPCWSTR location;
		long long contentType;
	};

#endif

    void Clear();
    std::wstring ReadWstr(int codeUnits);
    RingBuffer* _buffer = nullptr;
    psh::int32 _size = 0;
    long _refCount = 0;

    //static TlsPool<RecvBuffer, 0, false> _pool;

#ifdef SERIAL_DEBUG

	long debugIndex = 0;
	RelastinReleaseEncrypt_D release_D[debugSize];

#endif
};

template <typename T>
RecvBuffer& RecvBuffer::operator>>(T& value)
{
    static_assert(std::is_scalar_v<T>);
    CanPop(sizeof(T));

    _buffer->Peek(reinterpret_cast<char*>(&value), sizeof(T));
    _buffer->Dequeue(sizeof(T));
    _size -= sizeof(T);

    return *this;
}
