// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.
//
// Adapted by Kent Hall for mizu on Horizon Linux.

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#include <unordered_set>
#include <sys/types.h>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/concepts.h"
#include "common/swap.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/svc_common.h"

union ResultCode;

namespace IPC {
class ResponseBuilder;
}

namespace Kernel {
class SessionRequestManager;
}

namespace Service {
extern thread_local std::unordered_set<std::shared_ptr<Kernel::SessionRequestManager>> session_managers;
decltype(session_managers)::iterator FindSessionManager(unsigned long session_id);
}

namespace Kernel {

class HLERequestContext;

/**
 * Interface implemented by HLE Session handlers.
 * This can be provided to a ServerSession in order to hook into several relevant events
 * (such as a new connection or a SyncRequest) so they can be implemented in the emulator.
 */
class SessionRequestHandler {
public:
    SessionRequestHandler(const char* service_name_);
    virtual ~SessionRequestHandler();

    virtual void SetupSession(::pid_t requester_pid) {}
    virtual void CleanupSession(::pid_t requester_pid) {}

    /**
     * Handles a sync request from the emulated application.
     * @param server_session The ServerSession that was triggered for this sync request,
     * it should be used to differentiate which client (As in ClientSession) we're answering to.
     * TODO(Subv): Use a wrapper structure to hold all the information relevant to
     * this request (ServerSession, Originator thread, Translated command buffer, etc).
     * @returns ResultCode the result code of the translate operation.
     */
    virtual ResultCode HandleSyncRequest(Kernel::HLERequestContext& context) = 0;
};

using SessionRequestHandlerPtr = std::shared_ptr<SessionRequestHandler>;

/**
 * Manages the underlying HLE requests for a session, and whether (or not) the session should be
 * treated as a domain. This is managed separately from server sessions, as this state is shared
 * when objects are cloned.
 */
class SessionRequestManager final {
public:
    explicit SessionRequestManager();
    ~SessionRequestManager();

    unsigned long GetId() const {
        return reinterpret_cast<unsigned long>(this);
    }

    ::pid_t GetRequesterPid() const {
        return requester_pid;
    }

    bool IsDomain() const {
        return is_domain;
    }

    void ConvertToDomain() {
        domain_handlers = {session_handler};
        is_domain = true;
    }

    std::size_t DomainHandlerCount() const {
        return domain_handlers.size();
    }

    bool HasSessionHandler() const {
        return session_handler != nullptr;
    }

    SessionRequestHandler& SessionHandler() {
        return *session_handler;
    }

    const SessionRequestHandler& SessionHandler() const {
        return *session_handler;
    }

    void CloseDomainHandler(std::size_t index) {
        if (index < DomainHandlerCount()) {
            domain_handlers[index] = nullptr;
        } else {
            UNREACHABLE_MSG("Unexpected handler index {}", index);
        }
    }

    SessionRequestHandlerPtr DomainHandler(std::size_t index) const {
        ASSERT_MSG(index < DomainHandlerCount(), "Unexpected handler index {}", index);
        return domain_handlers.at(index);
    }

    void AppendDomainHandler(SessionRequestHandlerPtr&& handler) {
        domain_handlers.emplace_back(std::move(handler));
    }

    void SetSessionHandler(SessionRequestHandlerPtr&& handler) {
        if (session_handler) {
            session_handler->CleanupSession(requester_pid);
	}
        session_handler = std::move(handler);
        session_handler->SetupSession(requester_pid);
    }

    bool HasSessionRequestHandler(const HLERequestContext& context) const;

private:
    bool is_domain{};
    SessionRequestHandlerPtr session_handler;
    std::vector<SessionRequestHandlerPtr> domain_handlers;
    ::pid_t requester_pid;
};

/**
 * Class containing information about an in-flight IPC request being handled by an HLE service
 * implementation. Services should avoid using old global APIs (e.g. Kernel::GetCommandBuffer()) and
 * when possible use the APIs in this class to service the request.
 *
 * HLE handle protocol
 * ===================
 *
 * To avoid needing HLE services to keep a separate handle table, or having to directly modify the
 * requester's table, a tweaked protocol is used to receive and send handles in requests. The kernel
 * will decode the incoming handles into object pointers and insert a id in the buffer where the
 * handle would normally be. The service then calls GetIncomingHandle() with that id to get the
 * pointer to the object. Similarly, instead of inserting a handle into the command buffer, the
 * service calls AddOutgoingHandle() and stores the returned id where the handle would normally go.
 *
 * The end result is similar to just giving services their own real handle tables, but since these
 * ids are local to a specific context, it avoids requiring services to manage handles for objects
 * across multiple calls and ensuring that unneeded handles are cleaned up.
 */
class HLERequestContext {
public:
    explicit HLERequestContext(SessionRequestManager *manager, u32_le* cmd_buf);
    ~HLERequestContext();

    /// Returns a pointer to the IPC command buffer for this request.
    u32* CommandBuffer() {
        return cmd_buf;
    }

    unsigned long GetSessionId() {
	    return manager->GetId();
    }

    ::pid_t GetRequesterPid() {
	    return manager->GetRequesterPid();
    }

    /// Adds a new domain request handler to the collection of request handlers within
    /// this ServerSession instance.
    void AppendDomainHandler(SessionRequestHandlerPtr handler);

    /// Retrieves the total number of domain request handlers that have been
    /// appended to this ServerSession instance.
    std::size_t NumDomainRequestHandlers() const;

    /// Returns true if the session has been converted to a domain, otherwise False
    bool IsDomain() const {
	    return manager->IsDomain();
    }

    /// Converts the session to a domain at the end of the current command
    void ConvertToDomain() {
        convert_to_domain = true;
    }

    /// Gets the session request manager, which forwards requests to the underlying service
    std::shared_ptr<SessionRequestManager> GetSessionRequestManagerShared() {
        auto manager = *Service::FindSessionManager(GetSessionId());
	if (manager == nullptr) {
            LOG_CRITICAL(IPC, "invalid manager (id={}) in context!", GetSessionId());
	}
	return manager;
    }

    /// Writes data from this context back to the requesting process/thread.
    ResultCode WriteToOutgoingCommandBuffer();

    u32_le GetHipcCommand() const {
        return command;
    }

    u32_le GetTipcCommand() const {
        return static_cast<u32_le>(command_header->type.Value()) -
               static_cast<u32_le>(IPC::CommandType::TIPC_CommandRegion);
    }

    u32_le GetCommand() const {
        return command_header->IsTipc() ? GetTipcCommand() : GetHipcCommand();
    }

    bool IsTipc() const {
        return command_header->IsTipc();
    }

    IPC::CommandType GetCommandType() const {
        return command_header->type;
    }

    u64 GetPID() const {
        return pid;
    }

    u32 GetDataPayloadOffset() const {
        return data_payload_offset;
    }

    const std::vector<IPC::BufferDescriptorX>& BufferDescriptorX() const {
        return buffer_x_desciptors;
    }

    const std::vector<IPC::BufferDescriptorABW>& BufferDescriptorA() const {
        return buffer_a_desciptors;
    }

    const std::vector<IPC::BufferDescriptorABW>& BufferDescriptorB() const {
        return buffer_b_desciptors;
    }

    const std::vector<IPC::BufferDescriptorC>& BufferDescriptorC() const {
        return buffer_c_desciptors;
    }

    const IPC::DomainMessageHeader& GetDomainMessageHeader() const {
        return domain_message_header.value();
    }

    bool HasDomainMessageHeader() const {
        return domain_message_header.has_value();
    }

    /// Helper function to read a buffer using the appropriate buffer descriptor
    std::vector<u8> ReadBuffer(std::size_t buffer_index = 0) const;

    /// Helper function to write a buffer using the appropriate buffer descriptor
    std::size_t WriteBuffer(const void* buffer, std::size_t size,
                            std::size_t buffer_index = 0) const;

    /* Helper function to write a buffer using the appropriate buffer descriptor
     *
     * @tparam T an arbitrary container that satisfies the
     *         ContiguousContainer concept in the C++ standard library or a trivially copyable type.
     *
     * @param data         The container/data to write into a buffer.
     * @param buffer_index The buffer in particular to write to.
     */
    template <typename T, typename = std::enable_if_t<!std::is_pointer_v<T>>>
    std::size_t WriteBuffer(const T& data, std::size_t buffer_index = 0) const {
        if constexpr (Common::IsSTLContainer<T>) {
            using ContiguousType = typename T::value_type;
            static_assert(std::is_trivially_copyable_v<ContiguousType>,
                          "Container to WriteBuffer must contain trivially copyable objects");
            return WriteBuffer(std::data(data), std::size(data) * sizeof(ContiguousType),
                               buffer_index);
        } else {
            static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
            return WriteBuffer(&data, sizeof(T), buffer_index);
        }
    }

    /// Helper function to get the size of the input buffer
    std::size_t GetReadBufferSize(std::size_t buffer_index = 0) const;

    /// Helper function to get the size of the output buffer
    std::size_t GetWriteBufferSize(std::size_t buffer_index = 0) const;

    /// Helper function to test whether the input buffer at buffer_index can be read
    bool CanReadBuffer(std::size_t buffer_index = 0) const;

    /// Helper function to test whether the output buffer at buffer_index can be written
    bool CanWriteBuffer(std::size_t buffer_index = 0) const;

    Handle GetCopyHandle(std::size_t index) const {
        return incoming_copy_handles.at(index);
    }

    Handle GetMoveHandle(std::size_t index) const {
        return incoming_move_handles.at(index);
    }

    void AddMoveHandle(Handle handle) {
        outgoing_move_handles.push_back(handle);
    }

    void AddCopyFd(int fd) {
        outgoing_copy_fds.push_back(fd);
    }

    void AddDomainObject(SessionRequestHandlerPtr object) {
        outgoing_domain_objects.push_back(std::move(object));
    }

    template <typename T>
    std::shared_ptr<T> GetDomainHandler(std::size_t index) const {
        return std::static_pointer_cast<T>(manager->DomainHandler(index));
    }

    std::string Description() const;

    bool IsThreadWaiting() const {
        return is_thread_waiting;
    }

    /// When set to True, converts the session to a domain at the end of the command
    bool convert_to_domain{};

private:
    friend class IPC::ResponseBuilder;

    void ParseCommandBuffer(u32_le* src_cmdbuf, bool incoming);

    u32 *cmd_buf = nullptr;

    std::vector<Handle> incoming_move_handles;
    std::vector<Handle> incoming_copy_handles;

    std::vector<Handle> outgoing_move_handles;
    std::vector<int> outgoing_copy_fds;
    std::vector<SessionRequestHandlerPtr> outgoing_domain_objects;

    std::optional<IPC::CommandHeader> command_header;
    std::optional<IPC::HandleDescriptorHeader> handle_descriptor_header;
    std::optional<IPC::DataPayloadHeader> data_payload_header;
    std::optional<IPC::DomainMessageHeader> domain_message_header;
    std::vector<IPC::BufferDescriptorX> buffer_x_desciptors;
    std::vector<IPC::BufferDescriptorABW> buffer_a_desciptors;
    std::vector<IPC::BufferDescriptorABW> buffer_b_desciptors;
    std::vector<IPC::BufferDescriptorABW> buffer_w_desciptors;
    std::vector<IPC::BufferDescriptorC> buffer_c_desciptors;

    u32_le command{};
    u64 pid{};
    u32 write_size{};
    u32 data_payload_offset{};
    u32 handles_offset{};
    u32 domain_offset{};

    SessionRequestManager *manager;
    bool is_thread_waiting{};
};

} // namespace Kernel
