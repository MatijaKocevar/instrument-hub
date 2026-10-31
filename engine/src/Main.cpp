#include <juce_gui_extra/juce_gui_extra.h>

#include <iostream>
#include <memory>

namespace
{
constexpr int kWindowWidth = 1200;
constexpr int kWindowHeight = 800;

// Empty content area for the skeleton. The React UI replaces this in T002.
class MainComponent final : public juce::Component
{
public:
    MainComponent() = default;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b1b1f));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow (const juce::String& name)
        : juce::DocumentWindow (name,
                                juce::Colour (0xff1b1b1f),
                                juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent(), true);

        setResizable (true, false);
        setResizeLimits (480, 320, 8192, 8192);
        centreWithSize (kWindowWidth, kWindowHeight);
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};
} // namespace

class InstrumentHubApplication final : public juce::JUCEApplication,
                                       private juce::Timer
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    void initialise (const juce::String& commandLine) override
    {
        mainWindow = std::make_unique<MainWindow> (getApplicationName());

        if (commandLine.contains ("--selftest"))
        {
            std::cout << "selftest: window opened " << kWindowWidth << "x" << kWindowHeight
                      << ", quitting in 3 s" << std::endl;
            startTimer (3000);
        }
    }

    void shutdown() override
    {
        stopTimer();
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override {}

private:
    void timerCallback() override
    {
        stopTimer();

        if (mainWindow == nullptr || ! mainWindow->isOnDesktop() || ! mainWindow->isVisible())
        {
            std::cerr << "selftest: FAILED, window is not on the desktop" << std::endl;
            setApplicationReturnValue (1);
            quit();
            return;
        }

        const auto bounds = mainWindow->getScreenBounds();
        std::cout << "selftest: window on desktop at " << bounds.getWidth() << "x"
                  << bounds.getHeight() << ", quitting cleanly" << std::endl;
        quit();
    }

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (InstrumentHubApplication)
