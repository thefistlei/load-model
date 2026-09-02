#include <learnopengl/platform.h>

#include <android/asset_manager.h>
#include <android/log.h>

#include <sys/stat.h>

#include <fstream>
#include <string>
#include <vector>

#define LOG_TAG "model_loading"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static AAssetManager* gAssetManager = nullptr;
static std::string gDataRoot;

void platformSetAndroidContext(AAssetManager* assetManager, const char* internalDataPath)
{
    gAssetManager = assetManager;
    gDataRoot = internalDataPath ? internalDataPath : ".";
}

std::string platformDataRoot()
{
    return gDataRoot.empty() ? "." : gDataRoot;
}

bool platformFileExists(const std::string& path)
{
    std::ifstream file(path.c_str());
    return file.good();
}

bool platformIsDirectory(const std::string& path)
{
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool ensureParentDir(const std::string& filePath)
{
    const size_t pos = filePath.find_last_of('/');
    if (pos == std::string::npos)
        return true;
    std::string dir = filePath.substr(0, pos);
    if (dir.empty())
        return true;
    if (platformIsDirectory(dir))
        return true;
    return mkdir(dir.c_str(), 0755) == 0 || platformIsDirectory(dir);
}

static void extractAssetDir(const std::string& assetDir, const std::string& destDir)
{
    if (!gAssetManager)
        return;

    AAssetDir* dir = AAssetManager_openDir(gAssetManager, assetDir.c_str());
    if (!dir)
        return;

    const char* name = nullptr;
    while ((name = AAssetDir_getNextFileName(dir)) != nullptr)
    {
        const std::string assetPath = assetDir.empty() ? name : assetDir + "/" + name;
        const std::string outPath = destDir + "/" + name;

        AAssetDir* sub = AAssetManager_openDir(gAssetManager, assetPath.c_str());
        if (sub)
        {
            AAssetDir_close(sub);
            mkdir(outPath.c_str(), 0755);
            extractAssetDir(assetPath, outPath);
            continue;
        }

        AAsset* asset = AAssetManager_open(gAssetManager, assetPath.c_str(), AASSET_MODE_BUFFER);
        if (!asset)
            continue;

        const off_t length = AAsset_getLength(asset);
        if (length <= 0)
        {
            AAsset_close(asset);
            continue;
        }

        std::vector<char> buffer(static_cast<size_t>(length));
        const int read = AAsset_read(asset, buffer.data(), static_cast<size_t>(length));
        AAsset_close(asset);
        if (read != length)
            continue;

        if (!ensureParentDir(outPath))
            continue;

        std::ofstream out(outPath, std::ios::binary);
        if (!out.is_open())
            continue;
        out.write(buffer.data(), buffer.size());
    }

    AAssetDir_close(dir);
}

void platformInitAssets()
{
    if (!gAssetManager || gDataRoot.empty())
        return;

    LOGI("Extracting assets to %s", gDataRoot.c_str());
    extractAssetDir("", gDataRoot);
}
