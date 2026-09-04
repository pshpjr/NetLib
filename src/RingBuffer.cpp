#include "RingBuffer.h"
#include <Protocol.h>
#include "stdafx.h"


//front가 맨 앞이고 rear가 맨 끝인 상황 처리 해야 함. 
int RingBuffer::Size() const
{
    const int rear = GetIndex(_rear);
    const int front = GetIndex(_front);

    if (rear >= front)
    {
        return rear - front;
    }

    return BUFFER_SIZE - (front - rear);
}

void RingBuffer::Decode(const char staticKey)
{
    //맨 앞에 헤더가 있다는 가정. 
    NetHeader header;
    Peek(reinterpret_cast<char*>(&header), sizeof(NetHeader));

    int decodeStart = GetIndex(_front + offsetof(NetHeader, checkSum));
    //체크섬부터 디코딩한다는 가정. 
    const unsigned short decodeLen = header.len + static_cast<unsigned short>(
        sizeof(NetHeader) - offsetof(NetHeader, checkSum));

    unsigned char p = 0;
    unsigned char e = 0;
    unsigned char oldP = 0;
    for (int i = 1; i <= decodeLen; i++)
    {
        unsigned char tmp = _buffer[decodeStart];
        p = tmp ^ (e + staticKey + i);
        e = tmp;
        _buffer[decodeStart] = p ^ (oldP + header.randomKey + i);
        oldP = p;
        decodeStart = GetIndex(decodeStart + 1);
    }

    //NetHeader* head = (NetHeader*)GetFront();
    //char* decodeData = (char*)head + offsetof(NetHeader, checkSum);
    //const unsigned short decodeLen = head->len + static_cast<unsigned short>(sizeof(NetHeader) - offsetof(NetHeader, checkSum));

    //char p = 0;
    //char e = 0;
    //char oldP = 0;
    //for (int i = 1; i <= decodeLen; i++)
    //{
    //	p = (*decodeData) ^ (e + staticKey + i);
    //	e = *decodeData;
    //	*decodeData = p ^ (oldP + head->randomKey + i);
    //	oldP = p;
    //	decodeData++;
    //}
}

bool RingBuffer::ChecksumValid() const
{
    //맨 앞에 헤더가 있다는 가정. 

    NetHeader header{};
    Peek(reinterpret_cast<char*>(&header), sizeof(NetHeader));

    const int packetLen = header.len;
    unsigned char payloadChecksum = 0;
    const int payloadIndex = GetIndex(_front + sizeof(NetHeader));

    for (auto i = 0; i < packetLen; i++)
    {
        payloadChecksum += _buffer[GetIndex(payloadIndex + i)];
    }

    if (payloadChecksum != header.checkSum)
    {
        return false;
    }
    return true;
}


int RingBuffer::DirectEnqueueSize() const
{
    const int rear = GetIndex(_rear);
    const int front = GetIndex(_front);

    //front는 변할 수 있지만 rear는 변하지 않음. 
    //가득 찬 상황
    if (GetIndex(rear + 1) == front)
    {
        return 0;
    }

    //if문 탐지할 때 front와 들어가서 front 값이 달라질 수 있음.
    //front는 증가만 하지만 rear와 front가 둘 다 버퍼 끝자락에서 있을 때 front가 증가해 rear보다 작아질 수 있음.
    //이 경우 result가 음수 나올 수 있음.
    if (rear < front)
    {
        const int result = front - rear - 1;
        ASSERT_CRASH(result > 0, "Out of Case");
        return result;
    }
    if (rear >= front)
    {
        int result;
        if (front == 0)
        {
            result = BUFFER_SIZE - rear - 1;
        }
        else
        {
            result = BUFFER_SIZE - rear;
        }

        ASSERT_CRASH(result > 0, "Out of Case");

        return result;
    }

    ASSERT_CRASH(false, "Out of Case");

    return 0;
}

//int CRingBuffer::DirectEnqueueSize() const
//{
//	int rear = _rear;
//	int front = _front;
//
//	//front는 변할 수 있지만 rear는 변하지 않음. 
//	if ((rear + 1) % BUFFER_SIZE == front)
//	{
//		return 0;
//	}
//
//	//if문 탐지할 때 front와 들어가서 front 값이 달라질 수 있음.
//	//front는 증가만 하지만 rear와 front가 둘 다 버퍼 끝자락에서 있을 때 front가 증가해 rear보다 작아질 수 있음.
//	//이 경우 result가 음수 나올 수 있음.
//	if (rear <= ((front + BUFFER_SIZE - 1) % BUFFER_SIZE))
//	{
//		int result = (front + BUFFER_SIZE - 1) % BUFFER_SIZE - rear;
//		ASSERT_CRASH(result > 0, "Out of Case");
//		return result;
//	}
//	if (rear >= front)
//	{
//		int result;
//		if (front == 0)
//			result = BUFFER_SIZE - rear - 1;
//		else
//			result = BUFFER_SIZE - rear;
//
//		ASSERT_CRASH(result > 0, "Out of Case");
//
//		return result;
//	}
//
//	ASSERT_CRASH(false, "Out of Case");
//
//	return 0;
//}

int RingBuffer::DirectDequeueSize() const
{
    const int rear = GetIndex(_rear);
    const int front = GetIndex(_front);
    if (rear >= front)
    {
        return rear - front;
    }
    return BUFFER_SIZE - front;
}


int RingBuffer::Dequeue(const int deqSize)
{
    if (Size() == 0)
    {
        return 0;
    }

    //이후 dequeue 할 내용이 있다고 가정함. 

    int size = deqSize;

    //size 함수는 늘어나기만 함. if 이후 변경되어도 상관 없음. 
    if (deqSize > Size())
    {
        __debugbreak();
        size = Size();
    }

    _front += size;


    return size;
}

unsigned int RingBuffer::Peek(char *dst, const unsigned int size) const
{
    const int front = GetIndex(_front);
    const unsigned int peekSize = size;
    //size는 늘어나기만 함. 
    if (Size() < size)
    {
        throw;
    }


    if (DirectDequeueSize() >= peekSize)
    {
        memcpy_s(dst, peekSize, &_buffer[front], peekSize);
    }
    else
    {
        const int deqSize = DirectDequeueSize();

        memcpy_s(dst, deqSize, &_buffer[front], deqSize);
        memcpy_s(dst + deqSize, peekSize - deqSize, &_buffer[0], peekSize - deqSize);
    }

    return peekSize;
}
