#pragma once
#include <JuceHeader.h>
#include <array>

class WIR3DProcessor;

// Interfaz "Navi": monitor CRT beige, ventanas de SO retro, teclado funcional y cables.
class WIR3DEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit WIR3DEditor(WIR3DProcessor&);
    ~WIR3DEditor() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void drawRoom(juce::Graphics&);
    void drawMonitorCase(juce::Graphics&);
    void drawMenuBar(juce::Graphics&);
    void drawTrace(juce::Graphics&, juce::Rectangle<float> area);
    void drawMeters(juce::Graphics&, juce::Rectangle<float> area);
    void drawTuning(juce::Graphics&, juce::Rectangle<float> win);
    void drawEye(juce::Graphics&, juce::Point<float> centre);
    void drawTicker(juce::Graphics&, juce::Rectangle<float> area);
    void drawKeyboard(juce::Graphics&);

    juce::Rectangle<float> keyRect(int index) const;
    int keyAt(juce::Point<float> p) const;

    void refreshPresets();
    void onPresetChosen();
    void saveUserPreset();

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    static constexpr int kKnobs = 11;
    static constexpr int kTrace = 150; // ~5 s a 30 fps

    WIR3DProcessor& proc;
    std::array<juce::Slider, kKnobs> knobs;
    std::array<std::unique_ptr<SliderAttachment>, kKnobs> knobAtt;
    juce::Slider refA4;
    juce::ComboBox scaleBox, rangeBox, divBox, presetBox;
    juce::TextButton saveBtn { "SAVE" };
    std::unique_ptr<SliderAttachment> refAtt;
    std::unique_ptr<ComboAttachment> scaleAtt, rangeAtt, divAtt;
    std::unique_ptr<juce::LookAndFeel_V4> look;
    juce::Array<juce::File> userFiles;

    std::array<float, kTrace> traceIn, traceOut;
    int tracePos = 0, hoverKey = -1;
    float centreMidi = 60.0f, eyeOpen = 0.25f, pupil = 3.5f, t = 0.0f;
    float tickerX = 0.0f, tickerW = 0.0f;
    float lastIn = -1.0f, lastOut = -1.0f, lastTarget = -1.0f;
    float meterIn = 0.0f, meterOut = 0.0f;
    bool lastLock = false;
    int glitchFrames = 0, nextGlitch = 70;

    juce::Image noiseImg, scanImg;
    juce::Random rng { 1234 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WIR3DEditor)
};
