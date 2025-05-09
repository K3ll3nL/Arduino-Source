/*  Image Filters RGB32 Range
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifdef PA_AutoDispatch_arm64_20_M1

#include "Kernels/Kernels_arm64_NEON.h"
#include "Kernels/PartialWordAccess/Kernels_PartialWordAccess_arm64_NEON.h"
#include "Kernels/ImageFilters/Kernels_ImageFilter_Basic_Routines.h"
#include "Kernels/ImageFilters/Kernels_ImageFilter_Basic_Routines_ARM64_NEON.h"
#include "Kernels_ImageFilter_RGB32_Range_Routines.h"
#include "Kernels_ImageFilter_RGB32_Range.h"



namespace PokemonAutomation{
namespace Kernels{




class PixelTest_Rgb32Range_ARM64_NEON{
public:
    static const size_t VECTOR_SIZE = 4;
    using Mask = size_t;

public:
    PA_FORCE_INLINE PixelTest_Rgb32Range_ARM64_NEON(
        uint32_t mins, uint32_t maxs
    )
        : m_mins_u8(vreinterpretq_u8_u32(vdupq_n_u32(mins)))
        , m_maxs_u8(vreinterpretq_u8_u32(vdupq_n_u32(maxs)))
        , m_zeros_u8(vreinterpretq_u32_u8(vdupq_n_u8(0)))
    {}
    PA_FORCE_INLINE PixelTest_Rgb32Range_ARM64_NEON(
        const ToBlackWhiteRgb32RangeFilter& filter
    )
        : m_mins_u8(vreinterpretq_u8_u32(vdupq_n_u32(filter.mins)))
        , m_maxs_u8(vreinterpretq_u8_u32(vdupq_n_u32(filter.maxs)))
        , m_zeros_u8(vreinterpretq_u32_u8(vdupq_n_u8(0)))
    {}

    //  Return a mask indicating which lanes are in range.
    PA_FORCE_INLINE uint32x4_t test_word(uint32x4_t& pixel) const{
        uint8x16_t in_u8 = vreinterpretq_u8_u32(pixel);

        // Check if mins > pixel per color channel
        uint8x16_t cmp0 = vcgtq_u8(m_mins_u8, in_u8);
        // Check if pixel > maxs per color channel
        uint8x16_t cmp1 = vcgtq_u8(in_u8, m_maxs_u8);
        // cmp: if mins > pixel or pixel > maxs per color channel
        uint8x16_t cmp_u8 = vorrq_u8(cmp0, cmp1);
        // vceqq_u32: compare bitwise equal
        // cmp_u32: if each pixel is within the range
        // If a pixel is within [mins, maxs], its uint32_t in `cmp_u32` is all 1 bits, otherwise, all 0 bits
        return vceqq_u32(vreinterpretq_u32_u8(cmp_u8), vreinterpretq_u32_u8(m_zeros_u8));
    }

private:
    uint8x16_t m_mins_u8;
    uint8x16_t m_maxs_u8;
    uint8x16_t m_zeros_u8;
};






#if 0

class ImageFilterRunner_Rgb32Range_ARM64_NEON{
public:
    static const size_t VECTOR_SIZE = 4;
    using Mask = size_t;

public:
    ImageFilterRunner_Rgb32Range_ARM64_NEON(
        uint32_t replacement_color, bool replace_color_within_range,
        uint32_t mins, uint32_t maxs
    )
        : m_mins_u8(vreinterpretq_u8_u32(vdupq_n_u32(mins)))
        , m_maxs_u8(vreinterpretq_u8_u32(vdupq_n_u32(maxs)))
        , m_zeros_u8(vreinterpretq_u32_u8(vdupq_n_u8(0)))
        , m_replacement_color_u32(vdupq_n_u32(replacement_color))
        , m_replace_color_within_range(replace_color_within_range)
        , m_count_u32(vdupq_n_u32(0))
    {}
    ImageFilterRunner_Rgb32Range_ARM64_NEON(FilterRgb32RangeFilter& filter)
        : ImageFilterRunner_Rgb32Range_ARM64_NEON(filter.mins, filter.maxs, filter.replacement, filter.invert)
    {}

    PA_FORCE_INLINE size_t count() const{
        // long pairwise add
        uint64x2_t sum_u64 = vpaddlq_u32(m_count_u32);
        return sum_u64[0] + sum_u64[1];
    }

    // Given 4 pixels from in[4], apply color range comparison and count the pixels that are in range.
    // The counts are stored in m_count_u32.
    // If a per-pixel mask, cmp_mask_u32 is not nullptr, it only counts the pixels covered by the mask.
    // It also changes pixels in or out of the range to have the new color m_replacement_color_u32.
    // The resulting pixels are saved in out[4]
    PA_FORCE_INLINE void process_full(uint32_t out[4], const uint32_t in[4], const uint32x4_t* cmp_mask_u32 = nullptr){
        uint8x16_t in_u8 = vreinterpretq_u8_u32(vld1q_u32(in));

        // Check if mins > pixel per color channel
        uint8x16_t cmp0 = vcgtq_u8(m_mins_u8, in_u8);
        // Check if pixel > maxs per color channel
        uint8x16_t cmp1 = vcgtq_u8(in_u8, m_maxs_u8);
        // cmp: if mins > pixel or pixel > maxs per color channel
        uint8x16_t cmp_u8 = vorrq_u8(cmp0, cmp1);
        // vceqq_u32: compare bitwise equal
        // cmp_u32: if each pixel is within the range
        // If a pixel is within [mins, maxs], its uint32_t in `cmp_u32` is all 1 bits, otherwise, all 0 bits
        uint32x4_t cmp_u32 = vceqq_u32(vreinterpretq_u32_u8(cmp_u8), vreinterpretq_u32_u8(m_zeros_u8));
        if (cmp_mask_u32) {
            cmp_u32 = vandq_u32(cmp_u32, *cmp_mask_u32);
        }
        // Increase count for each pixel in range. Each uint32 lane is counted separately.
        // We achieve +=1 by substracting 0xFFFFFFFF
        m_count_u32 = vsubq_u32(m_count_u32, cmp_u32);
        // select replacement color or in_u8 based on cmp_u32:
        uint32x4_t out_u32;
        if (m_replace_color_within_range){
            // vbslq_u32(a, b, c) for 1 bits in a, choose b; for 0 bits in a, choose c
            out_u32 = vbslq_u32(cmp_u32, m_replacement_color_u32, vreinterpretq_u32_u8(in_u8));
        }else{
            out_u32 = vbslq_u32(cmp_u32, vreinterpretq_u32_u8(in_u8), m_replacement_color_u32);
        }
        vst1q_u32(out, out_u32);
    }
    // Same as `process_full()` but only process `left` (< 4) pixels
    PA_FORCE_INLINE void process_partial(uint32_t* out, const uint32_t* in, size_t left){
        uint32x4_t cmp_mask_u32 = vreinterpretq_u32_u8(PartialWordAccess_arm64_NEON::create_front_mask(left * 4));
        uint32_t buffer_in[4], buffer_out[4];
        memcpy(buffer_in, in, sizeof(uint32_t) * left);
        process_full(buffer_out, buffer_in, &cmp_mask_u32);
        memcpy(out, buffer_out, sizeof(uint32_t) * left);
    }

private:
    uint8x16_t m_mins_u8;
    uint8x16_t m_maxs_u8;
    uint8x16_t m_zeros_u8;
    uint32x4_t m_replacement_color_u32;
    bool m_replace_color_within_range;
    uint32x4_t m_count_u32;
};

#endif


size_t filter_rgb32_range_arm64_NEON(
    const uint32_t* in, size_t in_bytes_per_row, size_t width, size_t height,
    uint32_t* out, size_t out_bytes_per_row,
    uint32_t replacement, bool replace_color_within_range,
    uint32_t mins, uint32_t maxs
){
    PixelTest_Rgb32Range_ARM64_NEON tester(mins, maxs);
    FilterImage_Rgb32_ARM64_NEON<PixelTest_Rgb32Range_ARM64_NEON> filter(
        tester,
        replacement, replace_color_within_range
    );
//    ImageFilterRunner_Rgb32Range_ARM64_NEON filter(
//        replacement, replace_color_within_range,
//        mins, maxs
//    );
    filter_per_pixel(in, in_bytes_per_row, width, height, filter, out, out_bytes_per_row);
    return filter.count();
}

#if 0
void filter_rgb32_range_arm64_NEON(
    const uint32_t* image, size_t bytes_per_row, size_t width, size_t height,
    FilterRgb32RangeFilter* filter, size_t filter_count
){
    filter_per_pixel<ImageFilterRunner_Rgb32Range_ARM64_NEON>(
        image, bytes_per_row, width, height, filter, filter_count
    );
}
#endif





size_t to_blackwhite_rgb32_range_arm64_NEON(
    const uint32_t* in, size_t in_bytes_per_row, size_t width, size_t height,
    uint32_t* out, size_t out_bytes_per_row,
    bool in_range_black,
    uint32_t mins, uint32_t maxs
){
    PixelTest_Rgb32Range_ARM64_NEON tester(mins, maxs);
    ToBlackWhite_Rgb32_ARM64_NEON<PixelTest_Rgb32Range_ARM64_NEON> filter(
        tester, in_range_black
    );
    filter_per_pixel(in, in_bytes_per_row, width, height, filter, out, out_bytes_per_row);
    return filter.count();
}






}
}
#endif
