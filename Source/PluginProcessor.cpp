#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct FactoryPreset
{
    const char* name;
    float bend, tune, speed, vibe, smile, mix, input, output, harmo, filter, rpt;
    int div;
};

const FactoryPreset kPresets[] = {
    { "00 // INIT",            0.0f,  70.0f,  70.0f, 15.0f,  0.0f, 100.0f, 0.0f, 0.0f,  0.0f,   0.0f,  0.0f, 1 },
    { "01 // WIRED NATURAL",   0.0f,  45.0f,  40.0f, 15.0f, 10.0f, 100.0f, 0.0f, 0.0f, 10.0f,   0.0f,  0.0f, 1 },
    { "02 // SNAP",            0.0f, 100.0f, 100.0f,  0.0f,  0.0f, 100.0f, 0.0f, 0.0f,  0.0f,   0.0f,  0.0f, 1 },
    { "03 // LAYER 07 HALO",   0.0f,  70.0f,  75.0f, 20.0f, 65.0f, 100.0f, 0.0f, 0.0f, 35.0f,   0.0f,  0.0f, 1 },
    { "04 // DIMENSION",       0.0f,  60.0f,  60.0f, 15.0f, 20.0f, 100.0f, 0.0f, 0.0f, 70.0f,   0.0f,  0.0f, 1 },
    { "05 // GLITCH TERMINAL", 0.0f,  90.0f,  95.0f,  0.0f, 15.0f, 100.0f, 0.0f, 0.0f, 20.0f,  25.0f, 45.0f, 2 },
    { "06 // ANALOG GHOST",    0.0f,  30.0f,  30.0f, 25.0f, 40.0f, 100.0f, 0.0f, 0.0f, 40.0f, -35.0f,  0.0f, 1 },
    { "07 // BENDER",          0.0f,   0.0f,  70.0f, 10.0f, 30.0f, 100.0f, 0.0f, 0.0f,  0.0f,   0.0f,  0.0f, 1 },
};
constexpr int kNumPresets = (int) (sizeof(kPresets) / sizeof(kPresets[0]));

// Registro de voz (midi bajo, midi alto), con margen de unos semitonos.
const int kRangeLo[] = { 37, 57, 54, 50, 45, 40, 37 };
const int kRangeHi[] = { 83, 84, 80, 77, 72, 67, 63 };

float dbToGain(float db) { return std::pow(10.0f, db / 20.0f); }
float midiToHz(float m) { return 440.0f * std::exp2((m - 69.0f) / 12.0f); }
} // namespace

WIR3DProcessor::WIR3DProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "PARAMS", createParams())
{
    pBend   = params.getRawParameterValue("bend");
    pTune   = params.getRawParameterValue("tune");
    pSpeed  = params.getRawParameterValue("speed");
    pVibe   = params.getRawParameterValue("vibe");
    pSmile  = params.getRawParameterValue("smile");
    pMix    = params.getRawParameterValue("mix");
    pRef    = params.getRawParameterValue("ref");
    pRoot   = params.getRawParameterValue("root");
    pScale  = params.getRawParameterValue("scale");
    pInput  = params.getRawParameterValue("input");
    pOutput = params.getRawParameterValue("output");
    pHarmo  = params.getRawParameterValue("harmo");
    pFilter = params.getRawParameterValue("filter");
    pRpt    = params.getRawParameterValue("rptMix");
    pDiv    = params.getRawParameterValue("div");
    pRange  = params.getRawParameterValue("range");
}

juce::AudioProcessorValueTreeState::ParameterLayout WIR3DProcessor::createParams()
{
    using P = juce::AudioParameterFloat;
    using C = juce::AudioParameterChoice;
    using ID = juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<P>(ID { "input", 1 },  "INPUT",  juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<P>(ID { "bend", 1 },   "BEND",   juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<P>(ID { "tune", 1 },   "TUNE",   juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 70.0f));
    layout.add(std::make_unique<P>(ID { "speed", 1 },  "SPEED",  juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 70.0f));
    layout.add(std::make_unique<P>(ID { "vibe", 1 },   "VIBE",   juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 15.0f));
    layout.add(std::make_unique<P>(ID { "harmo", 1 },  "HARMO",  juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<P>(ID { "smile", 1 },  "SMILE",  juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<P>(ID { "filter", 1 }, "FILTER", juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<P>(ID { "rptMix", 1 }, "RPT MIX", juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<P>(ID { "output", 1 }, "OUTPUT", juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<P>(ID { "mix", 1 },    "MIX",    juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    layout.add(std::make_unique<P>(ID { "ref", 1 },    "A4",     juce::NormalisableRange<float>(430.0f, 450.0f, 0.1f), 440.0f));
    layout.add(std::make_unique<C>(ID { "root", 1 }, "KEY",
        juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add(std::make_unique<C>(ID { "scale", 1 }, "SCALE",
        juce::StringArray { "Chromatic", "Major", "Minor", "Major Pent", "Minor Pent", "Harm Minor",
                            "Blues", "Dorian", "Phrygian", "Mixolydian" }, 2));
    layout.add(std::make_unique<C>(ID { "range", 1 }, "RANGE",
        juce::StringArray { "Wide (all)", "Soprano", "Mezzo", "Contralto", "Tenor", "Baritone", "Bass" }, 0));
    layout.add(std::make_unique<C>(ID { "div", 1 }, "DIVISION",
        juce::StringArray { "1/4", "1/8", "1/16", "1/32", "Glitch" }, 1));
    return layout;
}

void WIR3DProcessor::setParamValue(const char* id, float value)
{
    if (auto* p = params.getParameter(id))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}

void WIR3DProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    sampleRate = sr;
    detector.prepare(sr);
    lastRange = -1;
    for (auto& s : mainShift) s.prepare(sr);
    for (auto& s : riseShift) s.prepare(sr);
    for (auto& s : harmoShift) s.prepare(sr);

    latencySamples = mainShift[0].latencySamples();
    setLatencySamples(latencySamples);

    drySize = latencySamples + 1;
    riseSize = (int) (sr * 0.8) + 8;
    haloDelay = juce::jlimit(1, riseSize / 4, (int) (sr * 0.16));
    for (int ch = 0; ch < 2; ++ch)
        dryBuf[(size_t) ch].assign((size_t) drySize, 0.0f);
    for (auto& d : riseDelay) d.assign((size_t) riseSize, 0.0f);
    dryPos = 0; risePos = 0;

    harmoDelayL.prepare((int) (sr * 0.7));
    harmoDelayR.prepare((int) (sr * 0.7));
    delaySm = (float) (sr * 0.375);

    for (int ch = 0; ch < 2; ++ch)
    {
        const size_t c = (size_t) ch;
        shelf[c] = wir3d::Biquad(); peak[c] = wir3d::Biquad();
        exHp1[c] = wir3d::Biquad(); exHp1[c].setHighpass(sr, 4000.0);
        exHp2[c] = wir3d::Biquad(); exHp2[c].setHighpass(sr, 4000.0);
        haloHp[c] = wir3d::Biquad(); haloHp[c].setHighpass(sr, 350.0);
        haloLp[c] = wir3d::Biquad(); haloLp[c].setLowpass(sr, 9000.0);
        tapeLp[c] = wir3d::Biquad(); tapeLp[c].setLowpass(sr, 3800.0);
        tapeHp[c] = wir3d::Biquad(); tapeHp[c].setHighpass(sr, 150.0);
        fLp[c] = wir3d::Biquad(); fHp[c] = wir3d::Biquad();
        loopBuf[c].assign((size_t) (sr * 2.6) + 8, 0.0f);
    }
    groupPos = 0;

    wetBuf.setSize(2, juce::jmax(samplesPerBlock, 1), false, false, true);
    wetBuf.clear();
    reverb.setSampleRate(sr);
    reverb.reset();

    bendSm = 0.0f; corrSm = 0.0f; targetCorr = 0.0f; noteValid = false;
    unvoicedSamples = 0; lockSamples = 0;
    smileSm = pSmile->load() * 0.01f;
    harmoSm = pHarmo->load() * 0.01f;
    filterSm = pFilter->load() * 0.01f;
    rptSm = pRpt->load() * 0.01f;
    mixSm = pMix->load() * 0.01f;
    inGainPrev = dbToGain(pInput->load());
    outGainPrev = dbToGain(pOutput->load());
    harmoPhase = wowPhase = flutterPhase = 0.0f;
    vibPhase = 0.0f; drift = 0.0f; driftTarget = 0.0f; driftCounter = 0;
    uiIn.store(-1.0f); uiOut.store(-1.0f); uiTarget.store(-1.0f); uiLock.store(false);
}

void WIR3DProcessor::releaseResources() {}

bool WIR3DProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet()
        && (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono()
            || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
}

void WIR3DProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int chans = juce::jmin(buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();
    if (chans < 1 || n < 1) return;
    if (wetBuf.getNumSamples() < n) wetBuf.setSize(2, n, false, false, true);

    const float sr = (float) sampleRate;
    const float bendTarget = pBend->load();
    const float tune  = pTune->load() * 0.01f;
    const float speed = pSpeed->load() * 0.01f;
    const float vibe  = pVibe->load() * 0.01f;
    const float smileTarget = pSmile->load() * 0.01f;
    const float harmoTarget = pHarmo->load() * 0.01f;
    const float filterTarget = pFilter->load() * 0.01f;
    const float rptTarget = pRpt->load() * 0.01f;
    const float mixTarget = pMix->load() * 0.01f;
    const float inGain = dbToGain(pInput->load()), outGain = dbToGain(pOutput->load());
    const float refOffset = 12.0f * std::log2(juce::jmax(pRef->load(), 400.0f) / 440.0f); // desvio de A4
    const int root  = (int) pRoot->load();
    const int scale = (int) pScale->load();
    const int div = juce::jlimit(0, 4, (int) pDiv->load());
    const int rangeIdx = juce::jlimit(0, 6, (int) pRange->load());
    if (rangeIdx != lastRange)
    {
        detector.setRange((double) midiToHz((float) kRangeLo[rangeIdx]), (double) midiToHz((float) kRangeHi[rangeIdx]));
        lastRange = rangeIdx;
    }

    // Tempo del host (si no hay, 120 bpm)
    double bpm = 120.0, ppq = -1.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    bpm = juce::jlimit(30.0, 300.0, bpm);
    const double beatLen = 60.0 / bpm * sampleRate;
    const float delayTarget = (float) juce::jlimit(0.12, 0.5, 60.0 / bpm * 0.75) * sr; // corchea con puntillo

    // BEAT REPEAT: tamano de la rebanada y del grupo (la primera rebanada se graba en vivo y se repite)
    int sliceLen, groupLen;
    switch (div)
    {
        case 0:  sliceLen = (int) beatLen;          groupLen = 2 * sliceLen; break;
        case 1:  sliceLen = (int) (beatLen * 0.5);  groupLen = 2 * sliceLen; break;
        case 2:  sliceLen = (int) (beatLen * 0.25); groupLen = 4 * sliceLen; break;
        case 3:  sliceLen = (int) (beatLen * 0.125); groupLen = 8 * sliceLen; break;
        default: sliceLen = (int) (0.025 * sampleRate); groupLen = 8 * sliceLen; break;
    }
    sliceLen = juce::jlimit(64, (int) loopBuf[0].size() - 1, sliceLen);
    groupLen = juce::jmax(groupLen, 2 * sliceLen);
    if (ppq >= 0.0 && div < 4)
    {
        const double groupBeats = (double) groupLen / beatLen;
        groupPos = (int) (std::fmod(ppq, groupBeats) / groupBeats * (double) groupLen);
    }
    if (groupPos >= groupLen || groupPos < 0) groupPos = 0;
    const int xfade = juce::jmax(8, (int) (0.002 * sampleRate));

    // SPEED: tiempo de retune de ~300 ms (suave) a ~4 ms (efecto duro)
    const float tau = 0.3f * std::pow(0.004f / 0.3f, speed);
    const float corrCoef  = 1.0f - std::exp(-1.0f / (tau * sr));
    const float releaseCoef = 1.0f - std::exp(-1.0f / (0.08f * sr)); // al acabar la nota
    const int holdSamples = (int) (0.05f * sr);                      // mantiene la correccion en consonantes
    const float bendCoef  = 1.0f - std::exp(-1.0f / (0.012f * sr));
    const float smileCoef = 1.0f - std::exp(-1.0f / (0.030f * sr));
    const float driftCoef = 1.0f - std::exp(-1.0f / (0.12f * sr));
    const float vibInc = 5.3f / sr;
    const float twoPi = 2.0f * wir3d::kPi;
    static const float kFifth = 1.4983071f; // +7 semitonos

    float peakIn = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float gIn = inGainPrev + (inGain - inGainPrev) * (float) (i + 1) / (float) n;
        const float l = buffer.getSample(0, i) * gIn;
        const float r = (chans > 1 ? buffer.getSample(1, i) : buffer.getSample(0, i)) * gIn;
        const float monoIn = chans > 1 ? 0.5f * (l + r) : l;
        peakIn = juce::jmax(peakIn, std::abs(l), std::abs(r));

        // --- deteccion de tono + objetivo de correccion ---
        if (detector.push(monoIn))
        {
            if (detector.isVoiced())
            {
                const float det = detector.midiNote() - refOffset;
                lastDet = det;
                // Histeresis de nota: el vibrato natural no debe saltar entre notas vecinas.
                float target = wir3d::quantiseMidi(det, root, scale);
                if (noteValid && target != currentNote
                    && wir3d::quantiseMidi(currentNote, root, scale) == currentNote
                    && std::abs(det - target) + 0.30f >= std::abs(det - currentNote))
                    target = currentNote;
                currentNote = target;
                noteValid = true;
                float err = target - det;
                if (std::abs(err) < 0.03f) err = 0.0f; // banda muerta de ~3 cents
                targetCorr = err * tune;
                const float period = sr / juce::jmax(midiToHz(detector.midiNote()), 1.0f);
                for (auto& s : mainShift) s.setPeriod(period);
                for (auto& s : riseShift) s.setPeriod(period);
                for (auto& s : harmoShift) s.setPeriod(period);
            }
            else
            {
                for (auto& s : mainShift) s.setPeriod(0.0f);
                for (auto& s : riseShift) s.setPeriod(0.0f);
                for (auto& s : harmoShift) s.setPeriod(0.0f);
            }
        }

        // Sin tono (consonantes, respiraciones, fin de nota): se mantiene la
        // correccion un instante y despues se libera suavemente.
        if (detector.isVoiced()) unvoicedSamples = 0; else ++unvoicedSamples;
        const bool released = unvoicedSamples > holdSamples;
        if (released) { targetCorr = 0.0f; noteValid = false; }

        // --- suavizados ---
        bendSm  += (bendTarget - bendSm) * bendCoef;
        corrSm  += (targetCorr - corrSm) * (released ? releaseCoef : corrCoef);
        smileSm += (smileTarget - smileSm) * smileCoef;
        harmoSm += (harmoTarget - harmoSm) * smileCoef;

        // LOCK: la nota (sin bend ni vibrato) lleva >0.25 s dentro de +-10 cents del objetivo
        if (noteValid && !released && std::abs(lastDet + corrSm - currentNote) < 0.10f) ++lockSamples;
        else lockSamples = 0;

        // --- VIBE: vibrato real (modulacion de afinacion) + deriva lenta ---
        vibPhase += vibInc;
        if (vibPhase >= 1.0f) vibPhase -= 1.0f;
        if (--driftCounter <= 0) { driftCounter = (int) (0.05f * sr); driftTarget = rng.nextFloat() * 2.0f - 1.0f; }
        drift += (driftTarget - drift) * driftCoef;
        const float vibSemis = vibe * (0.30f * std::sin(twoPi * vibPhase) + 0.20f * drift);

        const float semis = juce::jlimit(-24.0f, 24.0f, bendSm + corrSm + vibSemis);
        lastSemis = semis;
        const float ratio = std::exp2(semis / 12.0f);

        // --- SMILE por etapas ---
        //   0-50 %  : voz mas abierta y brillante (shelf de aire + presencia + excitador armonico)
        //   12-100 %: halo de voces ascendentes (quinta, octava, octava+quinta) con eco
        //   35-100 %: floracion de reverb
        const float smile = smileSm;
        const float bright = juce::jlimit(0.0f, 1.0f, smile / 0.5f);
        if ((i & 31) == 0)
        {
            wir3d::Biquad tmp;
            tmp.setHighShelf(sampleRate, 7000.0, 0.7071, 5.0 * bright);
            for (auto& b : shelf) b.copyCoeffsFrom(tmp);
            tmp.setPeak(sampleRate, 3200.0, 0.9, 2.5 * bright);
            for (auto& b : peak) b.copyCoeffsFrom(tmp);
        }
        const float exGain = 0.55f * bright;
        const float drive = 1.0f + smile * 1.5f;
        const float satGain = 1.0f / std::pow(drive, 0.5f);
        const float satAmt = juce::jmin(1.0f, smile * 2.0f);
        const float makeup = 1.0f / (1.0f + 0.30f * smile);

        static const float riseRatio[3] = { 1.4983071f, 2.0f, 2.9966142f }; // +7, +12, +19 semitonos
        static const float riseBase[3]  = { 0.55f, 0.42f, 0.32f };
        float hv[3];
        for (int k = 0; k < 3; ++k)
        {
            const float v = riseShift[(size_t) k].process(monoIn, juce::jmin(4.0f, ratio * riseRatio[k]));
            riseDelay[(size_t) k][(size_t) risePos] = v;
            int idx = risePos - (k + 1) * haloDelay;
            if (idx < 0) idx += riseSize;
            const float gk = riseBase[k] * wir3d::smoothstep01((smile - 0.12f - 0.22f * (float) k) / 0.30f);
            hv[k] = riseDelay[(size_t) k][(size_t) idx] * gk;
        }
        if (++risePos >= riseSize) risePos = 0;
        float haloL = hv[0] + 0.35f * hv[1] + 0.70f * hv[2];
        float haloR = 0.35f * hv[0] + hv[1] + 0.70f * hv[2];
        haloL = haloLp[0].process(haloHp[0].process(haloL));
        haloR = haloLp[1].process(haloHp[1].process(haloR));
        if (chans == 1) { haloL = 0.5f * (haloL + haloR); haloR = haloL; }

        // --- HARMO: quinta estereo (-8 / +8 cents + LFO de 0.42 Hz en cuadratura) y ping-pong de cinta ---
        harmoPhase += 0.42f / sr; if (harmoPhase >= 1.0f) harmoPhase -= 1.0f;
        wowPhase += 0.7f / sr;    if (wowPhase >= 1.0f) wowPhase -= 1.0f;
        flutterPhase += 6.1f / sr; if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
        const float detL = std::exp2((-8.0f + 5.0f * std::sin(twoPi * harmoPhase)) / 1200.0f);
        const float detR = std::exp2(( 8.0f + 5.0f * std::cos(twoPi * harmoPhase)) / 1200.0f);
        const float hl = harmoShift[0].process(monoIn, juce::jmin(4.0f, ratio * kFifth * detL));
        const float hr = harmoShift[1].process(monoIn, juce::jmin(4.0f, ratio * kFifth * detR));
        delaySm += (delayTarget - delaySm) * 0.0002f;
        const float wow = 0.0005f * sr * std::sin(twoPi * wowPhase) + 0.00012f * sr * std::sin(twoPi * flutterPhase);
        const float maxD = (float) harmoDelayL.capacity() - 4.0f;
        const float eL = harmoDelayL.read(juce::jlimit(4.0f, maxD, delaySm + wow));
        const float eR = harmoDelayR.read(juce::jlimit(4.0f, maxD, delaySm - wow));
        const float fb = 0.38f;
        harmoDelayL.write(std::tanh(tapeLp[0].process(tapeHp[0].process(hl + eR * fb))));
        harmoDelayR.write(std::tanh(tapeLp[1].process(tapeHp[1].process(hr + eL * fb))));
        float harmL = harmoSm * (0.45f * hl + 0.60f * eL);
        float harmR = harmoSm * (0.45f * hr + 0.60f * eR);
        if (chans == 1) { harmL = 0.5f * (harmL + harmR); harmR = harmL; }

        dryPos = (dryPos + 1) % drySize;
        for (int ch = 0; ch < chans; ++ch)
        {
            const float in = ch == 0 ? l : r;
            dryBuf[(size_t) ch][(size_t) dryPos] = in;

            const float body = mainShift[(size_t) ch].process(in, ratio);

            // brillo: aire + presencia, mas armonicos generados en las altas frecuencias
            float v = peak[(size_t) ch].process(shelf[(size_t) ch].process(body));
            const float hf = exHp1[(size_t) ch].process(body);
            v += exHp2[(size_t) ch].process(std::tanh(4.0f * hf) * 0.25f) * exGain;

            // saturacion suave del cuerpo
            v += (std::tanh(v * drive) * satGain - v) * satAmt;

            v = (v + (ch == 0 ? haloL + harmL : haloR + harmR)) * makeup;
            wetBuf.setSample(ch, i, v);

            // la senal seca se retrasa para alinearla con la latencia del efecto
            buffer.setSample(ch, i, dryBuf[(size_t) ch][(size_t) ((dryPos + 1) % drySize)]);
        }
    }
    inGainPrev = inGain;

    // --- datos para el monitor de la interfaz ---
    const bool tracking = detector.isVoiced() && unvoicedSamples <= holdSamples;
    uiIn.store(tracking ? lastDet : -1.0f);
    uiOut.store(tracking ? lastDet + lastSemis : -1.0f);
    uiTarget.store(tracking && noteValid ? currentNote : -1.0f);
    uiLock.store(lockSamples > (int) (0.25f * sr));
    uiLevelIn.store(peakIn);

    // --- reverb de SMILE ---
    const float bloom = juce::jlimit(0.0f, 1.0f, (smileTarget - 0.35f) / 0.65f);
    juce::Reverb::Parameters rp;
    rp.roomSize = 0.60f + 0.30f * bloom;
    rp.damping = 0.50f;
    rp.wetLevel = bloom * 0.24f;
    rp.dryLevel = 0.5f;
    rp.width = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters(rp);
    if (chans > 1) reverb.processStereo(wetBuf.getWritePointer(0), wetBuf.getWritePointer(1), n);
    else           reverb.processMono(wetBuf.getWritePointer(0), n);

    // --- etapa final: mezcla, beat repeat, filtro maestro, trim de salida y techo suave ---
    const float m0 = mixSm;
    mixSm += (mixTarget - mixSm) * (1.0f - std::exp(-(float) n / (0.03f * sr)));
    float peakOut = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float m = m0 + (mixSm - m0) * (float) (i + 1) / (float) n;
        float o[2] = { 0.0f, 0.0f };
        for (int ch = 0; ch < chans; ++ch)
            o[ch] = buffer.getSample(ch, i) * (1.0f - m) + wetBuf.getSample(ch, i) * m;

        // beat repeat: la primera rebanada de cada grupo se graba en vivo y se repite el resto del grupo
        rptSm += (rptTarget - rptSm) * smileCoef;
        if (groupPos < sliceLen)
        {
            for (int ch = 0; ch < chans; ++ch) loopBuf[(size_t) ch][(size_t) groupPos] = o[ch];
        }
        else if (rptSm > 0.0005f)
        {
            const int j = (groupPos - sliceLen) % sliceLen;
            const float env = juce::jlimit(0.0f, 1.0f, juce::jmin((float) j, (float) (sliceLen - 1 - j)) / (float) xfade);
            const float x = rptSm * env;
            for (int ch = 0; ch < chans; ++ch)
                o[ch] = o[ch] * (1.0f - x) + loopBuf[(size_t) ch][(size_t) j] * x;
        }
        if (++groupPos >= groupLen) groupPos = 0;

        // filtro maestro bipolar: izquierda = paso bajo (20 kHz -> 180 Hz), derecha = paso alto (20 Hz -> 3.8 kHz)
        filterSm += (filterTarget - filterSm) * smileCoef;
        if ((i & 31) == 0)
        {
            wir3d::Biquad lp, hp; // identidad por defecto
            const float f = filterSm;
            if (f < -0.03f)      lp.setLowpass(sampleRate, 20000.0 * std::pow(180.0 / 20000.0, std::pow((double) -f, 0.8)), 0.8);
            else if (f > 0.03f)  hp.setHighpass(sampleRate, 20.0 * std::pow(3800.0 / 20.0, std::pow((double) f, 0.8)), 0.8);
            for (auto& b : fLp) b.copyCoeffsFrom(lp);
            for (auto& b : fHp) b.copyCoeffsFrom(hp);
        }

        const float gOut = outGainPrev + (outGain - outGainPrev) * (float) (i + 1) / (float) n;
        for (int ch = 0; ch < chans; ++ch)
        {
            float y = fHp[(size_t) ch].process(fLp[(size_t) ch].process(o[ch]));
            y = wir3d::softCeiling(y * gOut);
            buffer.setSample(ch, i, y);
            peakOut = juce::jmax(peakOut, std::abs(y));
        }
    }
    outGainPrev = outGain;
    uiLevelOut.store(peakOut);
}

juce::AudioProcessorEditor* WIR3DProcessor::createEditor() { return new WIR3DEditor(*this); }
bool WIR3DProcessor::hasEditor() const { return true; }
const juce::String WIR3DProcessor::getName() const { return JucePlugin_Name; }
bool WIR3DProcessor::acceptsMidi() const { return false; }
bool WIR3DProcessor::producesMidi() const { return false; }
bool WIR3DProcessor::isMidiEffect() const { return false; }
double WIR3DProcessor::getTailLengthSeconds() const { return 5.0; }

int WIR3DProcessor::getNumPrograms() { return kNumPresets; }
int WIR3DProcessor::getCurrentProgram() { return currentProgram; }
const juce::String WIR3DProcessor::getProgramName(int index)
{
    return juce::isPositiveAndBelow(index, kNumPresets) ? juce::String(kPresets[index].name) : juce::String();
}
void WIR3DProcessor::changeProgramName(int, const juce::String&) {}

void WIR3DProcessor::setCurrentProgram(int index)
{
    if (! juce::isPositiveAndBelow(index, kNumPresets)) return;
    currentProgram = index;
    const auto& p = kPresets[index];
    setParamValue("bend", p.bend);   setParamValue("tune", p.tune);   setParamValue("speed", p.speed);
    setParamValue("vibe", p.vibe);   setParamValue("smile", p.smile); setParamValue("mix", p.mix);
    setParamValue("input", p.input); setParamValue("output", p.output);
    setParamValue("harmo", p.harmo); setParamValue("filter", p.filter);
    setParamValue("rptMix", p.rpt);  setParamValue("div", (float) p.div);
}

void WIR3DProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto state = params.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, dest);
}

void WIR3DProcessor::setStateInformation(const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, size));
    if (xml != nullptr && xml->hasTagName(params.state.getType()))
        params.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WIR3DProcessor(); }
