#include "FileRender.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <thread>

namespace
{
class MainComponent final : public juce::Component, private juce::Timer
{
public:
    MainComponent() : progressBar(displayProgress)
    {
        addAndMakeVisible(open);
        addAndMakeVisible(exportButton);
        addAndMakeVisible(fileName);
        addAndMakeVisible(status);
        addAndMakeVisible(title);
        addChildComponent(progressBar);
        title.setText("Blackie Vocal Box", juce::dontSendNotification);
        title.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
        fileName.setText("Choose a mono or stereo vocal file", juce::dontSendNotification);
        exportButton.setEnabled(false);
        open.onClick = [this] { chooseInput(); };
        exportButton.onClick = [this]
        {
            if (worker.joinable())
            {
                cancel.store(true);
                exportButton.setEnabled(false);
                status.setText("Cancelling export…", juce::dontSendNotification);
            }
            else
            {
                chooseOutput();
            }
        };

        const blackie::RenderSettings defaults;
        const char* names[] { "AI Noise", "Room Tail", "Natural EQ", "Auto EQ (Brian)" };
        const float amounts[] { defaults.cleanup.noise, defaults.cleanup.room, defaults.cleanup.tone, defaults.autoEq };
        for (std::size_t i = 0; i < controls.size(); ++i)
        {
            auto& control = controls[i];
            control.setSliderStyle(juce::Slider::LinearHorizontal);
            control.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 26);
            control.setRange(0, 100, 1);
            control.setValue(100 * amounts[i]);
            control.setTextValueSuffix("%");
            control.setTitle(names[i]);
            labels[i].setText(names[i], juce::dontSendNotification);
            addAndMakeVisible(control);
            addAndMakeVisible(labels[i]);
        }

        setSize(590, 380);
    }

    ~MainComponent() override
    {
        stopTimer();
        cancel.store(true);
        if (worker.joinable())
            worker.join();
    }

    void paint(juce::Graphics& graphics) override
    {
        graphics.fillAll(juce::Colour(0xff20242b));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(24);
        title.setBounds(area.removeFromTop(43));
        area.removeFromTop(10);
        auto fileRow = area.removeFromTop(32);
        open.setBounds(fileRow.removeFromLeft(105));
        fileRow.removeFromLeft(10);
        fileName.setBounds(fileRow);
        area.removeFromTop(16);
        for (std::size_t i = 0; i < controls.size(); ++i)
        {
            auto row = area.removeFromTop(37);
            labels[i].setBounds(row.removeFromLeft(130));
            controls[i].setBounds(row);
        }
        area.removeFromTop(10);
        auto exportRow = area.removeFromTop(34);
        exportButton.setBounds(exportRow.removeFromRight(130));
        exportRow.removeFromRight(12);
        progressBar.setBounds(exportRow.reduced(0, 5));
        status.setBounds(area);
    }

private:
    void chooseInput()
    {
        chooser = std::make_unique<juce::FileChooser>("Open a vocal file", input, "*.wav;*.mp3;*.aif;*.aiff;*.flac");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                            [self = juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& dialog)
        {
            if (self != nullptr && dialog.getResult().existsAsFile())
            {
                self->input = dialog.getResult();
                self->fileName.setText(self->input.getFileName(), juce::dontSendNotification);
                self->status.setText({}, juce::dontSendNotification);
                self->exportButton.setEnabled(true);
            }
        });
    }

    void chooseOutput()
    {
        const auto suggestion = input.getSiblingFile(input.getFileNameWithoutExtension() + " - Blackie.wav");
        chooser = std::make_unique<juce::FileChooser>("Export processed WAV", suggestion, "*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::warnAboutOverwriting,
                            [self = juce::Component::SafePointer<MainComponent>(this)](const juce::FileChooser& dialog)
        {
            if (self != nullptr && dialog.getResult() != juce::File())
            {
                const auto destination = dialog.getResult();
                if (!destination.hasFileExtension(".wav"))
                {
                    self->status.setText("Use a .wav extension for the export.", juce::dontSendNotification);
                    return;
                }
                self->startExport(destination);
            }
        });
    }

    void startExport(juce::File destination)
    {
        const auto amount = [this](std::size_t i) { return static_cast<float>(controls[i].getValue() / 100); };
        const blackie::RenderSettings settings { { amount(0), amount(1), amount(2) }, amount(3) };
        progress.store(0);
        displayProgress = 0;
        cancel.store(false);
        finished.store(false);
        open.setEnabled(false);
        for (auto& control : controls)
            control.setEnabled(false);
        exportButton.setButtonText("Cancel");
        progressBar.setVisible(true);
        status.setText("Exporting 48 kHz WAV…", juce::dontSendNotification);

        worker = std::thread([this, source = input, destination, settings]
        {
            try
            {
                const auto result = blackie::renderVocalFile(source, destination, settings, cancel, progress);
                exportMessage = result.wasOk() ? "Saved " + destination.getFileName() : result.getErrorMessage();
            }
            catch (const std::exception& error)
            {
                exportMessage = "Could not export audio. " + juce::String(error.what());
            }
            finished.store(true);
        });
        startTimerHz(15);
    }

    void timerCallback() override
    {
        displayProgress = progress.load();
        if (finished.load())
        {
            stopTimer();
            worker.join();
            open.setEnabled(true);
            for (auto& control : controls)
                control.setEnabled(true);
            exportButton.setButtonText("Export WAV");
            exportButton.setEnabled(true);
            progressBar.setVisible(false);
            status.setText(exportMessage, juce::dontSendNotification);
        }
    }

    juce::TextButton open { "Open audio" }, exportButton { "Export WAV" };
    juce::Label title, fileName, status;
    std::array<juce::Label, 4> labels;
    std::array<juce::Slider, 4> controls;
    double displayProgress = 0;
    juce::ProgressBar progressBar;
    juce::File input;
    std::unique_ptr<juce::FileChooser> chooser;
    std::atomic<bool> cancel { false }, finished { false };
    std::atomic<double> progress { 0 };
    juce::String exportMessage;
    std::thread worker;
};

class BlackieApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Blackie Vocal Box"; }
    const juce::String getApplicationVersion() override { return BVB_APP_VERSION; }

    void initialise(const juce::String&) override
    {
        window = std::make_unique<Window>();
    }

    void shutdown() override { window.reset(); }

private:
    class Window final : public juce::DocumentWindow
    {
    public:
        Window() : DocumentWindow("Blackie Vocal Box", juce::Colour(0xff20242b), closeButton)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<Window> window;
};
}

START_JUCE_APPLICATION(BlackieApplication)
