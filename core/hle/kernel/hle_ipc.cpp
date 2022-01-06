// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>
#include <cstring>

#include <boost/range/algorithm_ext/erase.hpp>

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/svc_results.h"
#include "core/hle/kernel/time_manager.h"

namespace Kernel {

SessionRequestHandler::SessionRequestHandler(const char* service_name_) {}

SessionRequestHandler::~SessionRequestHandler() = default;

SessionRequestManager::SessionRequestManager() {}

SessionRequestManager::~SessionRequestManager() = default;

bool SessionRequestManager::HasSessionRequestHandler(const HLERequestContext& context) const {
    if (IsDomain() && context.HasDomainMessageHeader()) {
        const auto& message_header = context.GetDomainMessageHeader();
        const auto object_id = message_header.object_id;

        if (object_id > DomainHandlerCount()) {
            LOG_CRITICAL(IPC, "object_id {} is too big!", object_id);
            return false;
        }
        return DomainHandler(object_id - 1) != nullptr;
    } else {
        return session_handler != nullptr;
    }
}

HLERequestContext::HLERequestContext(SessionRequestManager *manager_, u32_le* cmd_buf_)
    : manager(manager_), cmd_buf(cmd_buf_) {
    ParseCommandBuffer(cmd_buf, true);
}

HLERequestContext::~HLERequestContext() = default;

void HLERequestContext::ParseCommandBuffer(u32_le* src_cmdbuf, bool incoming) {
    IPC::RequestParser rp(src_cmdbuf);
    command_header = rp.PopRaw<IPC::CommandHeader>();

    if (command_header->IsCloseCommand()) {
        // Close does not populate the rest of the IPC header
        return;
    }

    // If handle descriptor is present, add size of it
    if (command_header->enable_handle_descriptor) {
        handle_descriptor_header = rp.PopRaw<IPC::HandleDescriptorHeader>();
        if (handle_descriptor_header->send_current_pid) {
            pid = rp.Pop<u64>();
        }
        if (incoming) {
            // Populate the object lists with the data in the IPC request.
            for (u32 handle = 0; handle < handle_descriptor_header->num_handles_to_copy; ++handle) {
                incoming_copy_handles.push_back(rp.Pop<Handle>());
            }
            for (u32 handle = 0; handle < handle_descriptor_header->num_handles_to_move; ++handle) {
                incoming_move_handles.push_back(rp.Pop<Handle>());
            }
        } else {
            // For responses we just ignore the handles, they're empty and will be populated when
            // translating the response.
            rp.Skip(handle_descriptor_header->num_handles_to_copy, false);
            rp.Skip(handle_descriptor_header->num_handles_to_move, false);
        }
    }

    for (u32 i = 0; i < command_header->num_buf_x_descriptors; ++i) {
        buffer_x_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorX>());
    }
    for (u32 i = 0; i < command_header->num_buf_a_descriptors; ++i) {
        buffer_a_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorABW>());
    }
    for (u32 i = 0; i < command_header->num_buf_b_descriptors; ++i) {
        buffer_b_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorABW>());
    }
    for (u32 i = 0; i < command_header->num_buf_w_descriptors; ++i) {
        buffer_w_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorABW>());
    }

    const auto buffer_c_offset = rp.GetCurrentOffset() + command_header->data_size;

    if (!command_header->IsTipc()) {
        // Padding to align to 16 bytes
        rp.AlignWithPadding();

        if (IsDomain() &&
            ((command_header->type == IPC::CommandType::Request ||
              command_header->type == IPC::CommandType::RequestWithContext) ||
             !incoming)) {
            // If this is an incoming message, only CommandType "Request" has a domain header
            // All outgoing domain messages have the domain header, if only incoming has it
            if (incoming || domain_message_header) {
                domain_message_header = rp.PopRaw<IPC::DomainMessageHeader>();
            } else {
                if (IsDomain()) {
                    LOG_WARNING(IPC, "Domain request has no DomainMessageHeader!");
                }
            }
        }

        data_payload_header = rp.PopRaw<IPC::DataPayloadHeader>();

        data_payload_offset = rp.GetCurrentOffset();

        if (domain_message_header &&
            domain_message_header->command ==
                IPC::DomainMessageHeader::CommandType::CloseVirtualHandle) {
            // CloseVirtualHandle command does not have SFC* or any data
            return;
        }

        if (incoming) {
            ASSERT(data_payload_header->magic == Common::MakeMagic('S', 'F', 'C', 'I'));
        } else {
            ASSERT(data_payload_header->magic == Common::MakeMagic('S', 'F', 'C', 'O'));
        }
    }

    rp.SetCurrentOffset(buffer_c_offset);

    // For Inline buffers, the response data is written directly to buffer_c_offset
    // and in this case we don't have any BufferDescriptorC on the request.
    if (command_header->buf_c_descriptor_flags >
        IPC::CommandHeader::BufferDescriptorCFlag::InlineDescriptor) {
        if (command_header->buf_c_descriptor_flags ==
            IPC::CommandHeader::BufferDescriptorCFlag::OneDescriptor) {
            buffer_c_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorC>());
        } else {
            u32 num_buf_c_descriptors =
                static_cast<u32>(command_header->buf_c_descriptor_flags.Value()) - 2;

            // This is used to detect possible underflows, in case something is broken
            // with the two ifs above and the flags value is == 0 || == 1.
            ASSERT(num_buf_c_descriptors < 14);

            for (u32 i = 0; i < num_buf_c_descriptors; ++i) {
                buffer_c_desciptors.push_back(rp.PopRaw<IPC::BufferDescriptorC>());
            }
        }
    }

    rp.SetCurrentOffset(data_payload_offset);

    command = rp.Pop<u32_le>();
    rp.Skip(1, false); // The command is actually an u64, but we don't use the high part.
}

ResultCode HLERequestContext::WriteToOutgoingCommandBuffer() {
    auto current_offset = handles_offset;

    for (int fd : outgoing_copy_fds) {
        Handle copy_handle = mizu_servctl(MIZU_SCTL_CREATE_COPY_HANDLE, fd);
        if (copy_handle == Svc::InvalidHandle) {
            LOG_CRITICAL(Core, "MIZU_SCTL_CREATE_COPY_HANDLE failed: {}", ResultCode(errno).description.Value());
        }
        cmd_buf[current_offset++] = copy_handle;
    }
    for (Handle handle : outgoing_move_handles) {
        cmd_buf[current_offset++] = handle;
    }

    // Write the domain objects to the command buffer, these go after the raw untranslated data.
    // TODO(Subv): This completely ignores C buffers.

    if (IsDomain()) {
        current_offset = domain_offset - static_cast<u32>(outgoing_domain_objects.size());
        for (const auto& object : outgoing_domain_objects) {
            AppendDomainHandler(object);
            cmd_buf[current_offset++] =
                static_cast<u32_le>(NumDomainRequestHandlers());
        }
    }

    // cmd_buf will be copied over by the kernel

    return ResultSuccess;
}

std::vector<u8> HLERequestContext::ReadBuffer(std::size_t buffer_index) const {
    std::vector<u8> buffer{};
    const bool is_buffer_a{BufferDescriptorA().size() > buffer_index &&
                           BufferDescriptorA()[buffer_index].Size()};

    if (is_buffer_a) {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorA().size() > buffer_index, { return buffer; },
            "BufferDescriptorA invalid buffer_index {}", buffer_index);
        buffer.resize(BufferDescriptorA()[buffer_index].Size());
        if (mizu_servctl(MIZU_SCTL_READ_BUFFER,
                         (unsigned long)BufferDescriptorA()[buffer_index].Address(),
                         (unsigned long)buffer.data(), buffer.size()) == -1) {
            LOG_CRITICAL(Core, "MIZU_SCTL_READ_BUFFER failed: {}", ResultCode(errno).description.Value());
        }
    } else {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorX().size() > buffer_index, { return buffer; },
            "BufferDescriptorX invalid buffer_index {}", buffer_index);
        buffer.resize(BufferDescriptorX()[buffer_index].Size());
        if (mizu_servctl(MIZU_SCTL_READ_BUFFER,
                         (unsigned long)BufferDescriptorX()[buffer_index].Address(),
                         (unsigned long)buffer.data(), buffer.size()) == -1) {
            LOG_CRITICAL(Core, "MIZU_SCTL_READ_BUFFER failed: {}", ResultCode(errno).description.Value());
        }
    }

    return buffer;
}

std::size_t HLERequestContext::WriteBuffer(const void* buffer, std::size_t size,
                                           std::size_t buffer_index) const {
    if (size == 0) {
        LOG_WARNING(Core, "skip empty buffer write");
        return 0;
    }

    const bool is_buffer_b{BufferDescriptorB().size() > buffer_index &&
                           BufferDescriptorB()[buffer_index].Size()};
    const std::size_t buffer_size{GetWriteBufferSize(buffer_index)};
    if (size > buffer_size) {
        LOG_CRITICAL(Core, "size ({:016X}) is greater than buffer_size ({:016X})", size,
                     buffer_size);
        size = buffer_size; // TODO(bunnei): This needs to be HW tested
    }

    if (is_buffer_b) {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorB().size() > buffer_index &&
                BufferDescriptorB()[buffer_index].Size() >= size,
            { return 0; }, "BufferDescriptorB is invalid, index={}, size={}", buffer_index, size);
        if (mizu_servctl(MIZU_SCTL_WRITE_BUFFER,
                         (unsigned long)BufferDescriptorB()[buffer_index].Address(),
                         (unsigned long)buffer, size) == -1)
            LOG_CRITICAL(Core, "MIZU_SCTL_WRITE_BUFFER failed: {}", ResultCode(errno).description.Value());
    } else {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorC().size() > buffer_index &&
                BufferDescriptorC()[buffer_index].Size() >= size,
            { return 0; }, "BufferDescriptorC is invalid, index={}, size={}", buffer_index, size);
        if (mizu_servctl(MIZU_SCTL_WRITE_BUFFER,
                         (unsigned long)BufferDescriptorC()[buffer_index].Address(),
                         (unsigned long)buffer, size) == -1)
            LOG_CRITICAL(Core, "MIZU_SCTL_WRITE_BUFFER failed: {}", ResultCode(errno).description.Value());
    }

    return size;
}

std::size_t HLERequestContext::GetReadBufferSize(std::size_t buffer_index) const {
    const bool is_buffer_a{BufferDescriptorA().size() > buffer_index &&
                           BufferDescriptorA()[buffer_index].Size()};
    if (is_buffer_a) {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorA().size() > buffer_index, { return 0; },
            "BufferDescriptorA invalid buffer_index {}", buffer_index);
        return BufferDescriptorA()[buffer_index].Size();
    } else {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorX().size() > buffer_index, { return 0; },
            "BufferDescriptorX invalid buffer_index {}", buffer_index);
        return BufferDescriptorX()[buffer_index].Size();
    }
}

std::size_t HLERequestContext::GetWriteBufferSize(std::size_t buffer_index) const {
    const bool is_buffer_b{BufferDescriptorB().size() > buffer_index &&
                           BufferDescriptorB()[buffer_index].Size()};
    if (is_buffer_b) {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorB().size() > buffer_index, { return 0; },
            "BufferDescriptorB invalid buffer_index {}", buffer_index);
        return BufferDescriptorB()[buffer_index].Size();
    } else {
        ASSERT_OR_EXECUTE_MSG(
            BufferDescriptorC().size() > buffer_index, { return 0; },
            "BufferDescriptorC invalid buffer_index {}", buffer_index);
        return BufferDescriptorC()[buffer_index].Size();
    }
    return 0;
}

bool HLERequestContext::CanReadBuffer(std::size_t buffer_index) const {
    const bool is_buffer_a{BufferDescriptorA().size() > buffer_index &&
                           BufferDescriptorA()[buffer_index].Size()};

    if (is_buffer_a) {
        return BufferDescriptorA().size() > buffer_index;
    } else {
        return BufferDescriptorX().size() > buffer_index;
    }
}

bool HLERequestContext::CanWriteBuffer(std::size_t buffer_index) const {
    const bool is_buffer_b{BufferDescriptorB().size() > buffer_index &&
                           BufferDescriptorB()[buffer_index].Size()};

    if (is_buffer_b) {
        return BufferDescriptorB().size() > buffer_index;
    } else {
        return BufferDescriptorC().size() > buffer_index;
    }
}

std::string HLERequestContext::Description() const {
    if (!command_header) {
        return "No command header available";
    }
    std::ostringstream s;
    s << "IPC::CommandHeader: Type:" << static_cast<u32>(command_header->type.Value());
    s << ", X(Pointer):" << command_header->num_buf_x_descriptors;
    if (command_header->num_buf_x_descriptors) {
        s << '[';
        for (u64 i = 0; i < command_header->num_buf_x_descriptors; ++i) {
            s << "0x" << std::hex << BufferDescriptorX()[i].Size();
            if (i < command_header->num_buf_x_descriptors - 1)
                s << ", ";
        }
        s << ']';
    }
    s << ", A(Send):" << command_header->num_buf_a_descriptors;
    if (command_header->num_buf_a_descriptors) {
        s << '[';
        for (u64 i = 0; i < command_header->num_buf_a_descriptors; ++i) {
            s << "0x" << std::hex << BufferDescriptorA()[i].Size();
            if (i < command_header->num_buf_a_descriptors - 1)
                s << ", ";
        }
        s << ']';
    }
    s << ", B(Receive):" << command_header->num_buf_b_descriptors;
    if (command_header->num_buf_b_descriptors) {
        s << '[';
        for (u64 i = 0; i < command_header->num_buf_b_descriptors; ++i) {
            s << "0x" << std::hex << BufferDescriptorB()[i].Size();
            if (i < command_header->num_buf_b_descriptors - 1)
                s << ", ";
        }
        s << ']';
    }
    s << ", C(ReceiveList):" << BufferDescriptorC().size();
    if (!BufferDescriptorC().empty()) {
        s << '[';
        for (u64 i = 0; i < BufferDescriptorC().size(); ++i) {
            s << "0x" << std::hex << BufferDescriptorC()[i].Size();
            if (i < BufferDescriptorC().size() - 1)
                s << ", ";
        }
        s << ']';
    }
    s << ", data_size:" << command_header->data_size.Value();

    return s.str();
}

void HLERequestContext::AppendDomainHandler(SessionRequestHandlerPtr handler) {
    manager->AppendDomainHandler(std::move(handler));
}

std::size_t HLERequestContext::NumDomainRequestHandlers() const {
    return manager->DomainHandlerCount();
}

} // namespace Kernel
