#include "April10MainComponent.h"
#include "ScaleScopeState.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

class April10Window : public juce::DocumentWindow,
                      private juce::Timer
{
public:
    explicit April10Window(AggregaScaleState& state)
        : juce::DocumentWindow("April 10",
                               juce::Colours::black,
                               juce::DocumentWindow::allButtons)
    {
        auto content = std::make_unique<April10MainComponent>(state);
        const auto width = content->getWidth();
        const auto height = content->getHeight();

        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setResizeLimits(360, 110, 700, 240);
        setContentOwned(content.release(), true);
        centreWithSize(width, height);
        setVisible(true);
        toFront(true);
        startTimer(150);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    void timerCallback() override
    {
       #if JUCE_WINDOWS
        if (auto* peer = getPeer())
            if (auto hwnd = static_cast<HWND>(peer->getNativeHandle()))
            {
                ShowWindow(hwnd, SW_RESTORE);
                ShowWindow(hwnd, SW_SHOW);
                SetWindowPos(hwnd, HWND_TOP, 120, 120, getWidth(), getHeight(), SWP_SHOWWINDOW);
                SetForegroundWindow(hwnd);
                BringWindowToTop(hwnd);
                UpdateWindow(hwnd);
            }
       #endif
        stopTimer();
    }
};

class April10Application : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "April 10"; }
    const juce::String getApplicationVersion() override { return "0.1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        state = std::make_unique<AggregaScaleState>();
        mainWindow = std::make_unique<April10Window>(*state);
    }

    void shutdown() override
    {
        mainWindow.reset();
        state.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

private:
    std::unique_ptr<AggregaScaleState> state;
    std::unique_ptr<April10Window> mainWindow;
};

START_JUCE_APPLICATION(April10Application)
