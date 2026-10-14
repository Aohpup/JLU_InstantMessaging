#pragma once
#include <filesystem>
#include <stdexcept>
#ifdef JLU_PORTABLE_DATA
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// 开发版沿用项目库；便携发布版使用 exe 同目录下的 data，不受启动工作目录影响。
inline std::filesystem::path defaultDatabasePathLJY()
{
#ifdef JLU_PORTABLE_DATA
    wchar_t path[32768];
    const auto size = GetModuleFileNameW(nullptr, path, 32768);
    if (size == 0 || size >= 32768) throw std::runtime_error("无法取得程序目录。");
    return std::filesystem::path(std::wstring(path, size)).parent_path() / "data" / "jlu_im.db";
#else
    return std::filesystem::u8path(JLU_DEFAULT_DATABASE_PATH);
#endif
}
