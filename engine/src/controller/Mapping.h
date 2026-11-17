#pragma once

#include "MappingModel.h"
#include "MappingYaml.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace Controller {

class Mapping final {
public:
    struct MacroChange {
        int slot = -1;
        MacroSpec macro;
        int rawValue = 0;
        float normalized = 0.0f;
        float value = 0.0f;
    };

    struct Listeners {
        std::function<void(int slot)> armSlot;

        std::function<void(int pad, const PadBinding* binding, bool pressed)> padEvent;

        std::function<void(const MacroChange& change)> macroEvent;

        std::function<void(int bank, const MacroBankSpec* page)> bankEvent;

        std::function<void(int scene, const SceneSpec* selection)> sceneEvent;

        std::function<void(const std::shared_ptr<const SetMapping>& mapping, bool hotReload)>
            changed;

        std::function<void(const MappingError& error)> error;
    };

    static constexpr int defaultPollIntervalMs = 25;

    Mapping();
    ~Mapping();

    Mapping(const Mapping&) = delete;
    Mapping& operator=(const Mapping&) = delete;

    void setListeners(Listeners newListeners);

    bool load(const std::filesystem::path& file, MappingError* error = nullptr);

    bool loadProject(const std::filesystem::path& projectFile, MappingError* error = nullptr);

    bool reload(MappingError* error = nullptr);

    bool poll();

    bool startWatching(int intervalMs = defaultPollIntervalMs);

    void stopWatching();
    bool isWatching() const noexcept;

    std::filesystem::path getPath() const;
    std::shared_ptr<const SetMapping> getMapping() const;

    std::uint64_t getRevision() const noexcept;

    MappingError getLastError() const;

    std::optional<PadBinding> getPadBinding(int pad) const;
    std::optional<EncoderBinding> getEncoderBinding(int encoder) const;
    std::optional<MacroBankSpec> getBank(int bank) const;
    std::optional<SceneSpec> getScene(int scene) const;

    int getActivePad() const noexcept;

    int getArmedSlot() const noexcept;

    int getActiveBank() const noexcept;

    int getActiveScene() const noexcept;

    bool handlePad(int pad, bool pressed);

    bool handleEncoder(int encoder, int value);

    bool handleButton(int cc, bool pressed);

    bool handleTopRow(int index, bool pressed);

    bool handleRightColumn(int index, bool pressed);

    static std::filesystem::path findDefaultSetFile();

private:
    bool applyText(const std::string& text, bool hotReload, std::uint64_t contentHash, bool force,
                   MappingError* error);
    bool openSource(const std::filesystem::path& file, std::string archiveEntry,
                    MappingError* error);
    bool reloadInternal(bool hotReload, MappingError* error);
    void reportError(const MappingError& error, std::uint64_t contentHash, bool rememberHash);
    void watchLoop(int intervalMs);

    mutable std::mutex mutex;
    Listeners listeners;

    std::filesystem::path path;
    std::string archiveEntry;
    std::filesystem::file_time_type writeTime{};
    bool hasWriteTime = false;
    std::shared_ptr<const SetMapping> current;

    MappingError lastError;
    std::atomic<std::uint64_t> revision{0};
    std::uint64_t lastHash = 0;
    bool hasAttempted = false;
    bool hasLoaded = false;
    std::atomic<int> activePad{-1};
    std::atomic<int> activeBank{-1};
    std::atomic<int> activeScene{-1};

    std::mutex watchMutex;
    std::thread watcher;
    std::atomic<bool> quitWatcher{false};
    std::atomic<bool> watching{false};
};

}  // namespace Controller
