#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
// ---- Paleta: plastico gris calido, tubo casi negro, texto blanco sucio, rojo de alarma, cian de datos ----
const juce::Colour kPlasticHi (0xffd2cdbd);
const juce::Colour kPlasticMid(0xffbab5a4);
const juce::Colour kPlasticLo (0xff948f7f);
const juce::Colour kPlasticSh (0xff5d5a4f);
const juce::Colour kScreenBg  (0xff04080a);
const juce::Colour kBody      (0xff0b1113);
const juce::Colour kBar       (0xffc6c6c1);
const juce::Colour kBarDark   (0xff8d8d88);
const juce::Colour kEdge      (0xffb9b9b4);
const juce::Colour kText      (0xffe6e5dc);
const juce::Colour kDim       (0xff878985);
const juce::Colour kLine      (0xff30393b);
const juce::Colour kRed       (0xffe0352e);
const juce::Colour kCyan      (0xff78d2d0);

constexpr int kW = 1000, kH = 640;
const juce::Rectangle<float> kScreen(58.0f, 48.0f, 884.0f, 442.0f);
const juce::Rectangle<float> kWinTrace(68.0f, 82.0f, 566.0f, 176.0f);
const juce::Rectangle<float> kWinTune (642.0f, 82.0f, 290.0f, 176.0f);
const juce::Rectangle<float> kWinParams(68.0f, 266.0f, 864.0f, 196.0f);

const char* const kIds[]    = { "input", "bend", "tune", "speed", "vibe", "harmo", "smile", "filter", "rptMix", "output", "mix" };
const char* const kLabels[] = { "INPUT", "BEND", "TUNE", "SPEED", "VIBE", "HARMO", "SMILE", "FILTER", "RPT MIX", "OUTPUT", "MIX" };
const char* const kSuffix[] = { " dB", " st", " %", " %", " %", " %", " %", "", " %", " dB", " %" };
constexpr int kCellX = 76, kCellStep = 77, kKnobY = 298, kKnobH = 88;
const char* const kKeyNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

juce::Font mono(float size, float kerning = 0.0f, bool bold = false)
{
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), size,
                                        bold ? juce::Font::bold : juce::Font::plain)
                          .withKerningFactor(kerning));
}

// Textos japoneses en UTF-8 explicito (no dependen de la codificacion del archivo).
juce::String jpLink()   { return juce::String::fromUTF8("\xe6\x8e\xa5" "\xe7\xb6\x9a"); }
juce::String jpSignal() { return juce::String::fromUTF8("\xe4\xbf\xa1" "\xe5\x8f\xb7"); }
juce::String jpWired()  { return juce::String::fromUTF8("\xe3\x83\xaf" "\xe3\x82\xa4" "\xe3\x83\xa4" "\xe3\x83\xbc" "\xe3\x83\x89"); }

juce::String noteName(int midi)
{
    static const char* const names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int n = ((midi % 12) + 12) % 12;
    return juce::String(names[n]) + juce::String(midi / 12 - 1);
}

juce::String noteLabel(float midi)
{
    if (midi < 0.0f) return "--";
    const int n = (int) std::lround(midi);
    const int cents = (int) std::lround((midi - (float) n) * 100.0f);
    return noteName(n) + " " + juce::String(cents >= 0 ? "+" : "-")
           + juce::String(std::abs(cents)).paddedLeft('0', 2) + "c";
}

juce::File presetDir()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("wir3d").getChildFile("Presets");
}

// Ventana estilo SO retro: barra de titulo gris con rayas, caja de cierre y cuerpo oscuro.
void drawWindow(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    g.setColour(kBody);
    g.fillRect(r);
    g.setColour(kEdge);
    g.drawRect(r, 1.0f);
    const auto bar = juce::Rectangle<float>(r.getX(), r.getY(), r.getWidth(), 16.0f);
    g.setColour(kBar);
    g.fillRect(bar);
    g.setColour(kBarDark);
    for (float y = bar.getY() + 3.0f; y < bar.getBottom() - 2.0f; y += 3.0f)
        g.drawLine(bar.getX() + 22.0f, y, bar.getRight() - 4.0f, y, 1.0f);
    const auto box = juce::Rectangle<float>(bar.getX() + 5.0f, bar.getY() + 3.0f, 10.0f, 10.0f);
    g.setColour(kBar);
    g.fillRect(box);
    g.setColour(kBarDark);
    g.drawRect(box, 1.0f);
    g.setFont(mono(10.0f, 0.1f, true));
    const float tw = 7.0f * (float) title.length() + 14.0f;
    const auto plate = juce::Rectangle<float>(bar.getCentreX() - tw * 0.5f, bar.getY() + 1.0f, tw, 14.0f);
    g.setColour(kBar);
    g.fillRect(plate);
    g.setColour(juce::Colour(0xff101010));
    g.drawText(title, plate.toNearestInt(), juce::Justification::centred, false);
}

void cable(juce::Graphics& g, float x0, float y0, float c0x, float c0y, float c1x, float c1y,
           float x1, float y1, juce::Colour col, float thick)
{
    juce::Path p;
    p.startNewSubPath(x0, y0);
    p.cubicTo(c0x, c0y, c1x, c1y, x1, y1);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.strokePath(p, juce::PathStrokeType(thick + 2.0f));
    g.setColour(col);
    g.strokePath(p, juce::PathStrokeType(thick));
}

// ---------------------------------------------------------------------------
class NaviLook : public juce::LookAndFeel_V4
{
public:
    NaviLook()
    {
        setColour(juce::PopupMenu::backgroundColourId, kBar);
        setColour(juce::PopupMenu::textColourId, juce::Colour(0xff101010));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kRed);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::Label::textColourId, kText);
    }

    juce::Font getLabelFont(juce::Label&) override { return mono(11.0f); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return mono(11.0f); }
    juce::Font getPopupMenuFont() override { return mono(11.0f); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return mono(11.0f, 0.1f, true); }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                          float pos, float start, float end, juce::Slider&) override
    {
        auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h).reduced(4.0f);
        const auto c = b.getCentre();
        const float r = juce::jmin(b.getWidth(), b.getHeight()) * 0.5f;
        const float angle = start + pos * (end - start);

        // marcas del dial (las activas en cian)
        const int ticks = 21;
        for (int i = 0; i < ticks; ++i)
        {
            const float f = (float) i / (float) (ticks - 1);
            const float a = start + f * (end - start) - juce::MathConstants<float>::halfPi; // 0 = arriba
            const bool major = (i % 5 == 0);
            const float r1 = r * (major ? 0.82f : 0.88f);
            g.setColour(f <= pos + 0.001f ? kCyan : kLine.brighter(0.4f));
            g.drawLine(c.x + std::cos(a) * r1, c.y + std::sin(a) * r1,
                       c.x + std::cos(a) * r, c.y + std::sin(a) * r, major ? 1.5f : 1.0f);
        }

        // cuerpo del boton: plastico oscuro con aro claro
        const float rb = r * 0.70f;
        juce::ColourGradient body(juce::Colour(0xff2a3032), c.x, c.y - rb, juce::Colour(0xff0d1112), c.x, c.y + rb, false);
        g.setGradientFill(body);
        g.fillEllipse(c.x - rb, c.y - rb, rb * 2.0f, rb * 2.0f);
        g.setColour(kBar);
        g.drawEllipse(c.x - rb, c.y - rb, rb * 2.0f, rb * 2.0f, 1.2f);

        // indicador rojo
        const float ang = angle - juce::MathConstants<float>::halfPi;
        g.setColour(kRed);
        g.drawLine(c.x + std::cos(ang) * rb * 0.25f, c.y + std::sin(ang) * rb * 0.25f,
                   c.x + std::cos(ang) * rb * 0.92f, c.y + std::sin(ang) * rb * 0.92f, 2.2f);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                          float, float, juce::Slider::SliderStyle, juce::Slider&) override
    {
        const float cy = (float) y + (float) h * 0.5f;
        g.setColour(kLine.brighter(0.5f));
        g.drawLine((float) x, cy, (float) (x + w), cy, 1.0f);
        for (int i = 0; i <= 10; ++i)
        {
            const float tx = (float) x + (float) w * (float) i / 10.0f;
            g.drawLine(tx, cy - 3.0f, tx, cy + 3.0f, 1.0f);
        }
        g.setColour(kCyan);
        g.drawLine((float) x, cy, sliderPos, cy, 1.6f);
        const auto thumb = juce::Rectangle<float>(sliderPos - 4.0f, cy - 8.0f, 8.0f, 16.0f);
        g.setColour(kBar);
        g.fillRect(thumb);
        g.setColour(kRed);
        g.drawLine(sliderPos, cy - 6.0f, sliderPos, cy + 6.0f, 1.5f);
    }

    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        const auto r = juce::Rectangle<float>(0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f);
        g.setColour(kBar);
        g.fillRect(r);
        g.setColour(juce::Colours::white.withAlpha(0.7f));          // bisel claro
        g.drawLine(0.5f, 0.5f, (float) w - 1.0f, 0.5f, 1.0f);
        g.drawLine(0.5f, 0.5f, 0.5f, (float) h - 1.0f, 1.0f);
        g.setColour(kBarDark);                                       // bisel oscuro
        g.drawLine(0.5f, (float) h - 0.5f, (float) w - 0.5f, (float) h - 0.5f, 1.0f);
        g.drawLine((float) w - 0.5f, 0.5f, (float) w - 0.5f, (float) h - 0.5f, 1.0f);
        juce::Path tri;
        const float cx = (float) w - 11.0f, cy = (float) h * 0.5f;
        tri.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
        g.setColour(juce::Colour(0xff101010));
        g.fillPath(tri);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&,
                              bool highlighted, bool down) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(down ? kRed : (highlighted ? kBar.brighter(0.2f) : kBar));
        g.fillRect(r);
        g.setColour(kBarDark);
        g.drawRect(r, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        g.setFont(mono(11.0f, 0.1f, true));
        g.setColour(down ? juce::Colours::white : juce::Colour(0xff101010));
        g.drawText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
    }
};
} // namespace

// ===========================================================================
WIR3DEditor::WIR3DEditor(WIR3DProcessor& p) : AudioProcessorEditor(&p), proc(p)
{
    traceIn.fill(-1.0f);
    traceOut.fill(-1.0f);

    look = std::make_unique<NaviLook>();
    setLookAndFeel(look.get());
    setSize(kW, kH);

    // ruido de grano y scanlines pre-renderizados (se repiten en mosaico)
    noiseImg = juce::Image(juce::Image::ARGB, 128, 128, true);
    for (int yy = 0; yy < 128; ++yy)
        for (int xx = 0; xx < 128; ++xx)
            noiseImg.setPixelAt(xx, yy, juce::Colour::fromFloatRGBA(1.0f, 1.0f, 1.0f, rng.nextFloat() * 0.07f));
    scanImg = juce::Image(juce::Image::ARGB, 4, 3, true);
    for (int xx = 0; xx < 4; ++xx)
        scanImg.setPixelAt(xx, 0, juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.30f));

    for (int i = 0; i < kKnobs; ++i)
    {
        auto& s = knobs[(size_t) i];
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                              juce::MathConstants<float>::pi * 2.75f, true);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 74, 18);
        s.setColour(juce::Slider::textBoxTextColourId, kText);
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour(juce::Slider::textBoxHighlightColourId, kRed);
        s.setTextValueSuffix(kSuffix[i]);
        addAndMakeVisible(s);
        knobAtt[(size_t) i].reset(new SliderAttachment(proc.params, kIds[i], s));
    }

    refA4.setSliderStyle(juce::Slider::LinearHorizontal);
    refA4.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 20);
    refA4.setTextValueSuffix(" Hz");
    refA4.setColour(juce::Slider::textBoxTextColourId, kText);
    refA4.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    refA4.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(refA4);
    refAtt.reset(new SliderAttachment(proc.params, "ref", refA4));

    // Los items salen de los propios parametros: nunca se desincronizan.
    if (auto* s = dynamic_cast<juce::AudioParameterChoice*>(proc.params.getParameter("scale")))
        scaleBox.addItemList(s->choices, 1);
    if (auto* r = dynamic_cast<juce::AudioParameterChoice*>(proc.params.getParameter("range")))
        rangeBox.addItemList(r->choices, 1);
    if (auto* d = dynamic_cast<juce::AudioParameterChoice*>(proc.params.getParameter("div")))
        divBox.addItemList(d->choices, 1);
    for (auto* c : { &scaleBox, &rangeBox, &divBox, &presetBox })
    {
        addAndMakeVisible(*c);
        c->setColour(juce::ComboBox::textColourId, juce::Colour(0xff101010));
        c->setColour(juce::ComboBox::backgroundColourId, kBar);
        c->setColour(juce::ComboBox::outlineColourId, kBarDark);
        c->setJustificationType(juce::Justification::centredLeft);
    }
    scaleAtt.reset(new ComboAttachment(proc.params, "scale", scaleBox));
    rangeAtt.reset(new ComboAttachment(proc.params, "range", rangeBox));
    divAtt.reset(new ComboAttachment(proc.params, "div", divBox));

    addAndMakeVisible(saveBtn);
    saveBtn.onClick = [this] { saveUserPreset(); };
    presetBox.onChange = [this] { onPresetChosen(); };
    refreshPresets();

    startTimerHz(30);
}

WIR3DEditor::~WIR3DEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

// ---------------------------------------------------------------------------
void WIR3DEditor::refreshPresets()
{
    presetBox.clear(juce::dontSendNotification);
    for (int i = 0; i < proc.getNumPrograms(); ++i)
        presetBox.addItem(proc.getProgramName(i), i + 1);
    userFiles.clear();
    const auto dir = presetDir();
    if (dir.isDirectory())
    {
        userFiles = dir.findChildFiles(juce::File::findFiles, false, "*.wir3dpreset");
        if (userFiles.size() > 0) presetBox.addSeparator();
        for (int i = 0; i < userFiles.size(); ++i)
            presetBox.addItem("USER // " + userFiles[i].getFileNameWithoutExtension(), 101 + i);
    }
    presetBox.setSelectedId(proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void WIR3DEditor::onPresetChosen()
{
    const int id = presetBox.getSelectedId();
    if (id >= 1 && id <= proc.getNumPrograms())
        proc.setCurrentProgram(id - 1);
    else if (id >= 101 && id - 101 < userFiles.size())
    {
        if (auto xml = juce::parseXML(userFiles[id - 101]))
            if (xml->hasTagName(proc.params.state.getType()))
                proc.params.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

void WIR3DEditor::saveUserPreset()
{
    const auto dir = presetDir();
    dir.createDirectory();
    const auto name = "USER-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
    auto state = proc.params.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr)
        dir.getChildFile(name + ".wir3dpreset").replaceWithText(xml->toString());
    refreshPresets();
    for (int i = 0; i < userFiles.size(); ++i)
        if (userFiles[i].getFileNameWithoutExtension() == name)
            presetBox.setSelectedId(101 + i, juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
juce::Rectangle<float> WIR3DEditor::keyRect(int i) const
{
    return juce::Rectangle<float>(178.0f + (float) i * 54.0f, 578.0f, 50.0f, 32.0f);
}

int WIR3DEditor::keyAt(juce::Point<float> p) const
{
    for (int i = 0; i < 12; ++i)
        if (keyRect(i).expanded(2.0f).contains(p)) return i;
    return -1;
}

void WIR3DEditor::mouseDown(const juce::MouseEvent& e)
{
    const int k = keyAt(e.position);
    if (k < 0) return;
    if (auto* prm = proc.params.getParameter("root"))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost(prm->convertTo0to1((float) k));
        prm->endChangeGesture();
    }
    repaint();
}

void WIR3DEditor::mouseMove(const juce::MouseEvent& e)
{
    const int k = keyAt(e.position);
    if (k != hoverKey) { hoverKey = k; repaint(); }
}

void WIR3DEditor::mouseExit(const juce::MouseEvent&)
{
    if (hoverKey != -1) { hoverKey = -1; repaint(); }
}

// ---------------------------------------------------------------------------
void WIR3DEditor::timerCallback()
{
    t += 1.0f / 30.0f;
    lastIn = proc.uiIn.load();
    lastOut = proc.uiOut.load();
    lastTarget = proc.uiTarget.load();
    lastLock = proc.uiLock.load();
    traceIn[(size_t) tracePos] = lastIn;
    traceOut[(size_t) tracePos] = lastOut;
    tracePos = (tracePos + 1) % kTrace;

    meterIn = juce::jmax(proc.uiLevelIn.load(), meterIn * 0.86f);
    meterOut = juce::jmax(proc.uiLevelOut.load(), meterOut * 0.86f);

    if (lastIn > 0.0f) centreMidi += (lastIn - centreMidi) * 0.06f;
    eyeOpen += ((lastIn > 0.0f ? 1.0f : 0.22f) - eyeOpen) * 0.15f;
    pupil += ((lastIn > 0.0f ? 6.5f : 3.0f) - pupil) * 0.2f;
    tickerX -= 0.9f;
    if (tickerW > 1.0f && tickerX < -tickerW) tickerX += tickerW;

    if (glitchFrames > 0) --glitchFrames;
    else if (--nextGlitch <= 0) { glitchFrames = 2 + rng.nextInt(4); nextGlitch = 70 + rng.nextInt(220); }

    repaint();
}

// ---------------------------------------------------------------------------
void WIR3DEditor::drawRoom(juce::Graphics& g)
{
    const float w = (float) kW, h = (float) kH;
    juce::ColourGradient bg(juce::Colour(0xff10161a), w * 0.5f, 0.0f, juce::Colour(0xff040506), w * 0.5f, h, false);
    g.setGradientFill(bg);
    g.fillAll();

    // luz difusa del tubo sobre el cuarto
    juce::ColourGradient glow(juce::Colour(0x2c78d2d0), w * 0.5f, h * 0.42f, juce::Colours::transparentBlack, 0.0f, 0.0f, true);
    g.setGradientFill(glow);
    g.fillRect(0.0f, 0.0f, w, h);

    // escritorio
    juce::ColourGradient desk(juce::Colour(0xff17191b), 0.0f, 548.0f, juce::Colour(0xff080909), 0.0f, h, false);
    g.setGradientFill(desk);
    g.fillRect(0.0f, 548.0f, w, h - 548.0f);
    g.setColour(juce::Colour(0xff2a2d2f));
    g.drawLine(0.0f, 548.0f, w, 548.0f, 1.0f);

    // cables: del techo al monitor, del monitor al teclado y fuera del cuadro
    const float sw = 5.0f * std::sin(t * 0.5f);
    cable(g, 150.0f, 0.0f, 150.0f, 30.0f + sw, 200.0f, 14.0f, 230.0f, 22.0f, juce::Colour(0xff1b1c1d), 4.0f);
    cable(g, 170.0f, 0.0f, 170.0f, 40.0f - sw, 230.0f, 20.0f, 260.0f, 22.0f, kRed.darker(0.5f), 2.5f);
    cable(g, 840.0f, 0.0f, 850.0f, 34.0f + sw, 800.0f, 12.0f, 770.0f, 22.0f, juce::Colour(0xff1b1c1d), 4.0f);
    cable(g, 330.0f, 540.0f, 300.0f, 580.0f, 240.0f, 560.0f, 190.0f, 574.0f, juce::Colour(0xff1e1f20), 3.5f);
    cable(g, 640.0f, 540.0f, 680.0f, 590.0f, 760.0f, 570.0f, 812.0f, 574.0f, juce::Colour(0xff1e1f20), 3.5f);
    cable(g, 560.0f, 540.0f, 560.0f, 600.0f, 700.0f, 610.0f, 760.0f, h, juce::Colour(0xff222324), 5.0f);
    cable(g, 420.0f, 540.0f, 400.0f, 620.0f, 300.0f, 620.0f, 180.0f, h, kPlasticLo.darker(0.6f), 3.0f);
}

void WIR3DEditor::drawMonitorCase(juce::Graphics& g)
{
    const auto cs = juce::Rectangle<float>(24.0f, 18.0f, 952.0f, 526.0f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(cs.translated(0.0f, 9.0f), 30.0f);

    juce::ColourGradient plastic(kPlasticHi, cs.getX(), cs.getY(), kPlasticLo, cs.getX(), cs.getBottom(), false);
    g.setGradientFill(plastic);
    g.fillRoundedRectangle(cs, 28.0f);
    g.setColour(juce::Colours::white.withAlpha(0.40f));
    g.drawRoundedRectangle(cs.reduced(1.5f), 27.0f, 1.5f);
    g.setColour(kPlasticSh.withAlpha(0.7f));
    g.drawRoundedRectangle(cs, 28.0f, 1.0f);

    // hundido del tubo con sombra interior
    const auto recess = juce::Rectangle<float>(44.0f, 34.0f, 912.0f, 470.0f);
    g.setColour(kPlasticSh.withAlpha(0.65f));
    g.fillRoundedRectangle(recess.expanded(3.0f), 36.0f);
    g.setColour(juce::Colour(0xff0a0b0b));
    g.fillRoundedRectangle(recess, 33.0f);
    g.setColour(kScreenBg);
    g.fillRoundedRectangle(kScreen, 26.0f);

    // tornillos
    for (auto p : { juce::Point<float>(40.0f, 32.0f), juce::Point<float>(960.0f, 32.0f),
                    juce::Point<float>(40.0f, 530.0f), juce::Point<float>(960.0f, 530.0f) })
    {
        g.setColour(kPlasticSh);
        g.fillEllipse(p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
        g.setColour(kPlasticHi);
        g.drawLine(p.x - 3.0f, p.y - 1.0f, p.x + 3.0f, p.y + 1.0f, 1.0f);
    }

    // rejilla de ventilacion (arriba a la derecha del bisel)
    g.setColour(kPlasticSh.withAlpha(0.8f));
    for (float x = 700.0f; x < 900.0f; x += 8.0f)
        g.drawLine(x, 22.0f, x, 31.0f, 2.0f);

    // bisel inferior: placa de marca, mandos, ranura y LED
    const auto plate = juce::Rectangle<float>(62.0f, 508.0f, 170.0f, 26.0f);
    g.setColour(kPlasticLo.darker(0.15f));
    g.fillRoundedRectangle(plate, 3.0f);
    g.setColour(juce::Colours::white.withAlpha(0.35f));
    g.drawRoundedRectangle(plate, 3.0f, 1.0f);
    g.setColour(juce::Colour(0xff2b2b27));
    g.setFont(mono(13.0f, 0.35f, true));
    g.drawText("WIR3D-NAVI", plate.toNearestInt(), juce::Justification::centred, false);

    g.setColour(kPlasticSh);
    g.fillRoundedRectangle(juce::Rectangle<float>(520.0f, 516.0f, 150.0f, 6.0f), 3.0f); // ranura de disco
    for (int i = 0; i < 2; ++i)
    {
        const float cx = 790.0f + (float) i * 34.0f;
        g.setColour(kPlasticLo.darker(0.3f));
        g.fillEllipse(cx - 10.0f, 511.0f, 20.0f, 20.0f);
        g.setColour(kPlasticHi);
        g.drawEllipse(cx - 10.0f, 511.0f, 20.0f, 20.0f, 1.0f);
        g.setColour(kPlasticSh);
        g.drawLine(cx, 513.0f, cx, 521.0f, 2.0f);
    }
    const bool on = lastIn > 0.0f || std::fmod(t, 1.6f) < 1.1f;
    g.setColour(on ? kRed : kRed.withAlpha(0.3f));
    g.fillEllipse(904.0f, 515.0f, 10.0f, 10.0f);
    g.setColour(kPlasticSh);
    g.drawEllipse(904.0f, 515.0f, 10.0f, 10.0f, 1.0f);
    g.setFont(mono(8.0f, 0.15f));
    g.setColour(kPlasticSh);
    g.drawText("PWR", juce::Rectangle<int>(896, 527, 26, 10), juce::Justification::centred, false);
}

void WIR3DEditor::drawMenuBar(juce::Graphics& g)
{
    const auto bar = juce::Rectangle<float>(kScreen.getX(), kScreen.getY(), kScreen.getWidth(), 24.0f);
    juce::Path clip;
    clip.addRoundedRectangle(kScreen.getX(), kScreen.getY(), kScreen.getWidth(), kScreen.getHeight(), 26.0f);
    g.saveState();
    g.reduceClipRegion(clip);
    g.setColour(kBar);
    g.fillRect(bar);
    g.setColour(kBarDark);
    g.drawLine(bar.getX(), bar.getBottom(), bar.getRight(), bar.getBottom(), 1.0f);
    g.restoreState();

    const bool glitch = glitchFrames > 0;
    g.setFont(mono(12.0f, 0.2f, true));
    if (glitch)
    {
        g.setColour(kRed.withAlpha(0.8f));
        g.drawText("WIR3D-NAVI OS", juce::Rectangle<int>(86, 50, 200, 20), juce::Justification::centredLeft, false);
        g.setColour(kCyan.withAlpha(0.8f));
        g.drawText("WIR3D-NAVI OS", juce::Rectangle<int>(82, 50, 200, 20), juce::Justification::centredLeft, false);
    }
    g.setColour(juce::Colour(0xff101010));
    g.drawText("WIR3D-NAVI OS", juce::Rectangle<int>(84, 50, 200, 20), juce::Justification::centredLeft, false);
    g.setFont(mono(11.0f, 0.1f));
    g.drawText("FILE   LAYER   WIRED   " + jpWired(), juce::Rectangle<int>(224, 50, 300, 20), juce::Justification::centredLeft, false);

    g.setColour(kRed);
    g.fillEllipse(70.0f, 56.0f, 8.0f, 8.0f);
    g.setColour(juce::Colour(0xff101010));
    g.setFont(mono(11.0f, 0.1f, true));
    g.drawText(juce::Time::getCurrentTime().toString(false, true, true, true),
               juce::Rectangle<int>(846, 50, 84, 20), juce::Justification::centredRight, false);
}

void WIR3DEditor::drawEye(juce::Graphics& g, juce::Point<float> c)
{
    const float rx = 34.0f, ry = 28.0f * eyeOpen + 2.0f;
    juce::Path eye;
    eye.startNewSubPath(c.x - rx, c.y);
    eye.quadraticTo(c.x, c.y - ry * 1.5f, c.x + rx, c.y);
    // con SMILE el parpado inferior se curva hacia arriba: ojo sonriente
    const float smileAmt = juce::jlimit(0.0f, 1.0f, (float) knobs[6].getValue() * 0.01f);
    eye.quadraticTo(c.x, c.y + ry * 1.5f * (1.0f - 1.7f * smileAmt), c.x - rx, c.y);
    eye.closeSubPath();

    g.setColour(juce::Colour(0xff0b0c0d));
    g.fillPath(eye);

    g.saveState();
    g.reduceClipRegion(eye);
    const float ox = std::sin(t * 0.7f) * 5.0f, oy = std::cos(t * 0.5f) * 2.0f;
    const juce::Point<float> ic(c.x + ox, c.y + oy);
    for (int i = 0; i < 4; ++i)
    {
        const float rr = 17.0f - (float) i * 3.5f;
        g.setColour((i % 2 == 0 ? kRed : kCyan).withAlpha(0.5f));
        g.drawEllipse(ic.x - rr, ic.y - rr, rr * 2.0f, rr * 2.0f, 1.0f);
    }
    g.setColour(kScreenBg);
    g.fillEllipse(ic.x - pupil, ic.y - pupil, pupil * 2.0f, pupil * 2.0f);
    g.setColour(kText.withAlpha(0.85f));
    g.fillEllipse(ic.x - pupil * 0.35f - 2.5f, ic.y - pupil * 0.35f - 3.0f, 3.0f, 3.0f);
    g.restoreState();

    g.setColour(kText.withAlpha(0.8f));
    g.strokePath(eye, juce::PathStrokeType(1.2f));
    g.setColour(kDim);
    g.drawLine(c.x - rx - 10.0f, c.y, c.x - rx - 3.0f, c.y, 1.0f);
    g.drawLine(c.x + rx + 3.0f, c.y, c.x + rx + 10.0f, c.y, 1.0f);
}

void WIR3DEditor::drawTrace(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour(juce::Colour(0xff060b0c));
    g.fillRect(area);
    g.setColour(kLine);
    g.drawRect(area, 1.0f);

    g.saveState();
    g.reduceClipRegion(area.toNearestInt());

    const float lo = centreMidi - 8.0f;
    auto yOf = [&](float m) { return area.getBottom() - (m - lo) / 16.0f * area.getHeight(); };

    g.setFont(mono(9.0f));
    for (int m = (int) std::ceil(lo); m <= (int) std::floor(lo + 16.0f); ++m)
    {
        const float y = yOf((float) m);
        const bool natural = noteName(m).containsChar('#') == false;
        g.setColour(natural ? kLine.brighter(0.5f) : kLine);
        g.drawLine(area.getX(), y, area.getRight(), y, 1.0f);
        g.setColour(natural ? kDim : kDim.withAlpha(0.45f));
        g.drawText(noteName(m), juce::Rectangle<int>((int) area.getX() + 4, (int) y - 6, 34, 12),
                   juce::Justification::centredLeft, false);
    }

    auto drawLine = [&](const std::array<float, kTrace>& arr, juce::Colour col, float thick)
    {
        juce::Path path;
        bool pen = false;
        float prev = 0.0f;
        for (int i = 0; i < kTrace; ++i)
        {
            const float v = arr[(size_t) ((tracePos + i) % kTrace)];
            const float x = area.getX() + area.getWidth() * (float) i / (float) (kTrace - 1);
            if (v < 0.0f) { pen = false; continue; }
            if (pen && std::abs(v - prev) > 4.0f) pen = false;
            const float y = juce::jlimit(area.getY(), area.getBottom(), yOf(v));
            if (! pen) { path.startNewSubPath(x, y); pen = true; }
            else path.lineTo(x, y);
            prev = v;
        }
        g.setColour(col.withAlpha(0.22f));
        g.strokePath(path, juce::PathStrokeType(thick + 3.0f));
        g.setColour(col);
        g.strokePath(path, juce::PathStrokeType(thick));
    };
    drawLine(traceIn, kCyan.withAlpha(0.85f), 1.0f);
    drawLine(traceOut, kRed, 1.7f);

    g.setColour(kText.withAlpha(0.5f));
    g.drawLine(area.getRight() - 1.0f, area.getY(), area.getRight() - 1.0f, area.getBottom(), 1.0f);
    g.restoreState();
}

void WIR3DEditor::drawMeters(juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setFont(mono(9.0f, 0.1f));
    for (int m = 0; m < 2; ++m)
    {
        const float level = m == 0 ? meterIn : meterOut;
        const float db = 20.0f * std::log10(juce::jmax(level, 1.0e-5f));
        const float x = area.getX() + (float) m * 22.0f;
        const int segs = 16;
        const float segH = (area.getHeight() - 14.0f) / (float) segs;
        for (int k = 0; k < segs; ++k)
        {
            const bool lit = db >= -48.0f + (float) k * 3.2f;
            const float y = area.getBottom() - 14.0f - (float) (k + 1) * segH;
            juce::Colour c = k >= segs - 2 ? kRed : (k >= segs - 5 ? juce::Colour(0xffe0b34a) : kCyan);
            g.setColour(lit ? c : c.withAlpha(0.12f));
            g.fillRect(x, y + 1.0f, 16.0f, segH - 2.0f);
        }
        g.setColour(kDim);
        g.drawText(m == 0 ? "IN" : "OUT", juce::Rectangle<int>((int) x - 4, (int) area.getBottom() - 12, 24, 12),
                   juce::Justification::centred, false);
    }
}

void WIR3DEditor::drawTuning(juce::Graphics& g, juce::Rectangle<float> win)
{
    drawEye(g, juce::Point<float>(win.getX() + 52.0f, win.getY() + 70.0f));

    // nota objetivo en grande
    const juce::String big = lastTarget >= 0.0f ? noteName((int) std::lround(lastTarget)) : juce::String("--");
    g.setColour(kText);
    g.setFont(mono(44.0f, 0.1f, true));
    g.drawText(big, juce::Rectangle<int>((int) win.getX() + 112, (int) win.getY() + 24, 170, 50),
               juce::Justification::centredLeft, false);
    g.setFont(mono(11.0f, 0.1f));
    g.setColour(kCyan);
    g.drawText("IN  " + noteLabel(lastIn), juce::Rectangle<int>((int) win.getX() + 114, (int) win.getY() + 80, 170, 14),
               juce::Justification::centredLeft, false);
    g.setColour(kRed);
    g.drawText("OUT " + noteLabel(lastOut), juce::Rectangle<int>((int) win.getX() + 114, (int) win.getY() + 95, 170, 14),
               juce::Justification::centredLeft, false);

    // medidor de cents (entrada respecto a la nota objetivo)
    const auto meter = juce::Rectangle<float>(win.getX() + 14.0f, win.getBottom() - 38.0f, win.getWidth() - 100.0f, 12.0f);
    g.setColour(juce::Colour(0xff060b0c));
    g.fillRect(meter);
    g.setColour(kLine.brighter(0.4f));
    g.drawRect(meter, 1.0f);
    for (int c = -50; c <= 50; c += 10)
    {
        const float x = meter.getCentreX() + (float) c / 50.0f * meter.getWidth() * 0.5f;
        g.setColour(c == 0 ? kRed : kDim);
        g.drawLine(x, meter.getY() + (c % 50 == 0 ? 1.0f : 3.0f), x, meter.getBottom() - (c % 50 == 0 ? 1.0f : 3.0f), 1.0f);
    }
    if (lastTarget >= 0.0f && lastIn >= 0.0f)
    {
        const float dev = juce::jlimit(-50.0f, 50.0f, (lastIn - lastTarget) * 100.0f);
        const float x = meter.getCentreX() + dev / 50.0f * meter.getWidth() * 0.5f;
        juce::Path tri;
        tri.addTriangle(x - 4.0f, meter.getBottom() + 7.0f, x + 4.0f, meter.getBottom() + 7.0f, x, meter.getBottom() + 1.0f);
        g.setColour(lastLock ? kCyan : kText);
        g.fillPath(tri);
    }
    g.setFont(mono(8.0f));
    g.setColour(kDim);
    g.drawText("-50", juce::Rectangle<int>((int) meter.getX() - 6, (int) meter.getBottom() + 8, 30, 10), juce::Justification::centredLeft, false);
    g.drawText("CENTS", juce::Rectangle<int>((int) meter.getCentreX() - 20, (int) meter.getBottom() + 8, 40, 10), juce::Justification::centred, false);
    g.drawText("+50", juce::Rectangle<int>((int) meter.getRight() - 24, (int) meter.getBottom() + 8, 30, 10), juce::Justification::centredRight, false);

    // lampara LOCK
    const auto lamp = juce::Rectangle<float>(win.getRight() - 66.0f, win.getBottom() - 44.0f, 52.0f, 22.0f);
    g.setColour(lastLock ? kRed : juce::Colour(0xff150d0d));
    g.fillRect(lamp);
    g.setColour(lastLock ? kRed.brighter(0.4f) : kLine);
    g.drawRect(lamp, 1.0f);
    g.setFont(mono(11.0f, 0.15f, true));
    g.setColour(lastLock ? juce::Colours::white : kDim);
    g.drawText("LOCK", lamp.toNearestInt(), juce::Justification::centred, false);
}

void WIR3DEditor::drawTicker(juce::Graphics& g, juce::Rectangle<float> area)
{
    const juce::String msg = "WIR3D.NAVI  //  PITCH LAYER ACTIVE  //  " + jpSignal()
                           + "  //  ARE YOU CONNECTED?  //  " + jpLink() + "  //  ";
    const auto font = mono(10.0f, 0.08f);
    if (tickerW < 1.0f)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText(font, msg, 0.0f, 0.0f);
        tickerW = ga.getBoundingBox(0, -1, true).getWidth();
    }
    g.saveState();
    g.reduceClipRegion(area.toNearestInt());
    g.setColour(kDim);
    g.setFont(font);
    const int h = (int) area.getHeight();
    for (float x = area.getX() + tickerX; x < area.getRight(); x += tickerW)
        g.drawText(msg, juce::Rectangle<int>((int) x, (int) area.getY(), (int) tickerW + 4, h),
                   juce::Justification::centredLeft, false);
    g.restoreState();
}

void WIR3DEditor::drawKeyboard(juce::Graphics& g)
{
    // chasis del teclado
    const auto base = juce::Rectangle<float>(150.0f, 556.0f, 700.0f, 76.0f);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(base.translated(0.0f, 6.0f), 8.0f);
    juce::ColourGradient chassis(kPlasticMid, base.getX(), base.getY(), kPlasticLo, base.getX(), base.getBottom(), false);
    g.setGradientFill(chassis);
    g.fillRoundedRectangle(base, 8.0f);
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawRoundedRectangle(base.reduced(1.0f), 7.0f, 1.0f);

    // fila superior decorativa
    for (int i = 0; i < 14; ++i)
    {
        const auto k = juce::Rectangle<float>(178.0f + (float) i * 46.0f, 561.0f, 40.0f, 11.0f);
        g.setColour(kPlasticLo.darker(0.2f));
        g.fillRoundedRectangle(k, 2.0f);
        g.setColour(kPlasticHi.withAlpha(0.6f));
        g.drawLine(k.getX() + 2.0f, k.getY() + 1.0f, k.getRight() - 2.0f, k.getY() + 1.0f, 1.0f);
    }

    // teclas de notas: eligen la tonica (KEY)
    const int root = (int) proc.params.getRawParameterValue("root")->load();
    for (int i = 0; i < 12; ++i)
    {
        const bool sel = i == root, hov = i == hoverKey;
        auto k = keyRect(i);
        const float press = sel ? 2.0f : 0.0f;
        g.setColour(kPlasticSh);
        g.fillRoundedRectangle(k.translated(0.0f, 3.0f - press), 4.0f);                // lateral de la tecla
        k = k.translated(0.0f, press);
        juce::ColourGradient cap(hov ? kPlasticHi.brighter(0.1f) : kPlasticHi, k.getX(), k.getY(), kPlasticMid, k.getX(), k.getBottom(), false);
        g.setGradientFill(cap);
        g.fillRoundedRectangle(k, 4.0f);
        g.setColour(sel ? kRed : kPlasticSh.withAlpha(0.7f));
        g.drawRoundedRectangle(k, 4.0f, sel ? 1.6f : 1.0f);
        if (sel)
        {
            g.setColour(kRed);
            g.fillRect(k.getX() + 12.0f, k.getY() + 4.0f, k.getWidth() - 24.0f, 2.5f);   // LED de la tecla activa
        }
        g.setFont(mono(12.0f, 0.1f, true));
        g.setColour(sel ? kRed.darker(0.2f) : juce::Colour(0xff2a2924));
        g.drawText(kKeyNames[i], k.toNearestInt().translated(0, 3), juce::Justification::centred, false);
    }

    // barra espaciadora decorativa y etiqueta
    const auto sp = juce::Rectangle<float>(300.0f, 616.0f, 400.0f, 12.0f);
    g.setColour(kPlasticLo.darker(0.2f));
    g.fillRoundedRectangle(sp, 3.0f);
    g.setColour(kPlasticHi.withAlpha(0.6f));
    g.drawLine(sp.getX() + 3.0f, sp.getY() + 1.0f, sp.getRight() - 3.0f, sp.getY() + 1.0f, 1.0f);
    g.setFont(mono(9.0f, 0.2f, true));
    g.setColour(juce::Colour(0xff3a3932));
    g.drawText("KEY", juce::Rectangle<int>(156, 582, 20, 26), juce::Justification::centred, false);
}

// ---------------------------------------------------------------------------
void WIR3DEditor::paint(juce::Graphics& g)
{
    drawRoom(g);
    drawMonitorCase(g);

    // contenido de la pantalla (recortado a su forma redondeada)
    juce::Path screenPath;
    screenPath.addRoundedRectangle(kScreen.getX(), kScreen.getY(), kScreen.getWidth(), kScreen.getHeight(), 26.0f);
    g.saveState();
    g.reduceClipRegion(screenPath);

    juce::ColourGradient crt(juce::Colour(0xff0d1b1e), kScreen.getCentreX(), kScreen.getCentreY(), kScreenBg, kScreen.getX(), kScreen.getY(), true);
    g.setGradientFill(crt);
    g.fillRect(kScreen);

    // ventana 1: traza de afinacion + medidores
    drawWindow(g, kWinTrace, "LAYER 01  //  PITCH TRACE  " + jpSignal());
    const auto traceArea = juce::Rectangle<float>(kWinTrace.getX() + 8.0f, kWinTrace.getY() + 24.0f, kWinTrace.getWidth() - 74.0f, kWinTrace.getHeight() - 32.0f);
    drawTrace(g, traceArea);
    drawMeters(g, juce::Rectangle<float>(kWinTrace.getRight() - 56.0f, kWinTrace.getY() + 24.0f, 46.0f, kWinTrace.getHeight() - 30.0f));

    // ventana 2: ojo, nota objetivo, medidor de cents y LOCK
    drawWindow(g, kWinTune, "LAYER 02  //  TUNING");
    drawTuning(g, kWinTune);

    // ventana 3: parametros
    drawWindow(g, kWinParams, "LAYER 03  //  PARAMETERS");
    g.setFont(mono(10.0f, 0.15f, true));
    g.setColour(kText);
    for (int i = 0; i < kKnobs; ++i)
        g.drawText(kLabels[i], juce::Rectangle<int>(kCellX + i * kCellStep, 284, kCellStep, 14), juce::Justification::centred, false);
    g.setColour(kLine);
    g.drawLine(kWinParams.getX() + 10.0f, 406.0f, kWinParams.getRight() - 10.0f, 406.0f, 1.0f);
    g.setFont(mono(10.0f, 0.15f));
    g.setColour(kDim);
    g.drawText("SCALE", juce::Rectangle<int>(76, 424, 44, 20), juce::Justification::centredLeft, false);
    g.drawText("RANGE", juce::Rectangle<int>(268, 424, 44, 20), juce::Justification::centredLeft, false);
    g.drawText("DIV",   juce::Rectangle<int>(448, 424, 30, 20), juce::Justification::centredLeft, false);
    g.drawText("A4",    juce::Rectangle<int>(572, 424, 22, 20), juce::Justification::centredLeft, false);

    drawMenuBar(g);
    drawTicker(g, juce::Rectangle<float>(kScreen.getX() + 10.0f, 468.0f, kScreen.getWidth() - 20.0f, 18.0f));
    g.restoreState();

    drawKeyboard(g);
}

void WIR3DEditor::paintOverChildren(juce::Graphics& g)
{
    juce::Path screenPath;
    screenPath.addRoundedRectangle(kScreen.getX(), kScreen.getY(), kScreen.getWidth(), kScreen.getHeight(), 26.0f);
    g.saveState();
    g.reduceClipRegion(screenPath);
    const auto sb = kScreen.toNearestInt();

    // scanlines y grano (el grano se desplaza al azar en cada fotograma)
    g.setTiledImageFill(scanImg, 0, 0, 1.0f);
    g.fillRect(sb);
    g.setTiledImageFill(noiseImg, rng.nextInt(128), rng.nextInt(128), 1.0f);
    g.fillRect(sb);

    // curvatura del cristal: viñeta + brillo
    juce::ColourGradient vig(juce::Colours::transparentBlack, kScreen.getCentreX(), kScreen.getCentreY(),
                             juce::Colour(0xd8000000), kScreen.getX(), kScreen.getY(), true);
    g.setGradientFill(vig);
    g.fillRect(sb);
    juce::ColourGradient shine(juce::Colour(0x1effffff), kScreen.getX() + 40.0f, kScreen.getY() + 10.0f,
                               juce::Colours::transparentWhite, kScreen.getX() + 360.0f, kScreen.getY() + 190.0f, false);
    g.setGradientFill(shine);
    g.fillRect(sb);

    // ruptura de imagen ocasional
    if (glitchFrames > 0)
    {
        for (int i = 0; i < 3; ++i)
        {
            const float y = kScreen.getY() + (float) rng.nextInt((int) kScreen.getHeight() - 4);
            g.setColour((i == 0 ? kRed : kText).withAlpha(0.16f + 0.18f * rng.nextFloat()));
            g.fillRect(kScreen.getX(), y, kScreen.getWidth(), 1.0f + (float) rng.nextInt(3));
        }
    }
    g.restoreState();
}

void WIR3DEditor::resized()
{
    for (int i = 0; i < kKnobs; ++i)
        knobs[(size_t) i].setBounds(kCellX + i * kCellStep, kKnobY, kCellStep, kKnobH);
    scaleBox.setBounds(122, 424, 138, 20);
    rangeBox.setBounds(314, 424, 126, 20);
    divBox.setBounds(480, 424, 80, 20);
    refA4.setBounds(598, 424, 220, 20);
    presetBox.setBounds(540, 50, 220, 20);
    saveBtn.setBounds(766, 50, 54, 20);
}
