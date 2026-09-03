#pragma once

class EFileOpen : public std::exception
{
public:
    const char* what() const override
    {
        return "file open fail";
    }
};

class EFileLoad : public std::exception
{
public:
    const char* what() const override
    {
        return "file load fail";
    }
};

class EUnExpectOp final : public std::exception
{
public:
    const char* what() const override
    {
        return "operation is invalid";
    }
};

class EInvalidGroup final : public std::exception
{
public:
    const char* what() const override
    {
        return "group name is invalid";
    }
};

class EInvalidName final : public std::exception
{
public:
    const char* what() const override
    {
        return "item name is invalid";
    }
};
