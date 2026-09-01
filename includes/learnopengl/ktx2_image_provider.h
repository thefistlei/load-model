#ifndef KTX2_IMAGE_PROVIDER_H
#define KTX2_IMAGE_PROVIDER_H

#include <glad/glad.h>

#include <cstddef>
#include <string>
#include <vector>

struct Ktx2MipLevel
{
    int width = 0;
    int height = 0;
    std::vector<unsigned char> data;
};

struct Ktx2Texture
{
    bool valid = false;
    GLenum internalFormat = 0;
    int width = 0;
    int height = 0;
    std::vector<Ktx2MipLevel> levels;
};

bool isKtx2File(const std::string& path);
bool loadKtx2FromFile(const std::string& path, Ktx2Texture& out);
bool loadKtx2FromMemory(const unsigned char* data, size_t size, Ktx2Texture& out);
bool uploadKtx2ToTexture(unsigned int texture, const Ktx2Texture& ktx);

#endif
