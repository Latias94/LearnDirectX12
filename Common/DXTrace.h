#pragma once

#include <Windows.h>
#include <string>
#include "d3dUtil.h"

class DxException
{
  public:
    DxException() = default;
    DxException(HRESULT hr, const std::wstring &functionName, const std::wstring &filename, int lineNumber);

    std::wstring ToString() const;

    HRESULT ErrorCode = S_OK;
    std::wstring FunctionName;
    std::wstring Filename;
    int LineNumber = -1;
};

// L#x 将宏 ThrowIfFailed 的参数转换成一个 Unicode string。
// 这样，我们就能将函数所引发的 error 输出到 message box 了。
// https://stackoverflow.com/questions/34477263/gcc-and-clang-preprocessor-doesnt-understand-l-prefix
#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                                                                               \
    {                                                                                                                  \
        HRESULT hr__ = (x);                                                                                            \
        std::wstring wfn = AnsiToWString(__FILE__);                                                                    \
        if (FAILED(hr__))                                                                                              \
        {                                                                                                              \
            throw DxException(hr__, L## #x, wfn, __LINE__);                                                            \
        }                                                                                                              \
    }
#endif

/*
#if defined(_DEBUG)
    #ifndef Assert
    #define Assert(x, description)                                  \
    {                                                               \
        static bool ignoreAssert = false;                           \
        if(!ignoreAssert && !(x))                                   \
        {                                                           \
            Debug::AssertResult result = Debug::ShowAssertDialog(   \
            (L#x), description, AnsiToWString(__FILE__), __LINE__); \
        if(result == Debug::AssertIgnore)                           \
        {                                                           \
            ignoreAssert = true;                                    \
        }                                                           \
                    else if(result == Debug::AssertBreak)           \
        {                                                           \
            __debugbreak();                                         \
        }                                                           \
        }                                                           \
    }
    #endif
#else
    #ifndef Assert
    #define Assert(x, description)
    #endif
#endif
    */
