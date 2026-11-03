#pragma once

#include <qtclasshelpermacros.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "qwayland-ext-workspace-v1.h"

struct wl_display;
struct wl_event_queue;
struct wl_output;
struct wl_registry;

namespace caelestia::screenshot {

class WorkspaceSnapshot final : private QtWayland::ext_workspace_manager_v1 {
public:
    Q_DISABLE_COPY_MOVE(WorkspaceSnapshot)

    WorkspaceSnapshot(wl_display* display, wl_registry* registry, uint32_t name);
    ~WorkspaceSnapshot() override;

    [[nodiscard]] ::ext_workspace_handle_v1* activeWorkspace(wl_output* output) const;

private:
    class Group;
    class Workspace;

    void ext_workspace_manager_v1_workspace_group(::ext_workspace_group_handle_v1* group) override;
    void ext_workspace_manager_v1_workspace(::ext_workspace_handle_v1* workspace) override;

    wl_event_queue* m_queue;
    std::vector<std::unique_ptr<Group>> m_groups;
    std::vector<std::unique_ptr<Workspace>> m_workspaces;
};

} // namespace caelestia::screenshot
