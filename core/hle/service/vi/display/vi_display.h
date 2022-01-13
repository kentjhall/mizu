// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Service::NVFlinger {
class BufferQueue;
}
namespace Service::KernelHelpers {
class ServiceContext;
} // namespace Service::KernelHelpers

namespace Service::VI {

class Layer;

/// Represents a single display type
class Display {
    YUZU_NON_COPYABLE(Display);
    YUZU_NON_MOVEABLE(Display);

public:
    /// Constructs a display with a given unique ID and name.
    ///
    /// @param id The unique ID for this display.
    /// @param service_context_ The ServiceContext for the owning service.
    /// @param name_ The name for this display.
    ///
    Display(u64 id, std::string name_);
    ~Display();

    /// Gets the unique ID assigned to this display.
    u64 GetID() const {
        return display_id;
    }

    /// Gets the name of this display
    const std::string& GetName() const {
        return name;
    }

    /// Whether or not this display has any layers added to it.
    bool HasLayers() const {
        return !layers.empty();
    }

    /// Gets a layer for this display based off an index.
    std::shared_ptr<Layer> GetLayer(std::size_t index);

    /// Gets the readable vsync event.
    int GetVSyncEvent() const;

    /// Signals the internal vsync event.
    void SignalVSyncEvent();

    /// Creates and adds a layer to this display with the given ID.
    ///
    /// @param layer_id     The ID to assign to the created layer.
    /// @param buffer_queue The buffer queue for the layer instance to use.
    /// @param pid          The PID of the requesting thread for the layer.
    ///
    void CreateLayer(u64 layer_id, NVFlinger::BufferQueue& buffer_queue, ::pid_t pid);

    /// Closes and removes a layer from this display with the given ID.
    ///
    /// @param layer_id The ID assigned to the layer to close.
    ///
    void CloseLayer(u64 layer_id);

    /// Closes/removes any layers from this display with the specified session.
    ///
    /// @param pid The PID of the session requester.
    ///
    void CloseSessionLayers(::pid_t pid);

    /// Attempts to find a layer with the given ID.
    ///
    /// @param layer_id The layer ID.
    ///
    /// @returns If found, the Layer instance with the given ID.
    ///          If not found, then nullptr is returned.
    ///
    Layer* FindLayer(u64 layer_id);

    /// Attempts to find a layer with the given ID.
    ///
    /// @param layer_id The layer ID.
    ///
    /// @returns If found, the Layer instance with the given ID.
    ///          If not found, then nullptr is returned.
    ///
    const Layer* FindLayer(u64 layer_id) const;

private:
    u64 display_id;
    std::string name;

    std::vector<std::shared_ptr<Layer>> layers;
    int vsync_event{};
};

} // namespace Service::VI
