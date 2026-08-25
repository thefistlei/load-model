#include <learnopengl/ktx2_image_provider.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>

namespace
{
#ifndef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_5x5_KHR
#define GL_COMPRESSED_RGBA_ASTC_5x5_KHR 0x93B2
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_6x6_KHR
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR 0x93BD
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_8x8_KHR
#define GL_COMPRESSED_RGBA_ASTC_8x8_KHR 0x93C1
#endif
#ifndef GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR 0x93D0
#endif
#ifndef GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR 0x93D1
#endif
#ifndef GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR 0x93D2
#endif
#ifndef GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR 0x93D6
#endif

struct KTX2_Header
{
    uint8_t identifier[12];
    uint32_t vk_format;
    uint32_t type_size;
    uint32_t pixel_width;
    uint32_t pixel_height;
    uint32_t pixel_depth;
    uint32_t layer_count;
    uint32_t face_count;
    uint32_t level_count;
    uint32_t supercompression_scheme;
    uint8_t dfd_byte_offset[4];
    uint8_t dfd_byte_length[4];
    uint8_t kvd_byte_offset[4];
    uint8_t kvd_byte_length[4];
    uint8_t sgd_byte_offset[8];
    uint8_t sgd_byte_length[8];
};

struct KTX2_Level_Index
{
    uint8_t byte_offset[8];
    uint8_t byte_length[8];
    uint8_t uncompressed_byte_length[8];
};

static constexpr uint8_t ktx2_magic[12] = {
    0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};

enum VkFormatAstc : uint32_t
{
    VK_FORMAT_ASTC_4x4_UNORM_BLOCK = 157,
    VK_FORMAT_ASTC_4x4_SRGB_BLOCK = 158,
    VK_FORMAT_ASTC_5x5_UNORM_BLOCK = 161,
    VK_FORMAT_ASTC_5x5_SRGB_BLOCK = 162,
    VK_FORMAT_ASTC_6x6_UNORM_BLOCK = 165,
    VK_FORMAT_ASTC_6x6_SRGB_BLOCK = 166,
    VK_FORMAT_ASTC_8x8_UNORM_BLOCK = 171,
    VK_FORMAT_ASTC_8x8_SRGB_BLOCK = 172,
};

uint32_t read_u32(const uint8_t* p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

uint64_t read_u64(const uint8_t* p)
{
    return uint64_t(read_u32(p)) | (uint64_t(read_u32(p + 4)) << 32);
}

bool is_ktx2_magic(const uint8_t* id)
{
    return std::memcmp(id, ktx2_magic, sizeof(ktx2_magic)) == 0;
}

bool vk_format_is_astc(uint32_t vk_format)
{
    switch (vk_format)
    {
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        return true;
    default:
        return false;
    }
}

GLenum vk_format_to_gl_internal_format(uint32_t vk_format)
{
    switch (vk_format)
    {
    case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
        return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
    case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
        return GL_COMPRESSED_RGBA_ASTC_5x5_KHR;
    case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
        return GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
    case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
        return GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
    case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR;
    case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR;
    case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR;
    case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR;
    default:
        return 0;
    }
}

bool read_file_binary(const std::string& path, std::vector<unsigned char>& out)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return false;

    const std::streamoff size = file.tellg();
    if (size <= 0)
        return false;

    out.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char*>(out.data()), size);
    return file.good();
}

bool parse_ktx2(const unsigned char* buffer, size_t buffer_size, Ktx2Texture& out)
{
    out = Ktx2Texture{};
    if (buffer_size < sizeof(KTX2_Header))
        return false;

    const auto& header = *reinterpret_cast<const KTX2_Header*>(buffer);
    if (!is_ktx2_magic(header.identifier))
        return false;

    if (header.supercompression_scheme != 0)
        return false;

    const uint32_t vk_format = header.vk_format;
    if (!vk_format_is_astc(vk_format))
        return false;

    const GLenum internal_format = vk_format_to_gl_internal_format(vk_format);
    if (internal_format == 0)
        return false;

    const int level_count = static_cast<int>(header.level_count);
    if (level_count <= 0 || level_count > 32)
        return false;

    const int base_width = static_cast<int>(header.pixel_width);
    const int base_height = static_cast<int>(header.pixel_height);
    if (base_width <= 0 || base_height <= 0)
        return false;

    const size_t level_index_offset = sizeof(KTX2_Header);
    if (buffer_size < level_index_offset + static_cast<size_t>(level_count) * sizeof(KTX2_Level_Index))
        return false;

    const auto* level_indices = reinterpret_cast<const KTX2_Level_Index*>(buffer + level_index_offset);
    out.valid = true;
    out.internalFormat = internal_format;
    out.width = base_width;
    out.height = base_height;
    out.levels.reserve(static_cast<size_t>(level_count));

    int w = base_width;
    int h = base_height;
    for (int i = 0; i < level_count; ++i)
    {
        const auto& li = level_indices[i];
        const uint64_t byte_offset = read_u64(li.byte_offset);
        const uint64_t byte_length = read_u64(li.byte_length);

        if (byte_offset > buffer_size || byte_length > buffer_size - byte_offset)
        {
            out = Ktx2Texture{};
            return false;
        }

        Ktx2MipLevel level;
        level.width = w;
        level.height = h;
        level.data.assign(buffer + byte_offset, buffer + byte_offset + byte_length);
        out.levels.push_back(std::move(level));

        w = (w > 1) ? w / 2 : 1;
        h = (h > 1) ? h / 2 : 1;
    }

    return true;
}
} // namespace

bool isKtx2File(const std::string& path)
{
    if (path.size() < 5)
        return false;

    std::string ext = path.substr(path.size() - 5);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".ktx2";
}

bool loadKtx2FromFile(const std::string& path, Ktx2Texture& out)
{
    std::vector<unsigned char> file_data;
    if (!read_file_binary(path, file_data))
        return false;

    return parse_ktx2(file_data.data(), file_data.size(), out);
}

bool uploadKtx2ToTexture(unsigned int texture, const Ktx2Texture& ktx)
{
    if (!ktx.valid || ktx.levels.empty())
        return false;

    // Drain previous errors so we can detect upload failures.
    while (glGetError() != GL_NO_ERROR) {}

    glBindTexture(GL_TEXTURE_2D, texture);
    for (size_t level = 0; level < ktx.levels.size(); ++level)
    {
        const Ktx2MipLevel& mip = ktx.levels[level];
        glCompressedTexImage2D(
            GL_TEXTURE_2D,
            static_cast<GLint>(level),
            ktx.internalFormat,
            mip.width,
            mip.height,
            0,
            static_cast<GLsizei>(mip.data.size()),
            mip.data.data());

        const GLenum err = glGetError();
        if (err != GL_NO_ERROR)
        {
            std::cout << "glCompressedTexImage2D failed level=" << level
                      << " format=0x" << std::hex << ktx.internalFormat << std::dec
                      << " size=" << mip.width << "x" << mip.height
                      << " err=0x" << std::hex << err << std::dec
                      << " (GPU may lack ASTC support)" << std::endl;
            return false;
        }
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    if (ktx.levels.size() > 1)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return true;
}
