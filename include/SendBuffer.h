#pragma once
#include "SendBufferImpl.h"
#include "Types.h"

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

	template <typename T>
	SendBuffer& operator<<(const T& value)
	{
		*buffer << value;
		return *this;
	}

	void SetWstr(psh::LPCWSTR arr, int size);
	void SetWstr(const wchar_t* arr, int size);
	void SetUtf16(std::u16string_view value);

	void SetCstr(psh::LPCSTR arr, int size);

	int CanPushSize();

private:
	SendBufferImpl* buffer;
};
