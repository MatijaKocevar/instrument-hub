#pragma once

#include <juce_core/juce_core.h>

#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace ihub::plugins
{
    class SandboxSlotControl;
}

namespace ihub::session
{

    class MirrorSync
    {
    public:
        struct ParameterEdit
        {
            juce::String id;
            float normalisedValue = 0.0f;
        };

        using SandboxLookup = std::function<plugins::SandboxSlotControl*(int slot)>;

        MirrorSync (int slotCapacity, SandboxLookup sandboxLookup);
        ~MirrorSync ();

        MirrorSync (const MirrorSync&) = delete;
        MirrorSync& operator= (const MirrorSync&) = delete;

        void parameterChanged (int slot, const juce::String& id, float normalisedValue);
        void stateChanged (int slot, juce::MemoryBlock state);
        void clear ();
        void shutdown ();

    private:
        struct Pending
        {
            std::map<juce::String, float> parameters;
            juce::MemoryBlock state;
            bool stateDirty = false;
            double lastEditMs = 0.0;
        };

        void run ();
        void applyState (int slot, juce::MemoryBlock state);
        void applyParameters (int slot, std::vector<ParameterEdit> edits);

        static constexpr int maximumParameterBatch = 64;
        static constexpr double parameterSettleMs = 10.0;
        static constexpr double stateSettleMs = 150.0;

        const int capacity;
        SandboxLookup lookupSandbox;

        mutable std::mutex lock;
        std::condition_variable wake;
        std::vector<Pending> pending;
        std::thread worker;
        bool quit = false;
    };

}
