#include <learnopengl/platform.h>

#include <fstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <climits>
#include <sys/stat.h>
#endif

static std::string desktopExecutableDirectory()
{
#ifdef _WIN32
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
    if (len == 0 || len == MAX_PATH)
        return ".";
    std::string path(buffer, len);
    const size_t pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
#else
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len <= 0)
        return ".";
    buffer[len] = '\0';
    std::string path(buffer);
    const size_t pos = path.find_last_of('/');
    return (pos != std::string::npos) ? path.substr(0, pos) : ".";
#endif
}

std::string platformDataRoot()
{
    return desktopExecutableDirectory();
}

bool platformFileExists(const std::string& path)
{
    std::ifstream file(path.c_str());
    return file.good();
}

bool platformIsDirectory(const std::string& path)
{
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

void platformInitAssets()
{
}
