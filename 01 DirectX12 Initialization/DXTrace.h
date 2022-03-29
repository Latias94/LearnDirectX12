#pragma once

#include <Windows.h>
#include <string>

inline std::wstring AnsiToWString(const std::string &str)
{
    WCHAR buffer[512];
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
    return std::wstring(buffer);
}

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
            throw DxException(hr__, L ## #x, wfn, __LINE__);                                                               \
        }                                                                                                              \
    }
#endif
