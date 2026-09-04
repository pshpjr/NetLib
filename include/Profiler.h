#pragma once
#include <chrono>
#include <list>
#include <shared_mutex>
#include <thread>

#include "LockFreeData.h"
#include "Macro.h"
#include "Types.h"
#include "Utility.h"

class ProfileManager;
class ProfileItem;


constexpr int MAX_NAME = 64;
constexpr int MAX_ITEM = 64;

struct PROFILE_SAMPLE
{
    PROFILE_SAMPLE()
        : iTotalTime(0)
        , iMin(987654321)
        , iMax(-1)
    {
    }

    psh::WCHAR szName[MAX_NAME] = L"";
    long lFlag = false; // 프로파일의 사용 여부. (배열시에만)
    // 프로파일 샘플 이름.

    std::chrono::microseconds iTotalTime; // 전체 사용시간 카운터 Time.	(출력시 호출회수로 나누어 평균 구함)
    std::chrono::microseconds iMin;
    std::chrono::microseconds iMax;

    __int64 iCall = 0; // 누적 호출 횟수.
};


class Profiler
{
    friend ProfileManager;

public:
    void ProfileDataOutText(LPWSTR szFileName);

    void ProfileReset()
    {
        for (auto& profileSample : Profile_Samples)
        {
            profileSample.lFlag = false;
        }
    }

    void ApplyProfile(const int& itemNumber, std::chrono::microseconds time);

    int GetItemNumber(psh::LPCWSTR name);

private:
    Profiler() = default;


    PROFILE_SAMPLE Profile_Samples[MAX_ITEM];
};

class ProfileItem
{
public:
    ProfileItem(psh::LPCWSTR name);
    ~ProfileItem();

private:
    std::chrono::steady_clock::time_point _start;
    std::chrono::steady_clock::time_point _end;
    psh::WCHAR _name[MAX_NAME];
};


int call_profile_manager_destructor();

class ProfileManager
{
    ProfileManager()
    {
        setTLSNum();
        //_onexit(call_profile_manager_destructor); 
    }

public:
    ~ProfileManager()
    {
        const time_t timer = time(nullptr);
        tm t;
        localtime_s(&t, &timer);
        psh::WCHAR buffer[100];
        wsprintfW(buffer,
            L"profile_%d-%d-%d_%2d%2d_%x.txt",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, std::this_thread::get_id());
        ProfileDataOutText(buffer);
        psh::util::TlsFree(TLSNum);
    }

    static ProfileManager& Get()
    {
        static ProfileManager manager;
        return manager;
    }

    Profiler& GetLocalProfiler()
    {
        auto ret = static_cast<Profiler*>(psh::util::TlsGet(TLSNum));

        if (ret == nullptr)
        {
            ret = new Profiler;
            psh::util::TlsSet(TLSNum, ret);

            WRITE_LOCK;
            _profilerList.push_front(ret);

        }

        return *ret;
    }

    void ProfileDataOutText(LPWSTR szFileName);

    void DumpAndReset()
    {
        const time_t timer = time(nullptr);
        tm t;
        localtime_s(&t, &timer);
        psh::WCHAR buffer[100];
        wsprintfW(buffer,
            L"profile_%d-%d-%d_%2d%2d_%x.txt",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, std::this_thread::get_id());
        ProfileDataOutText(buffer);

        ProfileReset();
    }


    void ProfileReset()
    {
        READ_LOCK;
        for (const auto& pro : _profilerList)
        {
            pro->ProfileReset();
        }
    }

    void SetOptional(const std::wstring& optional);

    void setTLSNum()
    {
        psh::util::TlsAlloc(TLSNum);
    }

private:
    USE_LOCK;

    inline static std::once_flag _flag;
    inline static ProfileManager* _instance;

    std::list<Profiler*> _profilerList;

    std::wstring optionalText;
    psh::util::TlsKey TLSNum;
};


//#define PROFILE

#ifdef PROFILE

#define PROFILE_CONCATENATE_DETAIL(x, y) x##y
#define PROFILE_CONCATENATE(x, y) PROFILE_CONCATENATE_DETAIL(x, y)
#define PROFILE_MAKE_UNIQUE(x) PROFILE_CONCATENATE(x, __COUNTER__)

#define PRO_BEGIN(TagName) ProfileItem PROFILE_MAKE_UNIQUE(t_)(L#TagName);
#else

#define PRO_BEGIN(TagName)
#define PRO_END(TagName)
#endif
