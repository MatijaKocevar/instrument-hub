#pragma once

namespace ihub::plugins
{
class PluginWindow;

class WindowCallbackToken
{
public:
    PluginWindow* owner = nullptr;
    bool alive = false;
};

}
