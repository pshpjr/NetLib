#include "SettingParser.h"

#include <format>
#include <fstream>
#include <iostream>

#include "Types.h"
#include "Logger.h"
#include "CoreGlobal.h"
#include "Utility.h"


#define WORD_START _buffer[_bufferIndex]
#define WORD_END _buffer[wordEnd]


void SettingParser::Init(const psh::LPCWSTR location)
{
    LoadSetting(location);
    Parse();
    free(_buffer);
}

void SettingParser::LoadSetting(const psh::LPCWSTR location)
{
    std::wifstream rawText{location, std::ios::binary | std::ios::ate};


    if (!rawText.is_open())
    {
        throw std::runtime_error(std::format("SettingParser::Couldn't open : {}", psh::util::WToS(location)));
    }

    const size_t fileSize = rawText.tellg();

    rawText.seekg(0);
    _buffer = static_cast<psh::LPWCH>(malloc(fileSize * 2 + 2));
    if (_buffer == nullptr)
    {
        throw std::runtime_error(std::format("SettingParser::Malloc return null"));
    }

    bufferSize = fileSize + 1;

    rawText.read(_buffer, fileSize);
    const size_t count = rawText.gcount();
    //파일 읽고 문자열 끝에 null 추가
    if (count != fileSize)
    {
        throw std::runtime_error(std::format("SettingParser::Invalid file Size"));
    }

    _buffer[fileSize] = L'\0';
}

//텍스트 파일은 키 : 값 쌍으로 이루어짐
void SettingParser::Parse()
{
    psh::WCHAR key[MAX_WORD_SIZE];
    psh::WCHAR value[MAX_WORD_SIZE];

    while (true)
    {
        psh::WCHAR op[MAX_WORD_SIZE];

        if (GetTok(key) == false)
        {
            break;
        }

        //그룹 종료 예외 처리
        if (key[0] == L'}')
        {
            continue;
        }

        if (GetTok(op) == false)
        {
            break;
        }

        // ':' 위치에 다른 거 있으면 에러
        if (op[0] != L':')
        {
            throw std::runtime_error(std::format("SettingParser::Parsing error. expected ':',but found {}",psh::util::WToS(String{op[0]})));
        }

        if (GetTok(value) == false)
        {
            break;
        }

        //그룹 시작 예외 처리
        if (value[0] == L'{')
        {
            _groupIndex++;
            _groupsName[_groupIndex] = key;
            continue;
        }
        _settingsContainer[_groupIndex].insert({key, value});
    }
}

bool SettingParser::GetTok(const psh::LPWCH word)
{
    //의미 있는 문자까지 이동. 널문자를 만나도 탈출
    while (_bufferIndex < bufferSize)
    {
        if (WORD_START == L'﻿' || WORD_START == L'\n' || WORD_START == L' ' || WORD_START == 0x09 || WORD_START == 0x0a
            || WORD_START == 0x0d)
        {
            _bufferIndex++;
        }
        else
        {
            break;
        }
    }

    //널문자 만났는지 확인
    if (WORD_START == L'\0' || _bufferIndex == bufferSize)
    {
        return false;
    }

    size_t wordEnd = _bufferIndex + 1;

    //의미 있는 문자가 한 줄 주석이면 다음 줄 시작까지 이동
    if (WORD_START == L'/' && WORD_END == L'/')
    {
        while (wordEnd < bufferSize)
        {
            if (WORD_END == L'\n')
            {
                wordEnd++;
                break;
            }
            wordEnd++;
        }
        _bufferIndex = wordEnd;
        return GetTok(word);
    }

    //여러 줄 주석이면 */ 만날 때 까지 이동
    if (WORD_START == L'/' && WORD_END == L'*')
    {
        while (wordEnd < bufferSize)
        {
            if (WORD_END == L'*' && _buffer[wordEnd + 1] == L'/')
            {
                wordEnd += 2;
                break;
            }
            wordEnd++;
        }
        _bufferIndex = wordEnd;
        return GetTok(word);
    }

    if (WORD_START == L'"')
    {
        //시작하는 " 문자열에 안 들어가게 하기 위한 처리
        _bufferIndex++;
        while (wordEnd < bufferSize)
        {
            if (WORD_END == L'"')
            {
                wordEnd++;
                break;
            }
            wordEnd++;
        }

        if (_bufferIndex == bufferSize)
        {
            return false;
        }

        //끝나는 " 문자열에 안 들어가게 
        const size_t wordSize = wordEnd - _bufferIndex - 1;

        wcsncpy_s(word, MAX_WORD_SIZE, &WORD_START, wordSize);
        _bufferIndex = wordEnd;
        return true;
    }


    //하나짜리 토큰이면 바로 리턴
    if (WORD_START == L':' || WORD_START == L'{' || WORD_START == L'}')
    {
        const size_t wordSize = wordEnd - _bufferIndex;
        wcsncpy_s(word, MAX_WORD_SIZE, &WORD_START, wordSize);
        word[wordSize] = L'\0';

        _bufferIndex = wordEnd;

        return true;
    }


    //긴 토큰 받는 곳
    while (wordEnd < bufferSize)
    {
        if (WORD_END == L'\n' || WORD_END == L' ' || WORD_END == 0x09 ||
            WORD_END == 0x0a || WORD_END == 0x0d || WORD_END == L'\0' ||
            WORD_END == L':' || WORD_END == L'{' || WORD_END == L'}')
        {
            break;
        }
        wordEnd++;
    }
    if (_bufferIndex == bufferSize)
    {
        return false;
    }

    const size_t wordSize = wordEnd - _bufferIndex;
    wcsncpy_s(word, MAX_WORD_SIZE, &WORD_START, wordSize);
    word[wordSize] = L'\0';

    _bufferIndex = wordEnd;

    return true;
}

std::optional<String> SettingParser::GetValueImpl(const String& name) const
{
    const size_t groupEnd = name.find(L'.');

    const auto nameStart = name.begin();
    const String group(nameStart, nameStart + groupEnd);
    const String typeName(nameStart + groupEnd + 1, name.end());

    for (int i = 0; i < MAXGROUPSIZE; i++)
    {
        if (_groupsName[i].compare(group) == 0)
        {
            const auto result = _settingsContainer[i].find(typeName);

            if (result == _settingsContainer[i].end())
            {
                gLogger->Write(L"SettingParser", Logger::LogLevel::Debug, L"%s %s", group.c_str(), typeName.c_str());
                return {};
            }
            return result->second;
        }
    }
    return {};
}
