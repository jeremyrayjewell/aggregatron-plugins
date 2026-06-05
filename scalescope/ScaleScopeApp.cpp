#include "ScaleScopeMainComponent.h"
#include "ScaleScopeState.h"

#if JUCE_WINDOWS
 #include <windows.h>
#endif

class AggregaScaleWindow : public juce::DocumentWindow,
                           private juce::Timer
{
public:
    explicit AggregaScaleWindow(AggregaScaleState& state)
        : juce::DocumentWindow("AggregaScale",
                               juce::Colours::black,
                               juce::DocumentWindow::allButtons)
    {
        auto content = std::make_unique<AggregaScaleMainComponent>(state);
        const auto width = content->getWidth();
        const auto height = content->getHeight();

        setUsingNativeTitleBar(true);
        setResizable(true, true);
        setResizeLimits(900, 540, 2200, 1400);
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

class AggregaScaleApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "AggregaScale"; }
    const juce::String getApplicationVersion() override { return "0.1"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        state = std::make_unique<AggregaScaleState>();
        mainWindow = std::make_unique<AggregaScaleWindow>(*state);
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
    std::unique_ptr<AggregaScaleWindow> mainWindow;
};

START_JUCE_APPLICATION(AggregaScaleApplication)
