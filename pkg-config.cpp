#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <filesystem>

#include <wil\com.h>

#include <winsdkver.h>
#include <sdkddkver.h>
#include <windows.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

// https://stackoverflow.com/a/25605631/868014

inline std::string Format(char const* Format, ...)
{
    va_list Arguments;
    va_start(Arguments, Format);
    char Text[8 << 10]; // 8 K
    Text[0] = 0;
    WI_VERIFY(vsprintf_s(Text, Format, Arguments) != -1);
    va_end(Arguments);
    return Text;
}
inline std::wstring Format(wchar_t const* Format, ...)
{
    va_list Arguments;
    va_start(Arguments, Format);
    wchar_t Text[8 << 10]; // 8 K
    Text[0] = 0;
    WI_VERIFY(vswprintf_s(Text, Format, Arguments) != -1);
    va_end(Arguments);
    return Text;
}
inline std::string Join(std::vector<std::string> const& Vector, char const* Separator)
{
    std::string Text;
    if (!Vector.empty())
    {
        Text = Vector[0];
        for (size_t Index = 1; Index < Vector.size(); Index++)
        {
            Text.append(Separator);
            Text.append(Vector[Index]);
        }
    }
    return Text;
}
inline std::wstring Join(std::vector<std::wstring> const& Vector, wchar_t const* Separator)
{
    std::wstring Text;
    if (!Vector.empty())
    {
        Text = Vector[0];
        for (size_t Index = 1; Index < Vector.size(); Index++)
        {
            Text.append(Separator);
            Text.append(Vector[Index]);
        }
    }
    return Text;
}
inline size_t Split(std::string const& Value, std::vector<std::string>& Vector, char Separator)
{
    WI_ASSERT(Vector.empty() && Separator);
    for (size_t Position = 0; Position < Value.size(); )
    {
        const size_t SeparatorPosition = Value.find(Separator, Position);
        if (SeparatorPosition == Value.npos)
        {
            Vector.emplace_back(Value.substr(Position));
            break;
        }
        Vector.emplace_back(Value.substr(Position, SeparatorPosition - Position));
        Position = SeparatorPosition + 1;
    }
    return Vector.size();
}
inline std::vector<std::string> Split(std::string const& Value, char Separator)
{
    std::vector<std::string> Vector;
    Split(Value, Vector, Separator);
    return Vector;
}
inline size_t Split(std::wstring const& Value, std::vector<std::wstring>& Vector, wchar_t Separator)
{
    WI_ASSERT(Vector.empty() && Separator);
    for (size_t Position = 0; Position < Value.size(); )
    {
        const size_t SeparatorPosition = Value.find(Separator, Position);
        if (SeparatorPosition == Value.npos)
        {
            Vector.emplace_back(Value.substr(Position));
            break;
        }
        Vector.emplace_back(Value.substr(Position, SeparatorPosition - Position));
        Position = SeparatorPosition + 1;
    }
    return Vector.size();
}
inline std::vector<std::wstring> Split(std::wstring const& Value, wchar_t Separator)
{
    std::vector<std::wstring> Vector;
    Split(Value, Vector, Separator);
    return Vector;
}

inline std::wstring FromMultiByte(char const* Input, size_t InputLength, DWORD CodePage = CP_UTF8, DWORD Flags = 0)
{
    std::wstring Output;
    if (InputLength)
    {
        auto const OutputCapacity = MultiByteToWideChar(CodePage, Flags, Input, static_cast<INT>(InputLength), nullptr, 0);
        Output.resize(OutputCapacity);
        auto const OutputSize = MultiByteToWideChar(CodePage, Flags, Input, static_cast<INT>(InputLength), Output.data(), OutputCapacity);
        THROW_LAST_ERROR_IF(OutputSize <= 0);
        Output.resize(OutputSize);
    }
    return Output;
}
inline std::wstring FromMultiByte(std::string const& Input, DWORD CodePage = CP_UTF8, DWORD Flags = 0)
{
    return FromMultiByte(Input.c_str(), Input.size(), CodePage, Flags);
}
inline std::wstring FromMultiByte(uint8_t const* Input, size_t InputLength, DWORD CodePage = CP_UTF8, DWORD Flags = 0)
{
    return FromMultiByte(reinterpret_cast<char const*>(Input), InputLength, CodePage, Flags);
}

inline std::string ToMultiByte(wchar_t const* Input, size_t InputLength, DWORD CodePage = CP_UTF8, DWORD Flags = 0)
{
    std::string Output;
    if (InputLength)
    {
        auto const OutputCapacity = WideCharToMultiByte(CodePage, Flags, Input, static_cast<INT>(InputLength), nullptr, 0, nullptr, nullptr);
        Output.resize(OutputCapacity);
        auto const OutputSize = WideCharToMultiByte(CodePage, Flags, Input, static_cast<INT>(InputLength), Output.data(), OutputCapacity, nullptr, nullptr);
        THROW_LAST_ERROR_IF(OutputSize <= 0);
        Output.resize(OutputSize);
    }
    return Output;
}
inline std::string ToMultiByte(std::wstring const& Input, DWORD CodePage = CP_UTF8, DWORD Flags = 0)
{
    return ToMultiByte(Input.c_str(), Input.size(), CodePage, Flags);
}

std::vector<std::filesystem::path> Where()
{
    SECURITY_ATTRIBUTES Attributes{ sizeof Attributes };
    Attributes.bInheritHandle = TRUE;
    wil::unique_handle ReadPipe, WritePipe;
    THROW_IF_WIN32_BOOL_FALSE(CreatePipe(ReadPipe.put(), WritePipe.put(), &Attributes, 0));
    wchar_t SystemDirectory[MAX_PATH];
    WI_VERIFY(GetSystemDirectoryW(SystemDirectory, static_cast<DWORD>(std::size(SystemDirectory))));
    wchar_t Path[MAX_PATH];
    WI_VERIFY(PathCombineW(Path, SystemDirectory, L"where.exe"));
    std::wostringstream CommandLine;
    CommandLine << L'"' << Path << L'"' << L" pkg-config.exe";
    STARTUPINFOW StartupInfo{ sizeof StartupInfo };
    StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    StartupInfo.hStdOutput = WritePipe.get();
    StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION ProcessInformation{ };
    THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(Path, const_cast<wchar_t*>(CommandLine.str().c_str()), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &StartupInfo, &ProcessInformation));
    CloseHandle(ProcessInformation.hThread);
    WritePipe.reset();
    std::string Output;
    {
        for (;;)
        {
            char Data[8u << 10u];
            DWORD DataSize;
            auto const ReadResult = ReadFile(ReadPipe.get(), Data, static_cast<DWORD>(std::size(Data)), &DataSize, nullptr);
            //_RPTWN(_CRT_WARN, L"ReadResult %d, DataSize %u\n", ReadResult, DataSize);
            if (!ReadResult || DataSize == 0)
                break;
            Output.append(Data, Data + DataSize);
        }
    }
    CloseHandle(ProcessInformation.hProcess);
    auto const OutputVector = Split(Output, L'\n');
    std::vector<std::filesystem::path> Result;
    for (auto&& Path : OutputVector)
    {
        if (Path.empty())
            continue;
        if (Path.back() == '\r')
            Result.emplace_back(Path.substr(0, Path.length() - 1));
        else
            Result.emplace_back(Path);
    }
    return Result;
}

bool StartsWith(wchar_t const* A, wchar_t const* B)
{
    WI_ASSERT(A && B);
    auto const BL = wcslen(B);
    return wcsncmp(A, B, BL) == 0;
}
bool StartsWith(std::wstring const& A, std::wstring const& B)
{
    return StartsWith(A.c_str(), B.c_str());
}

std::wstring const PKG_CONFIG_PATH = L"PKG_CONFIG_PATH";

int wmain(int argc, wchar_t const* argv[])
{
    std::wstring OverridePath;
    std::vector<std::wstring> CommandLineVector;
    for (size_t Index = 1; Index < argc; Index++)
    {
        std::wstring ArgumentValue = argv[Index];
        if (StartsWith(ArgumentValue, PKG_CONFIG_PATH + L"="))
        {
            WI_ASSERT(OverridePath.empty());
            OverridePath = ArgumentValue.substr(PKG_CONFIG_PATH.length() + 1u);
            if (OverridePath.length() >= 2 && OverridePath.front() == L'\"' && OverridePath.back() == L'\"')
                OverridePath = OverridePath.substr(1u, OverridePath.length() - 2u);
            continue;
        }
        CommandLineVector.emplace_back(ArgumentValue);
    }
    _RPTWN(_CRT_WARN, L"OverridePath \"%ls\"\n", OverridePath.c_str());
    LOG_HR_IF(E_UNEXPECTED, OverridePath.empty());

    std::wstring Environment;
    {
        std::wostringstream Stream;
        wchar_t const* String = GetEnvironmentStringsW();
        WI_ASSERT(String);
        for (; *String; )
        {
            if (!StartsWith(String, PKG_CONFIG_PATH + L"=") || OverridePath.empty())
            {
                Stream << String;
                Stream << L'\0';
            }
            else
            {
                _RPTWN(_CRT_WARN, L"String \"%ls\"\n", String);
            }
            String += wcslen(String) + 1;
        }
        if (!OverridePath.empty())
        {
            Stream << PKG_CONFIG_PATH;
            Stream << L'=';
            Stream << OverridePath;
            Stream << L'\0';
        }
        Environment = Stream.str();
        if(false)
        {
            _RPTWN(_CRT_WARN, L"<<< Environment\n", 0);
            wchar_t const* String = Environment.c_str(); //GetEnvironmentStringsW();
            WI_ASSERT(String);
            for (; *String; )
            {
                _RPTWN(_CRT_WARN, L"%ls\n", String);
                String += wcslen(String) + 1;
            }
            _RPTWN(_CRT_WARN, L">>>\n", 0);
        }
    }

    auto const Vector = Where();
    THROW_HR_IF(E_FAIL, Vector.empty());
    size_t Index = 0;
    {
        wchar_t Path[MAX_PATH];
        WI_VERIFY(GetModuleFileNameW(nullptr, Path, static_cast<DWORD>(std::size(Path))));
        if (Vector[Index].compare(Path) == 0)
            Index++;
    }
    auto const Path = Vector[Index];
    _RPTWN(_CRT_WARN, L"Path \"%ls\"\n", Path.c_str());

    SECURITY_ATTRIBUTES Attributes{ sizeof Attributes };
    Attributes.bInheritHandle = TRUE;
    wil::unique_handle ReadPipe, WritePipe;
    THROW_IF_WIN32_BOOL_FALSE(CreatePipe(ReadPipe.put(), WritePipe.put(), &Attributes, 0));
    std::wostringstream CommandLine;
    CommandLine << L'"' << Path << L'"' << L" " << Join(CommandLineVector, L" ");
    _RPTWN(_CRT_WARN, L"CommandLine \"%ls\"\n", CommandLine.str().c_str());
    STARTUPINFOW StartupInfo{ sizeof StartupInfo };
    StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    StartupInfo.hStdOutput = WritePipe.get(); //GetStdHandle(STD_OUTPUT_HANDLE);
    StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION ProcessInformation{ };
    THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(Path.c_str(), const_cast<wchar_t*>(CommandLine.str().c_str()), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT, const_cast<wchar_t*>(Environment.c_str()), nullptr, &StartupInfo, &ProcessInformation));
    CloseHandle(ProcessInformation.hThread);
    WritePipe.reset();
    {
        std::string Output;
        for (;;)
        {
            char Data[8u << 10u];
            DWORD DataSize;
            auto const ReadResult = ReadFile(ReadPipe.get(), Data, static_cast<DWORD>(std::size(Data)), &DataSize, nullptr);
            //_RPTWN(_CRT_WARN, L"ReadResult %d, DataSize %u\n", ReadResult, DataSize);
            if (!ReadResult || DataSize == 0)
                break;
            Output.append(Data, Data + DataSize);
            DWORD WriteDataSize;
            THROW_IF_WIN32_BOOL_FALSE(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), Data, DataSize, &WriteDataSize, nullptr));
            WI_ASSERT(WriteDataSize == DataSize);
        }
        _RPTWN(_CRT_WARN, L"<<< Output (%zu bytes)\n", Output.length());
        auto OutputVector = Split(Output, L'\n');
        for (auto& Output : OutputVector)
        {
            if (!Output.empty() && Output.back() == '\r')
                Output.erase(Output.length() - 1u);
            _RPTWN(_CRT_WARN, L"%ls\n", FromMultiByte(Output).c_str());
        }
        _RPTWN(_CRT_WARN, L">>>\n", 0);
    }
    auto const WaitResult = WaitForSingleObject(ProcessInformation.hProcess, INFINITE);
    THROW_HR_IF(E_UNEXPECTED, WaitResult != WAIT_OBJECT_0);
    DWORD ExitCode;
    THROW_IF_WIN32_BOOL_FALSE(GetExitCodeProcess(ProcessInformation.hProcess, &ExitCode));
    _RPTWN(_CRT_WARN, L"ExitCode %d\n", ExitCode);
    CloseHandle(ProcessInformation.hProcess);
    return static_cast<int>(ExitCode);
}
