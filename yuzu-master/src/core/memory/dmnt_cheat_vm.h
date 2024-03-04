// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <variant>
#include <vector>
#include <fmt/printf.h>
#include "common/common_types.h"
#include "core/memory/dmnt_cheat_types.h"

namespace Core::Memory {

enum class CheatVmOpcodeType : u32 {
    StoreStatic = 0,
    BeginConditionalBlock = 1,
    EndConditionalBlock = 2,
    ControlLoop = 3,
    LoadRegisterStatic = 4,
    LoadRegisterMemory = 5,
    StoreStaticToAddress = 6,
    PerformArithmeticStatic = 7,
    BeginKeypressConditionalBlock = 8,

    // These are not implemented by Gateway's VM.
    PerformArithmeticRegister = 9,
    StoreRegisterToAddress = 10,
    Reserved11 = 11,

    // This is a meta entry, and not a real opcode.
    // This is to facilitate multi-nybble instruction decoding.
    ExtendedWidth = 12,

    // Extended width opcodes.
    BeginRegisterConditionalBlock = 0xC0,
    SaveRestoreRegister = 0xC1,
    SaveRestoreRegisterMask = 0xC2,
    ReadWriteStaticRegister = 0xC3,

    // This is a meta entry, and not a real opcode.
    // This is to facilitate multi-nybble instruction decoding.
    DoubleExtendedWidth = 0xF0,

    // Double-extended width opcodes.
    PauseProcess = 0xFF0,
    ResumeProcess = 0xFF1,
    DebugLog = 0xFFF,
};

enum class MemoryAccessType : u32 {
    MainNso = 0,
    Heap = 1,
    Alias = 2,
    Aslr = 3,
};

enum class ConditionalComparisonType : u32 {
    GT = 1,
    GE = 2,
    LT = 3,
    LE = 4,
    EQ = 5,
    NE = 6,
};

enum class RegisterArithmeticType : u32 {
    Addition = 0,
    Subtraction = 1,
    Multiplication = 2,
    LeftShift = 3,
    RightShift = 4,

    // These are not supported by Gateway's VM.
    LogicalAnd = 5,
    LogicalOr = 6,
    LogicalNot = 7,
    LogicalXor = 8,

    None = 9,
};

enum class StoreRegisterOffsetType : u32 {
    None = 0,
    Reg = 1,
    Imm = 2,
    MemReg = 3,
    MemImm = 4,
    MemImmReg = 5,
};

enum class CompareRegisterValueType : u32 {
    MemoryRelAddr = 0,
    MemoryOfsReg = 1,
    RegisterRelAddr = 2,
    RegisterOfsReg = 3,
    StaticValue = 4,
    OtherRegister = 5,
};

enum class SaveRestoreRegisterOpType : u32 {
    Restore = 0,
    Save = 1,
    ClearSaved = 2,
    ClearRegs = 3,
};

enum class DebugLogValueType : u32 {
    MemoryRelAddr = 0,
    MemoryOfsReg = 1,
    RegisterRelAddr = 2,
    RegisterOfsReg = 3,
    RegisterValue = 4,
};

union VmInt {
    u8 bit8;
    u16 bit16;
    u32 bit32;
    u64 bit64;
};

struct StoreStaticOpcode {
    u32 bit_width{};
    MemoryAccessType mem_type{};
    u32 offset_register{};
    u64 rel_address{};
    VmInt value{};
};

struct BeginConditionalOpcode {
    u32 bit_width{};
    MemoryAccessType mem_type{};
    ConditionalComparisonType cond_type{};
    u64 rel_address{};
    VmInt value{};
};

struct EndConditionalOpcode {
    bool is_else;
};

struct ControlLoopOpcode {
    bool start_loop{};
    u32 reg_index{};
    u32 num_iters{};
};

struct LoadRegisterStaticOpcode {
    u32 reg_index{};
    u64 value{};
};

struct LoadRegisterMemoryOpcode {
    u32 bit_width{};
    MemoryAccessType mem_type{};
    u32 reg_index{};
    bool load_from_reg{};
    u64 rel_address{};
};

struct StoreStaticToAddressOpcode {
    u32 bit_width{};
    u32 reg_index{};
    bool increment_reg{};
    bool add_offset_reg{};
    u32 offset_reg_index{};
    u64 value{};
};

struct PerformArithmeticStaticOpcode {
    u32 bit_width{};
    u32 reg_index{};
    RegisterArithmeticType math_type{};
    u32 value{};
};

struct BeginKeypressConditionalOpcode {
    u32 key_mask{};
};

struct PerformArithmeticRegisterOpcode {
    u32 bit_width{};
    RegisterArithmeticType math_type{};
    u32 dst_reg_index{};
    u32 src_reg_1_index{};
    u32 src_reg_2_index{};
    bool has_immediate{};
    VmInt value{};
};

struct StoreRegisterToAddressOpcode {
    u32 bit_width{};
    u32 str_reg_index{};
    u32 addr_reg_index{};
    bool increment_reg{};
    StoreRegisterOffsetType ofs_type{};
    MemoryAccessType mem_type{};
    u32 ofs_reg_index{};
    u64 rel_address{};
};

struct BeginRegisterConditionalOpcode {
    u32 bit_width{};
    ConditionalComparisonType cond_type{};
    u32 val_reg_index{};
    CompareRegisterValueType comp_type{};
    MemoryAccessType mem_type{};
    u32 addr_reg_index{};
    u32 other_reg_index{};
    u32 ofs_reg_index{};
    u64 rel_address{};
    VmInt value{};
};

struct SaveRestoreRegisterOpcode {
    u32 dst_index{};
    u32 src_index{};
    SaveRestoreRegisterOpType op_type{};
};

struct SaveRestoreRegisterMaskOpcode {
    SaveRestoreRegisterOpType op_type{};
    std::array<bool, 0x10> should_operate{};
};

struct ReadWriteStaticRegisterOpcode {
    u32 static_idx{};
    u32 idx{};
};

struct PauseProcessOpcode {};

struct ResumeProcessOpcode {};

struct DebugLogOpcode {
    u32 bit_width{};
    u32 log_id{};
    DebugLogValueType val_type{};
    MemoryAccessType mem_type{};
    u32 addr_reg_index{};
    u32 val_reg_index{};
    u32 ofs_reg_index{};
    u64 rel_address{};
};

struct UnrecognizedInstruction {
    CheatVmOpcodeType opcode{};
};

struct CheatVmOpcode {
    bool begin_conditional_block{};
    std::variant<StoreStaticOpcode, BeginConditionalOpcode, EndConditionalOpcode, ControlLoopOpcode,
                 LoadRegisterStaticOpcode, LoadRegisterMemoryOpcode, StoreStaticToAddressOpcode,
                 PerformArithmeticStaticOpcode, BeginKeypressConditionalOpcode,
                 PerformArithmeticRegisterOpcode, StoreRegisterToAddressOpcode,
                 BeginRegisterConditionalOpcode, SaveRestoreRegisterOpcode,
                 SaveRestoreRegisterMaskOpcode, ReadWriteStaticRegisterOpcode, PauseProcessOpcode,
                 ResumeProcessOpcode, DebugLogOpcode, UnrecognizedInstruction>
        opcode{};
};

class DmntCheatVm {
public:
    /// Helper Type for DmntCheatVm <=> yuzu Interface
    class Callbacks {
    public:
        virtual ~Callbacks();

        virtual void MemoryReadUnsafe(VAddr address, void* data, u64 size) = 0;
        virtual void MemoryWriteUnsafe(VAddr address, const void* data, u64 size) = 0;

        virtual u64 HidKeysDown() = 0;

        virtual void PauseProcess() = 0;
        virtual void ResumeProcess() = 0;

        virtual void DebugLog(u8 id, u64 value) = 0;
        virtual void CommandLog(std::string_view data) = 0;
    };

    static constexpr std::size_t MaximumProgramOpcodeCount = 0x400;
    static constexpr std::size_t NumRegisters = 0x10;
    static constexpr std::size_t NumReadableStaticRegisters = 0x80;
    static constexpr std::size_t NumWritableStaticRegisters = 0x80;
    static constexpr std::size_t NumStaticRegisters =
        NumReadableStaticRegisters + NumWritableStaticRegisters;

    explicit DmntCheatVm(std::unique_ptr<Callbacks> callbacks_);
    ~DmntCheatVm();

    std::size_t GetProgramSize() const {
        return this->num_opcodes;
    }

    bool LoadProgram(const std::vector<CheatEntry>& cheats);
    void Execute(const CheatProcessMetadata& metadata);

private:
    std::unique_ptr<Callbacks> callbacks;

    std::size_t num_opcodes = 0;
    std::size_t instruction_ptr = 0;
    std::size_t condition_depth = 0;
    bool decode_success = false;
    std::array<u32, MaximumProgramOpcodeCount> program{};
    std::array<u64, NumRegisters> registers{};
    std::array<u64, NumRegisters> saved_values{};
    std::array<u64, NumStaticRegisters> static_registers{};
    std::array<std::size_t, NumRegisters> loop_tops{};

    bool DecodeNextOpcode(CheatVmOpcode& out);
    void SkipConditionalBlock(bool is_if);
    void ResetState();

    // For implementing the DebugLog opcode.
    void DebugLog(u32 log_id, u64 value);

    void LogOpcode(const CheatVmOpcode& opcode);

    static u64 GetVmInt(VmInt value, u32 bit_width);
    static u64 GetCheatProcessAddress(const CheatProcessMetadata& metadata,
                                      MemoryAccessType mem_type, u64 rel_address);
};

}; // namespace Core::Memory
