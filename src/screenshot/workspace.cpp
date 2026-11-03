#include "workspace.hpp"

#include <qlist.h>

#include <wayland-client-protocol.h>

namespace caelestia::screenshot {

class WorkspaceSnapshot::Group final : public QtWayland::ext_workspace_group_handle_v1 {
public:
    explicit Group(::ext_workspace_group_handle_v1* object)
        : QtWayland::ext_workspace_group_handle_v1(object) {}

    ~Group() override { destroy(); }

    [[nodiscard]] bool contains(wl_output* output) const { return m_outputs.contains(output); }

    [[nodiscard]] const QList<::ext_workspace_handle_v1*>& workspaces() const { return m_workspaces; }

protected:
    void ext_workspace_group_handle_v1_output_enter(wl_output* output) override { m_outputs << output; }

    void ext_workspace_group_handle_v1_output_leave(wl_output* output) override { m_outputs.removeAll(output); }

    void ext_workspace_group_handle_v1_workspace_enter(::ext_workspace_handle_v1* workspace) override {
        m_workspaces << workspace;
    }

    void ext_workspace_group_handle_v1_workspace_leave(::ext_workspace_handle_v1* workspace) override {
        m_workspaces.removeAll(workspace);
    }

private:
    QList<wl_output*> m_outputs;
    QList<::ext_workspace_handle_v1*> m_workspaces;
};

class WorkspaceSnapshot::Workspace final : public QtWayland::ext_workspace_handle_v1 {
public:
    explicit Workspace(::ext_workspace_handle_v1* object)
        : QtWayland::ext_workspace_handle_v1(object) {}

    ~Workspace() override { destroy(); }

    [[nodiscard]] bool visible() const { return (m_state & state_active) && !(m_state & state_hidden); }

protected:
    void ext_workspace_handle_v1_state(uint32_t state) override { m_state = state; }

private:
    uint32_t m_state = 0;
};

WorkspaceSnapshot::WorkspaceSnapshot(wl_display* display, wl_registry* registry, uint32_t name)
    : m_queue(wl_display_create_queue(display)) {
    auto* const wrapper = static_cast<wl_registry*>(wl_proxy_create_wrapper(registry));
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapper), m_queue);
    init(wrapper, name, 1);
    wl_proxy_wrapper_destroy(wrapper);

    stop();
    wl_display_roundtrip_queue(display, m_queue);
}

WorkspaceSnapshot::~WorkspaceSnapshot() {
    m_workspaces.clear();
    m_groups.clear();
    wl_proxy_destroy(reinterpret_cast<wl_proxy*>(object()));
    wl_event_queue_destroy(m_queue);
}

::ext_workspace_handle_v1* WorkspaceSnapshot::activeWorkspace(wl_output* output) const {
    for (const auto& group : m_groups) {
        if (!group->contains(output))
            continue;
        for (auto* handle : group->workspaces()) {
            if (static_cast<const Workspace*>(Workspace::fromObject(handle))->visible())
                return handle;
        }
    }
    return nullptr;
}

void WorkspaceSnapshot::ext_workspace_manager_v1_workspace_group(::ext_workspace_group_handle_v1* group) {
    m_groups.push_back(std::make_unique<Group>(group));
}

void WorkspaceSnapshot::ext_workspace_manager_v1_workspace(::ext_workspace_handle_v1* workspace) {
    m_workspaces.push_back(std::make_unique<Workspace>(workspace));
}

} // namespace caelestia::screenshot
