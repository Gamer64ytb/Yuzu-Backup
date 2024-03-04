// SPDX-FileCopyrightText: Ryujinx Team and Contributors
// SPDX-License-Identifier: MIT

#include <array>
#include <bit>

#include "common/scratch_buffer.h"
#include "common/settings.h"
#include "video_core/host1x/codecs/h264.h"
#include "video_core/host1x/host1x.h"
#include "video_core/memory_manager.h"

namespace Tegra::Decoder {
namespace {
// ZigZag LUTs from libavcodec.
constexpr std::array<u8, 64> zig_zag_direct{
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
    41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
    30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

constexpr std::array<u8, 16> zig_zag_scan{
    0 + 0 * 4, 1 + 0 * 4, 0 + 1 * 4, 0 + 2 * 4, 1 + 1 * 4, 2 + 0 * 4, 3 + 0 * 4, 2 + 1 * 4,
    1 + 2 * 4, 0 + 3 * 4, 1 + 3 * 4, 2 + 2 * 4, 3 + 1 * 4, 3 + 2 * 4, 2 + 3 * 4, 3 + 3 * 4,
};
} // Anonymous namespace

H264::H264(Host1x::Host1x& host1x_) : host1x{host1x_} {}

H264::~H264() = default;

std::span<const u8> H264::ComposeFrame(const Host1x::NvdecCommon::NvdecRegisters& state,
                                       size_t* out_configuration_size, bool is_first_frame) {
    H264DecoderContext context;
    host1x.GMMU().ReadBlock(state.picture_info_offset, &context, sizeof(H264DecoderContext));

    const s64 frame_number = context.h264_parameter_set.frame_number.Value();
    if (!is_first_frame && frame_number != 0) {
        frame.resize_destructive(context.stream_len);
        host1x.GMMU().ReadBlock(state.frame_bitstream_offset, frame.data(), frame.size());
        *out_configuration_size = 0;
        return frame;
    }

    // Encode header
    H264BitWriter writer{};
    writer.WriteU(1, 24);
    writer.WriteU(0, 1);
    writer.WriteU(3, 2);
    writer.WriteU(7, 5);
    writer.WriteU(100, 8);
    writer.WriteU(0, 8);
    writer.WriteU(31, 8);
    writer.WriteUe(0);
    const u32 chroma_format_idc =
        static_cast<u32>(context.h264_parameter_set.chroma_format_idc.Value());
    writer.WriteUe(chroma_format_idc);
    if (chroma_format_idc == 3) {
        writer.WriteBit(false);
    }

    writer.WriteUe(0);
    writer.WriteUe(0);
    writer.WriteBit(false); // QpprimeYZeroTransformBypassFlag
    writer.WriteBit(false); // Scaling matrix present flag

    writer.WriteUe(static_cast<u32>(context.h264_parameter_set.log2_max_frame_num_minus4.Value()));

    const auto order_cnt_type =
        static_cast<u32>(context.h264_parameter_set.pic_order_cnt_type.Value());
    writer.WriteUe(order_cnt_type);
    if (order_cnt_type == 0) {
        writer.WriteUe(context.h264_parameter_set.log2_max_pic_order_cnt_lsb_minus4);
    } else if (order_cnt_type == 1) {
        writer.WriteBit(context.h264_parameter_set.delta_pic_order_always_zero_flag != 0);

        writer.WriteSe(0);
        writer.WriteSe(0);
        writer.WriteUe(0);
    }

    const s32 pic_height = context.h264_parameter_set.frame_height_in_map_units /
                           (context.h264_parameter_set.frame_mbs_only_flag ? 1 : 2);

    // TODO (ameerj): Where do we get this number, it seems to be particular for each stream
    const auto nvdec_decoding = Settings::values.nvdec_emulation.GetValue();
    const bool uses_gpu_decoding = nvdec_decoding == Settings::NvdecEmulation::Gpu;
    const u32 max_num_ref_frames = uses_gpu_decoding ? 6u : 16u;
    writer.WriteUe(max_num_ref_frames);
    writer.WriteBit(false);
    writer.WriteUe(context.h264_parameter_set.pic_width_in_mbs - 1);
    writer.WriteUe(pic_height - 1);
    writer.WriteBit(context.h264_parameter_set.frame_mbs_only_flag != 0);

    if (!context.h264_parameter_set.frame_mbs_only_flag) {
        writer.WriteBit(context.h264_parameter_set.flags.mbaff_frame.Value() != 0);
    }

    writer.WriteBit(context.h264_parameter_set.flags.direct_8x8_inference.Value() != 0);
    writer.WriteBit(false); // Frame cropping flag
    writer.WriteBit(false); // VUI parameter present flag

    writer.End();

    // H264 PPS
    writer.WriteU(1, 24);
    writer.WriteU(0, 1);
    writer.WriteU(3, 2);
    writer.WriteU(8, 5);

    writer.WriteUe(0);
    writer.WriteUe(0);

    writer.WriteBit(context.h264_parameter_set.entropy_coding_mode_flag != 0);
    writer.WriteBit(context.h264_parameter_set.pic_order_present_flag != 0);
    writer.WriteUe(0);
    writer.WriteUe(context.h264_parameter_set.num_refidx_l0_default_active);
    writer.WriteUe(context.h264_parameter_set.num_refidx_l1_default_active);
    writer.WriteBit(context.h264_parameter_set.flags.weighted_pred.Value() != 0);
    writer.WriteU(static_cast<s32>(context.h264_parameter_set.weighted_bipred_idc.Value()), 2);
    s32 pic_init_qp = static_cast<s32>(context.h264_parameter_set.pic_init_qp_minus26.Value());
    writer.WriteSe(pic_init_qp);
    writer.WriteSe(0);
    s32 chroma_qp_index_offset =
        static_cast<s32>(context.h264_parameter_set.chroma_qp_index_offset.Value());

    writer.WriteSe(chroma_qp_index_offset);
    writer.WriteBit(context.h264_parameter_set.deblocking_filter_control_present_flag != 0);
    writer.WriteBit(context.h264_parameter_set.flags.constrained_intra_pred.Value() != 0);
    writer.WriteBit(context.h264_parameter_set.redundant_pic_cnt_present_flag != 0);
    writer.WriteBit(context.h264_parameter_set.transform_8x8_mode_flag != 0);

    writer.WriteBit(true); // pic_scaling_matrix_present_flag

    for (s32 index = 0; index < 6; index++) {
        writer.WriteBit(true);
        std::span<const u8> matrix{context.weight_scale};
        writer.WriteScalingList(scan, matrix, index * 16, 16);
    }

    if (context.h264_parameter_set.transform_8x8_mode_flag) {
        for (s32 index = 0; index < 2; index++) {
            writer.WriteBit(true);
            std::span<const u8> matrix{context.weight_scale_8x8};
            writer.WriteScalingList(scan, matrix, index * 64, 64);
        }
    }

    s32 chroma_qp_index_offset2 =
        static_cast<s32>(context.h264_parameter_set.second_chroma_qp_index_offset.Value());

    writer.WriteSe(chroma_qp_index_offset2);

    writer.End();

    const auto& encoded_header = writer.GetByteArray();
    frame.resize(encoded_header.size() + context.stream_len);
    std::memcpy(frame.data(), encoded_header.data(), encoded_header.size());

    *out_configuration_size = encoded_header.size();
    host1x.GMMU().ReadBlock(state.frame_bitstream_offset, frame.data() + encoded_header.size(),
                            context.stream_len);

    return frame;
}

H264BitWriter::H264BitWriter() = default;

H264BitWriter::~H264BitWriter() = default;

void H264BitWriter::WriteU(s32 value, s32 value_sz) {
    WriteBits(value, value_sz);
}

void H264BitWriter::WriteSe(s32 value) {
    WriteExpGolombCodedInt(value);
}

void H264BitWriter::WriteUe(u32 value) {
    WriteExpGolombCodedUInt(value);
}

void H264BitWriter::End() {
    WriteBit(true);
    Flush();
}

void H264BitWriter::WriteBit(bool state) {
    WriteBits(state ? 1 : 0, 1);
}

void H264BitWriter::WriteScalingList(Common::ScratchBuffer<u8>& scan, std::span<const u8> list,
                                     s32 start, s32 count) {
    scan.resize_destructive(count);
    if (count == 16) {
        std::memcpy(scan.data(), zig_zag_scan.data(), scan.size());
    } else {
        std::memcpy(scan.data(), zig_zag_direct.data(), scan.size());
    }
    u8 last_scale = 8;

    for (s32 index = 0; index < count; index++) {
        const u8 value = list[start + scan[index]];
        const s32 delta_scale = static_cast<s32>(value - last_scale);

        WriteSe(delta_scale);

        last_scale = value;
    }
}

std::vector<u8>& H264BitWriter::GetByteArray() {
    return byte_array;
}

const std::vector<u8>& H264BitWriter::GetByteArray() const {
    return byte_array;
}

void H264BitWriter::WriteBits(s32 value, s32 bit_count) {
    s32 value_pos = 0;

    s32 remaining = bit_count;

    while (remaining > 0) {
        s32 copy_size = remaining;

        const s32 free_bits = GetFreeBufferBits();

        if (copy_size > free_bits) {
            copy_size = free_bits;
        }

        const s32 mask = (1 << copy_size) - 1;

        const s32 src_shift = (bit_count - value_pos) - copy_size;
        const s32 dst_shift = (buffer_size - buffer_pos) - copy_size;

        buffer |= ((value >> src_shift) & mask) << dst_shift;

        value_pos += copy_size;
        buffer_pos += copy_size;
        remaining -= copy_size;
    }
}

void H264BitWriter::WriteExpGolombCodedInt(s32 value) {
    const s32 sign = value <= 0 ? 0 : 1;
    if (value < 0) {
        value = -value;
    }
    value = (value << 1) - sign;
    WriteExpGolombCodedUInt(value);
}

void H264BitWriter::WriteExpGolombCodedUInt(u32 value) {
    const s32 size = 32 - std::countl_zero(value + 1);
    WriteBits(1, size);

    value -= (1U << (size - 1)) - 1;
    WriteBits(static_cast<s32>(value), size - 1);
}

s32 H264BitWriter::GetFreeBufferBits() {
    if (buffer_pos == buffer_size) {
        Flush();
    }

    return buffer_size - buffer_pos;
}

void H264BitWriter::Flush() {
    if (buffer_pos == 0) {
        return;
    }
    byte_array.push_back(static_cast<u8>(buffer));

    buffer = 0;
    buffer_pos = 0;
}
} // namespace Tegra::Decoder
