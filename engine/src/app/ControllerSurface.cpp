#include "ControllerSurface.h"

#include "api/MidiBridge.h"
#include "api/Server.h"
#include "app/surface/TrackSurface.h"
#include "controller/LaunchpadDriver.h"
#include "controller/Mapping.h"

#include <iostream>
#include <utility>

namespace ihub::app {

ControllerSurface::ControllerSurface() = default;

ControllerSurface::~ControllerSurface() {
  resetMapping();
  shutdown();
}

void ControllerSurface::start(Api::StateStore* state, Api::MidiBridge* bridge,
                              const juce::File& projectFile) {
  apiState = state;
  midiBridge = bridge;

  if (projectFile.existsAsFile()) loadProjectFile(projectFile);

  launchpad = std::make_unique<Controller::LaunchpadDriver>();

  if (midiBridge != nullptr) midiBridge->setLaunchpad(launchpad.get());

  Controller::LaunchpadDriver::DriverListeners driverListeners;

  driverListeners.padEvent = [this](int pad, bool pressed) {
    if (mapping != nullptr) mapping->handlePad(pad, pressed);
  };

  driverListeners.buttonEvent = [this](int cc, bool pressed) {
    if (mapping != nullptr && mapping->handleButton(cc, pressed)) return;

    (void)handleBrowseButton(cc, pressed);
  };

  launchpad->setListeners(std::move(driverListeners));

  if (midiBridge != nullptr)
    midiBridge->setPadSink([this](int pad, bool pressed) {
      return mapping != nullptr && mapping->handlePad(pad, pressed);
    });

  if (!launchpad->connect())
    std::cerr << "controller: " << launchpad->getLastError() << std::endl;
  else
    refreshLeds();
}

void ControllerSurface::loadProjectFile(const juce::File& projectFile) {
  createMapping();

  mapping->stopWatching();

  Controller::MappingError error;

  if (!mapping->loadProject(projectFile.getFullPathName().toStdString(),
                            &error)) {
    std::cerr << "mapping: " << error.message << std::endl;
    return;
  }

  mapping->startWatching();
}

void ControllerSurface::createMapping() {
  if (mapping != nullptr) return;

  mapping = std::make_unique<Controller::Mapping>();

  Controller::Mapping::Listeners mappingListeners;

  mappingListeners.armSlot = [this](int slot) {
    if (slot >= 0 && apiState != nullptr) {
      (void)apiState->setArmedSlot(slot);
      refreshLeds();
    }
  };

  mappingListeners.padEvent = [this](int, const Controller::PadBinding* binding,
                                     bool pressed) {
    if (pressed && binding != nullptr && apiState != nullptr)
      applyTrackPadAction(*apiState, *binding);

    refreshLeds();
  };

  mappingListeners.bankEvent = [this](int bank,
                                      const Controller::MacroBankSpec*) {
    if (launchpad == nullptr) return;

    for (int index = 0; index < Launchpad::topRowCount; ++index)
      launchpad->setButtonState(Launchpad::topRowCCForIndex(index),
                                index == bank ? Controller::PadState::active
                                              : Controller::PadState::idle);
  };

  mappingListeners.sceneEvent = [this](int scene,
                                       const Controller::SceneSpec*) {
    if (launchpad == nullptr) return;

    for (int index = 0; index < Launchpad::rightColumnCount; ++index)
      launchpad->setButtonState(Launchpad::rightColumnCCForIndex(index),
                                index == scene ? Controller::PadState::active
                                               : Controller::PadState::idle);
  };

  mapping->setListeners(std::move(mappingListeners));
}

void ControllerSurface::resetMapping() {
  if (midiBridge != nullptr) midiBridge->setPadSink(nullptr);

  mapping.reset();
}

void ControllerSurface::shutdown() {
  if (midiBridge != nullptr) midiBridge->setPadSink(nullptr);

  if (launchpad != nullptr) {
    launchpad->disconnect();
    launchpad.reset();
  }

  midiBridge = nullptr;
}

}  // namespace ihub::app
