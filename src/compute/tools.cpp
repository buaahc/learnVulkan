#include "tools.h"
#include <iostream>
#include <string>
#include <windows.h>
#include <filesystem>
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

std::string getExeDirectory() {
    char buffer[MAX_PATH];
    // 获取 exe 的完整绝对路径，例如: C:\MyProject\build\Debug\App.exe
    GetModuleFileNameA(NULL, buffer, MAX_PATH);

    // 使用 std::filesystem 提取所在目录
    std::filesystem::path exePath(buffer);

    // parent_path() 会去掉 App.exe，只保留 C:\MyProject\build\Debug
    return exePath.parent_path().string();
}