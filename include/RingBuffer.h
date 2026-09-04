#pragma once
#include "Types.h"
#include "Container.h"

class RingBuffer
{
    //버퍼 사이즈는 2의 N승꼴
    static constexpr auto BUFFER_SIZE = 8192;
    static constexpr auto INDEX_MASK = BUFFER_SIZE - 1;

    static_assert((BUFFER_SIZE & BUFFER_SIZE - 1) == 0);

public:
    /**
     * \
     * \return 사용중인 데이터의 크기
     */
    int Size() const;


    /**
     * \brief 한 번에 넣을 수 있는 데이터의 크기
     * \return
     */
    int DirectEnqueueSize() const;
    /**
     * \brief
     * \return 한 번에 뺄 수 있는 데이터의 크기
     */
    int DirectDequeueSize() const;

    unsigned int Peek(char* dst, const unsigned int size) const;

    /**
     * \brief deqSize만큼 버퍼에서 제거한다.
     * \param deqSize 제거할 크기
     * \return 삭제한 데이터의 크기
     */
    int Dequeue(int deqSize);

    int GetBufferSize() const
    {
        return BUFFER_SIZE;
    }

    /**
     * \brief
     * \return front의 주소
     */
    char* GetFront()
    {
        return &_buffer[GetIndex(_front)];
    }

    int GetFrontIndex() const
    {
        return GetIndex(_front);
    }
    int GetRearIndex() const
    {
        return GetIndex(_rear);
    }
    /**
     * \brief
     * \return rear의 주소
     */
    char* GetRear()
    {
        return &_buffer[GetIndex(_rear)];
    }

    /**
     * \brief
     * \return 내부 버퍼의 시작 위치
     */
    char* GetBuffer()
    {
        return &_buffer[0];
    }

    void Clear()
    {
        _front = 0;
        _rear = 0;
    }

    /**
     * \brief size만큼 front를 이동시킨다.
     * \param size 이동시킬 front의 크기
     */
    void MoveFront(const int size)
    {
        _front += size;
    }

    /**
     * \brief size만큼 rear를 이동시킨다.
     * \param size
     */
    void MoveRear(const int size)
    {
        _rear += size;
    }


    /**
     * \brief 추가로 삽입할 수 있는 크기.
     * \return
     */
    //rear+1 == front일 때 가득 찬 것이므로 1 빼야 함. 
    psh::uint32 FreeSize() const
    {
        return BUFFER_SIZE - Size() - 1;
    }

    void Decode(char staticKey);
    bool ChecksumValid() const;

    static int GetIndex(int num)
    {
        return num & INDEX_MASK;
    }

private:
    std::array<char, BUFFER_SIZE> _buffer = {};

    /**
     * \brief 데이터가 들어있는 위치
     */
    volatile int _front = 0;
    /**
     * \brief 데이터 삽입할 위치. 데이터는 전까지 있음.
     */
    volatile int _rear = 0;
};
