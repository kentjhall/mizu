// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Tegra::Shader {

enum class OutputTopology : u32 {
    PointList = 1,
    LineStrip = 6,
    TriangleStrip = 7,
};

enum class AttributeUse : u8 {
    Unused = 0,
    Constant = 1,
    Perspective = 2,
    ScreenLinear = 3,
};

// Documentation in:
// http://download.nvidia.com/open-gpu-doc/Shader-Program-Header/1/Shader-Program-Header.html#ImapTexture
struct Header {
    union {
        BitField<0, 5, u32> sph_type;
        BitField<5, 5, u32> version;
        BitField<10, 4, u32> shader_type;
        BitField<14, 1, u32> mrt_enable;
        BitField<15, 1, u32> kills_pixels;
        BitField<16, 1, u32> does_global_store;
        BitField<17, 4, u32> sass_version;
        BitField<21, 5, u32> reserved;
        BitField<26, 1, u32> does_load_or_store;
        BitField<27, 1, u32> does_fp64;
        BitField<28, 4, u32> stream_out_mask;
    } common0{};

    union {
        BitField<0, 24, u32> shader_local_memory_low_size;
        BitField<24, 8, u32> per_patch_attribute_count;
    } common1{};

    union {
        BitField<0, 24, u32> shader_local_memory_high_size;
        BitField<24, 8, u32> threads_per_input_primitive;
    } common2{};

    union {
        BitField<0, 24, u32> shader_local_memory_crs_size;
        BitField<24, 4, OutputTopology> output_topology;
        BitField<28, 4, u32> reserved;
    } common3{};

    union {
        BitField<0, 12, u32> max_output_vertices;
        BitField<12, 8, u32> store_req_start; // NOTE: not used by geometry shaders.
        BitField<24, 4, u32> reserved;
        BitField<12, 8, u32> store_req_end; // NOTE: not used by geometry shaders.
    } common4{};

    union {
        struct {
            INSERT_PADDING_BYTES_NOINIT(3);  // ImapSystemValuesA
            INSERT_PADDING_BYTES_NOINIT(1);  // ImapSystemValuesB
            INSERT_PADDING_BYTES_NOINIT(16); // ImapGenericVector[32]
            INSERT_PADDING_BYTES_NOINIT(2);  // ImapColor
            union {
                BitField<0, 8, u16> clip_distances;
                BitField<8, 1, u16> point_sprite_s;
                BitField<9, 1, u16> point_sprite_t;
                BitField<10, 1, u16> fog_coordinate;
                BitField<12, 1, u16> tessellation_eval_point_u;
                BitField<13, 1, u16> tessellation_eval_point_v;
                BitField<14, 1, u16> instance_id;
                BitField<15, 1, u16> vertex_id;
            };
            INSERT_PADDING_BYTES_NOINIT(5);  // ImapFixedFncTexture[10]
            INSERT_PADDING_BYTES_NOINIT(1);  // ImapReserved
            INSERT_PADDING_BYTES_NOINIT(3);  // OmapSystemValuesA
            INSERT_PADDING_BYTES_NOINIT(1);  // OmapSystemValuesB
            INSERT_PADDING_BYTES_NOINIT(16); // OmapGenericVector[32]
            INSERT_PADDING_BYTES_NOINIT(2);  // OmapColor
            INSERT_PADDING_BYTES_NOINIT(2);  // OmapSystemValuesC
            INSERT_PADDING_BYTES_NOINIT(5);  // OmapFixedFncTexture[10]
            INSERT_PADDING_BYTES_NOINIT(1);  // OmapReserved
        } vtg;

        struct {
            INSERT_PADDING_BYTES_NOINIT(3); // ImapSystemValuesA
            INSERT_PADDING_BYTES_NOINIT(1); // ImapSystemValuesB
            union {
                BitField<0, 2, AttributeUse> x;
                BitField<2, 2, AttributeUse> y;
                BitField<4, 2, AttributeUse> w;
                BitField<6, 2, AttributeUse> z;
                u8 raw;
            } imap_generic_vector[32];
            INSERT_PADDING_BYTES_NOINIT(2);  // ImapColor
            INSERT_PADDING_BYTES_NOINIT(2);  // ImapSystemValuesC
            INSERT_PADDING_BYTES_NOINIT(10); // ImapFixedFncTexture[10]
            INSERT_PADDING_BYTES_NOINIT(2);  // ImapReserved
            struct {
                u32 target;
                union {
                    BitField<0, 1, u32> sample_mask;
                    BitField<1, 1, u32> depth;
                    BitField<2, 30, u32> reserved;
                };
            } omap;
            bool IsColorComponentOutputEnabled(u32 render_target, u32 component) const {
                const u32 bit = render_target * 4 + component;
                return omap.target & (1 << bit);
            }
            AttributeUse GetAttributeIndexUse(u32 attribute, u32 index) const {
                return static_cast<AttributeUse>(
                    (imap_generic_vector[attribute].raw >> (index * 2)) & 0x03);
            }
            AttributeUse GetAttributeUse(u32 attribute) const {
                AttributeUse result = AttributeUse::Unused;
                for (u32 i = 0; i < 4; i++) {
                    const auto index = GetAttributeIndexUse(attribute, i);
                    if (index == AttributeUse::Unused) {
                        continue;
                    }
                    if (result == AttributeUse::Unused || result == index) {
                        result = index;
                        continue;
                    }
                    LOG_CRITICAL(HW_GPU, "Generic Attribute Conflict in Interpolation Mode");
                    if (index == AttributeUse::Perspective) {
                        result = index;
                    }
                }
                return result;
            }
        } ps;

        std::array<u32, 0xF> raw{};
    };

    u64 GetLocalMemorySize() const {
        return (common1.shader_local_memory_low_size |
                (common2.shader_local_memory_high_size << 24));
    }
};

static_assert(sizeof(Header) == 0x50, "Incorrect structure size");

} // namespace Tegra::Shader
