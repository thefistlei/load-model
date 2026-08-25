#include "ktx2-image-provider.hpp"
#include <core/util/file.hpp>
#include <cstring>
#include <vector>

namespace ss::resource::wild_image
{
    inline namespace
    {
        // KTX2 file format structures.
        // See: https://registry.khronos.org/KTX/specs/2.0/ktxspec-2.0.html

        struct KTX2_Header
        {
            core::Byte identifier[12]; // KTX2 magic: 0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0E, 0x0D, 0x0A, 0x1A
            core::Unt32 vk_format;      // VkFormat enum value
            core::Unt32 type_size;      // Size of one element of the data type
            core::Unt32 pixel_width;
            core::Unt32 pixel_height;
            core::Unt32 pixel_depth;
            core::Unt32 layer_count;
            core::Unt32 face_count;
            core::Unt32 level_count;
            core::Unt32 supercompression_scheme;
            core::Byte dfd_byte_offset[4];   // Offset to DFD (as Uint32 LE)
            core::Byte dfd_byte_length[4];   // Length of DFD
            core::Byte kvd_byte_offset[4];   // Offset to KVD
            core::Byte kvd_byte_length[4];   // Length of KVD
            core::Byte sgd_byte_offset[8];   // Offset to SGD
            core::Byte sgd_byte_length[8];   // Length of SGD
        };

        struct KTX2_Level_Index
        {
            core::Byte byte_offset[8];  // Uint64 LE
            core::Byte byte_length[8];  // Uint64 LE
            core::Byte uncompressed_byte_length[8]; // Uint64 LE
        };

        // Read little-endian values.
        auto read_u32(core::Byte const* p) -> core::Unt32
        {
            return core::Unt32(p[0])
                 | (core::Unt32(p[1]) << 8)
                 | (core::Unt32(p[2]) << 16)
                 | (core::Unt32(p[3]) << 24);
        }

        auto read_u64(core::Byte const* p) -> core::Unt64
        {
            return core::Unt64(read_u32(p))
                 | (core::Unt64(read_u32(p + 4)) << 32);
        }

        // KTX2 magic bytes.
        static constexpr core::Byte ktx2_magic[12] = {
            core::Byte(0xAB), core::Byte(0x4B), core::Byte(0x54), core::Byte(0x58),
            core::Byte(0x20), core::Byte(0x32), core::Byte(0x30), core::Byte(0xBB),
            core::Byte(0x0D), core::Byte(0x0A), core::Byte(0x1A), core::Byte(0x0A)
        };

        auto is_ktx2_magic(core::Byte const* id) -> bool
        {
            for (int i = 0; i < 12; i++) {
               // core::print("id[i], ", id[i]);
                if (id[i] != ktx2_magic[i]) {
                   // core::print("id[i], ", i);
                    return false;
                }
            }
            return true;
        }

        // VkFormat values for ASTC formats.
        // See: https://registry.khronos.org/vulkan/specs/1.3/html/chap/formats.html
         enum : core::Unt32
        {
            VK_FORMAT_ASTC_4x4_UNORM_BLOCK          = 157,
            VK_FORMAT_ASTC_4x4_SRGB_BLOCK           = 158,
            VK_FORMAT_ASTC_5x5_UNORM_BLOCK          = 161,
            VK_FORMAT_ASTC_5x5_SRGB_BLOCK           = 162,
            VK_FORMAT_ASTC_6x6_UNORM_BLOCK          = 165,
            VK_FORMAT_ASTC_6x6_SRGB_BLOCK           = 166,
            VK_FORMAT_ASTC_8x8_UNORM_BLOCK          = 171,
            VK_FORMAT_ASTC_8x8_SRGB_BLOCK           = 172,
        };

        auto vk_format_to_compressed_layout(core::Unt32 vk_format) -> resource::Compressed_Layout
        {
            using enum resource::Compressed_Layout;
            switch (vk_format) {
                case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:   return astc_4x4;
                case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:   return astc_5x5;
                case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:   return astc_6x6;
                case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:   return astc_8x8;
                case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:    return astc_4x4_sRGB;
                case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:    return astc_5x5_sRGB;
                case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:    return astc_6x6_sRGB;
                case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:    return astc_8x8_sRGB;
                default: return resource::Compressed_Layout::astc_4x4; // unknown
            }
        }

        auto vk_format_is_astc(core::Unt32 vk_format) -> bool
        {
            switch (vk_format) {
                case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:
                case VK_FORMAT_ASTC_5x5_UNORM_BLOCK:
                case VK_FORMAT_ASTC_6x6_UNORM_BLOCK:
                case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:
                case VK_FORMAT_ASTC_4x4_SRGB_BLOCK:
                case VK_FORMAT_ASTC_5x5_SRGB_BLOCK:
                case VK_FORMAT_ASTC_6x6_SRGB_BLOCK:
                case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:
                    return true;
                default: return false;
            }
        }

        // Parse KTX2 data from a raw buffer and return all mip level data.
        struct KTX2_Mip_Level
        {
            int width;
            int height;
            core::Byte const* data;
            core::Unt64 data_size;
        };

        struct KTX2_Parse_Result
        {
            bool valid{false};
            resource::Compressed_Layout compressed_layout{resource::Compressed_Layout::astc_4x4};
            int base_width{0};
            int base_height{0};
            int level_count{0};
            KTX2_Mip_Level levels[32]{}; // max 32 mip levels
        };

        auto parse_ktx2(core::Byte const* buffer, core::Unt64 buffer_size) -> KTX2_Parse_Result
        {
            auto result = KTX2_Parse_Result{};
            if (buffer_size < sizeof(KTX2_Header)) return result;

            auto& header = *reinterpret_cast<KTX2_Header const*>(buffer);

            // Verify magic.
            if (!is_ktx2_magic(header.identifier)) return result;

             if (header.supercompression_scheme != 0) return result;

            auto vk_format = header.vk_format;
            if (!vk_format_is_astc(vk_format)) return result; // Only handle ASTC compressed formats; STB handles uncompressed.

            auto layout = vk_format_to_compressed_layout(vk_format);
            auto level_count = int(header.level_count);
            if (level_count <= 0 || level_count > 32) return result;

            auto base_width = int(header.pixel_width);
            auto base_height = int(header.pixel_height);
            if (base_width == 0 || base_height == 0) return result;

            // Level index starts right after the header.
            auto level_index_offset = sizeof(KTX2_Header);
            if (buffer_size < level_index_offset + core::Unt64(level_count) * sizeof(KTX2_Level_Index)) return result;

            auto level_indices = reinterpret_cast<KTX2_Level_Index const*>(buffer + level_index_offset);
            result.valid = true;
            result.compressed_layout = layout;
            result.base_width = base_width;
            result.base_height = base_height;
            result.level_count = level_count;

            auto w = base_width;
            auto h = base_height;
            core::print("parse_ktx2 ", w, ",", h);

            for (int i = 0; i < level_count; i++) {
                auto& li = level_indices[i];
                auto byte_offset = read_u64(li.byte_offset);
                auto byte_length = read_u64(li.byte_length);

                if (byte_offset > buffer_size || byte_length > buffer_size - byte_offset) {
                    result.valid = false;
                    return result;
                }

                result.levels[i].width = w;
                result.levels[i].height = h;
                result.levels[i].data = buffer + byte_offset;
                result.levels[i].data_size = byte_length;

                w = (w > 1) ? w / 2 : 1;
                h = (h > 1) ? h / 2 : 1;
            }

            return result;
        }

        struct KTX2_Image_Reader final: resource::Image_Reader
        {
            auto unsupported(core::Strung path) -> bool override
            {
                // Only handle .ktx2 files.
                std::string text = path / s / std_string; 
 
                // Returns std::string::npos if not found
                size_t pos = text.find(".ktx2");
                if (pos != std::string::npos) {
                    return false;
                } 
                return true;

                //return !core::String_View{path}.ends_with(".ktx2");
            }

            auto unsupported_from(core::Buffer buffer, core::Strung path) -> bool override
            {
                // Check KTX2 magic in the buffer.
                if (buffer.num_items() < 12) return true;
                return !is_ktx2_magic(buffer.begin());
            }

            auto try_read(core::Path path, resource::Image_Layout il, resource::Image_Vertical_Flip vf) -> resource::Image override
            {
                // Read the entire file into memory.
                auto file = util::read_file(path);
                auto file_size = file.range().size();
                if (file_size == 0u) {
                    core::print("read file size 0");
                    return {};
                } 

                // Read file contents into a vector.
                auto file_data = std::vector<core::Byte>(core::Size(file_size));
                auto buf = core::Buffer_Slice{file_data.data(), core::Size(file_size)};
                file.read(buf);
                auto buffer_ptr = file_data.data();
                auto buffer_size = core::Unt64(file_size);

                return load_from_buffer(buffer_ptr, buffer_size, std::string{path.string().begin(), path.string().end()});
            }

            auto try_read_from(core::Buffer buffer, core::Path path, resource::Image_Layout il, resource::Image_Vertical_Flip vf) -> resource::Image override
            {
                return load_from_buffer(buffer.begin(), core::Unt64(buffer.num_items()), std::string{path.string().begin(), path.string().end()});
            }

        private:
            auto load_from_buffer(core::Byte const* data, core::Unt64 size, std::string source) -> resource::Image
            {
                auto parsed = parse_ktx2(data, size);
                if (!parsed.valid) return {};

                // For compressed textures, we return the base mip level (level 0) data.
                auto& level0 = parsed.levels[0];

                // Allocate a buffer and copy the compressed data.
                auto compressed_size = level0.data_size;
                auto compressed_buffer = new core::Byte[compressed_size];
                std::memcpy(compressed_buffer, level0.data, compressed_size);

                core::print("ktx Image ", level0.width, ",", level0.height, ",", parsed.compressed_layout);
                return resource::Image{
                    level0.width,
                    level0.height,
                    parsed.compressed_layout,
                    core::move(source),
                    compressed_buffer,
                    core::Size(compressed_size),
                    [] (core::Byte* x) -> void { delete[] x; },
                };
            }
        };
    }

    auto wild_load_ktx2_image_provider(resource::Image_Provider* provider) -> void
    {
        core::print("wild_load_ktx2_image_provider ");
        // Rank 50: lower than STB (100), so KTX2 is tried first for .ktx2 files.
        provider->add_image_reader<KTX2_Image_Reader>("ktx2"_s, 50);
    }
}
