#pragma once
#include <core/resource/image-provider.hpp>

namespace ss::resource::wild_image
{
    CCTT_INTROSPECT(.image_provider{true});
    auto wild_load_ktx2_image_provider(resource::Image_Provider* provider) -> void;
}
