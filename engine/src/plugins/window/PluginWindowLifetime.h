#pragma once

namespace ihub::plugins
{
class PluginWindow;

/** Message-thread lifetime token shared with the callbacks that may outlive
    a PluginWindow controller (the instrument's before-unload hook and the
    window's asynchronous close request). When the controller dies, `alive` is
    cleared before anything else, so those callbacks become no-ops. */
class WindowCallbackToken
{
public:
    PluginWindow* owner = nullptr;
    bool alive = false;
};

} // namespace ihub::plugins
