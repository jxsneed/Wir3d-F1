#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "Dsp.h"

class WIR3DProcessor : public juce::AudioProcessor
{
public:
    WIR3DProcessor();
    ~WIR3DProcessor() override = default;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override;
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState params;

    // Lecturas para la interfaz (midi detectado / midi de salida / nota objetivo; -1 = sin tono)
    std::atomic<float> uiIn { -1.0f }, uiOut { -1.0f }, uiTarget { -1.0f };
    std::atomic<float> uiLevelIn { 0.0f }, uiLevelOut { 0.0f };
    std::atomic<bool> uiLock { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();
    void setParamValue(const char* id, float value);

    double sampleRate = 44100.0;
    int latencySamples = 0, currentProgram = 0, lastRange = -1;

    wir3d::PitchDetector detector;
    std::array<wir3d::PitchShifter, 2> mainShift;
    std::array<std::vector<float>, 2> dryBuf;
    int dryPos = 0, drySize = 1;

    // SMILE: halo de voces ascendentes (quinta, octava, octava+quinta) con eco
    std::array<wir3d::PitchShifter, 3> riseShift;
    std::array<std::vector<float>, 3> riseDelay;
    int risePos = 0, riseSize = 1, haloDelay = 1;
    std::array<wir3d::Biquad, 2> shelf, peak, exHp1, exHp2, haloHp, haloLp;

    // HARMO: armonizador "dimension" (+7 st, +-8 cents) con delay ping-pong de cinta
    std::array<wir3d::PitchShifter, 2> harmoShift;
    wir3d::DelayLine harmoDelayL, harmoDelayR;
    std::array<wir3d::Biquad, 2> tapeLp, tapeHp;
    float harmoSm = 0.0f, harmoPhase = 0.0f, wowPhase = 0.0f, flutterPhase = 0.0f, delaySm = 1.0f;

    // BEAT REPEAT
    std::array<std::vector<float>, 2> loopBuf;
    int groupPos = 0;
    float rptSm = 0.0f;

    // Filtro maestro bipolar
    std::array<wir3d::Biquad, 2> fLp, fHp;
    float filterSm = 0.0f;

    juce::AudioBuffer<float> wetBuf;
    juce::Reverb reverb;

    float bendSm = 0.0f, corrSm = 0.0f, smileSm = 0.0f, mixSm = 1.0f, targetCorr = 0.0f;
    float currentNote = 0.0f;
    bool noteValid = false;
    int unvoicedSamples = 0, lockSamples = 0;
    float lastDet = 60.0f, lastSemis = 0.0f, inGainPrev = 1.0f, outGainPrev = 1.0f;
    float vibPhase = 0.0f, drift = 0.0f, driftTarget = 0.0f;
    int driftCounter = 0;
    juce::Random rng;

    std::atomic<float>* pBend = nullptr;
    std::atomic<float>* pTune = nullptr;
    std::atomic<float>* pSpeed = nullptr;
    std::atomic<float>* pVibe = nullptr;
    std::atomic<float>* pSmile = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pRef = nullptr;
    std::atomic<float>* pRoot = nullptr;
    std::atomic<float>* pScale = nullptr;
    std::atomic<float>* pInput = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pHarmo = nullptr;
    std::atomic<float>* pFilter = nullptr;
    std::atomic<float>* pRpt = nullptr;
    std::atomic<float>* pDiv = nullptr;
    std::atomic<float>* pRange = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WIR3DProcessor)
};
