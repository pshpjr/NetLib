//
// Created by Park on 24. 8. 22.
#ifndef MODERNOBJECTPOOL_H
#define MODERNOBJECTPOOL_H

#include "Utility.h"
#include <Types.h>

#include <iostream>
#include <memory>
#include <vector>

/**
 * unique_ptr로 관리되는 객체를 리턴하는 풀.
 * 알아서 소멸 시 이 풀로 반환됨
 */
template <typename T>
class ModernObjectPool
{
    // 어떤 객체를 생성할 때 메모리 정렬을 보장해야 한다.
    using storageType = std::aligned_storage_t<sizeof(T), alignof(T)>;

    // 소멸할때도 정렬에 맞춰서 소멸시켜줘야 한다.
    using storagePtr = std::unique_ptr<storageType>;
public:

    // 생성자: 첫 초기 사이즈만큼 풀을 확장함
    explicit ModernObjectPool(size_t initialSize = 100)
    {
        expandPool(initialSize);
    }

    // 소멸자: 기본 소멸자 사용
    ~ModernObjectPool() = default;
    // Destruct 함수는 객체를 풀에 반환하는 역할을 함
    static void Deleter(T* obj, ModernObjectPool<T>* pool)
    {
        pool->Free(obj);
    }

    // 공용 멤버 함수: 객체를 풀에 반환
    void Free(T* obj)
    {
        std::destroy_at(obj);
        _pool.emplace_back(reinterpret_cast<storageType*>(obj));

        integrity();
    }

    // 객체를 풀에서 할당, 가변 인수를 받아 객체를 생성
    template <typename... Args>
    PoolPtr<T> Alloc(Args... args)
    {
        if (_pool.empty())
        {
            expandPool(_pool.capacity() + 1);
        }
        // 풀에서 객체를 가져옴
        T* objPtr = reinterpret_cast<T*>(_pool.back().release());
        if (objPtr == nullptr)
        {
            psh::util::DebugBreak();
        }
        _pool.pop_back();
        std::construct_at(objPtr, std::forward<Args>(args)...);

        integrity();


        return PoolPtr<T>(objPtr, [this](T* ptr) {
            Deleter(ptr, this);
        });
    }

    // 현재 풀에 남아있는 객체 수를 반환
    auto Size() const
    {
        return _pool.size();
    }

    // 템플릿 함수: Base 타입을 사용하여 객체를 캐스팅
    template <typename Base>
    PoolPtr<Base> Cast(PoolPtr<T> ptr)
    {
        static_assert(std::is_base_of_v<Base, T>, "Invalid Type");
        Base* basePtr = static_cast<Base*>(ptr.release());
        return PoolPtr<Base>(basePtr, [this](Base* basePtr) {
            Deleter(static_cast<T*>(basePtr), this);
        });
    }
private:

    // 비공용 멤버 함수: 풀을 확장
    void expandPool(size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            auto ptr = new storageType;
            if (ptr == nullptr)
            {
                psh::util::DebugBreak();
            }
            _pool.emplace_back(ptr);
        }
    }

    bool integrity()
    {
        for (auto& ptr : _pool)
        {
            if (ptr == nullptr)
            {
                psh::util::DebugBreak();
                return false;
            }
        }
        return true;
    }

    // 멤버 변수: 저장된 객체를 관리하는 벡터
    std::vector<storagePtr> _pool;
};

#endif //MODERNOBJECTPOOL_H
