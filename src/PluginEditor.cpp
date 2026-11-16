// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>

namespace sidsynth
{
namespace
{
const juce::Colour paper{0xffecebe5}, panel{0xfff7f6f0}, ink{0xff25362f}, muted{0xff637067},
    green{0xff257e69}, line{0xffcdcec4}, field{0xffe9e9e1};
using State = juce::AudioProcessorValueTreeState;

void text(juce::Graphics& g, const juce::String& value, int x, int y, int w, int h,
          float size = 11, juce::Colour colour = muted, bool bold = false)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    g.drawText(value, x, y, w, h, juce::Justification::centredLeft);
}

struct RackLook final : juce::LookAndFeel_V4
{
    RackLook()
    {
        setDefaultSansSerifTypefaceName("Noto Sans");
        setColour(juce::Slider::textBoxTextColourId, ink);
        setColour(juce::Slider::textBoxBackgroundColourId, panel);
        setColour(juce::Slider::textBoxOutlineColourId, line);
        setColour(juce::Slider::textBoxHighlightColourId, green.withAlpha(0.35f));
        setColour(juce::ComboBox::backgroundColourId, field);
        setColour(juce::ComboBox::textColourId, ink);
        setColour(juce::ComboBox::outlineColourId, line);
        setColour(juce::ComboBox::arrowColourId, muted);
        setColour(juce::PopupMenu::backgroundColourId, panel);
        setColour(juce::PopupMenu::textColourId, ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, green);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextButton::textColourOffId, ink);
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        setColour(juce::Label::textColourId, ink);
        setColour(juce::Label::textWhenEditingColourId, ink);
        setColour(juce::TextEditor::backgroundColourId, panel);
        setColour(juce::TextEditor::textColourId, ink);
        setColour(juce::TextEditor::highlightedTextColourId, ink);
        setColour(juce::TextEditor::highlightColourId, green.withAlpha(0.35f));
        setColour(juce::CaretComponent::caretColourId, ink);
        setColour(juce::TooltipWindow::backgroundColourId, ink);
        setColour(juce::TooltipWindow::textColourId, panel);
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::FontOptions(11.0f); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return juce::FontOptions(12.0f); }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        auto colour = b.getToggleState() ? green : field;
        if (over || down) colour = colour.darker(down ? 0.12f : 0.04f);
        g.setColour(colour.withMultipliedAlpha(b.isEnabled() ? 1.0f : 0.4f));
        g.fillRoundedRectangle(r, 4);
        g.setColour(b.hasKeyboardFocus(true) ? green : line);
        g.drawRoundedRectangle(r, 4, b.hasKeyboardFocus(true) ? 2.0f : 1.0f);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const int wave = static_cast<int>(b.getProperties().getWithDefault("wave", 0));
        auto colour = b.getToggleState() ? juce::Colours::white : ink;
        g.setColour(colour.withMultipliedAlpha(b.isEnabled() ? 1.0f : 0.4f));
        if (wave != 0)
        {
            juce::Path p;
            if (wave == 1) { p.startNewSubPath(12, 19); p.lineTo(20, 8); p.lineTo(28, 19); p.lineTo(36, 8); p.lineTo(44, 19); }
            if (wave == 2) { p.startNewSubPath(12, 19); p.lineTo(27, 8); p.lineTo(27, 19); p.lineTo(43, 8); p.lineTo(43, 19); }
            if (wave == 3) { p.startNewSubPath(12, 19); p.lineTo(12, 8); p.lineTo(27, 8); p.lineTo(27, 19); p.lineTo(43, 19); p.lineTo(43, 8); }
            if (wave == 4) { p.startNewSubPath(12, 16); p.lineTo(17, 10); p.lineTo(22, 19); p.lineTo(27, 7); p.lineTo(32, 17); p.lineTo(37, 11); p.lineTo(44, 16); }
            g.strokePath(p, juce::PathStrokeType(1.5f));
        }
        g.setFont(juce::FontOptions(wave ? 10.0f : 11.0f));
        g.drawText(b.getButtonText(), 2, wave ? 25 : 0, b.getWidth() - 4, wave ? 17 : b.getHeight(), juce::Justification::centred);
    }
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float position,
                          float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const float centre = static_cast<float>(y) + static_cast<float>(h) * 0.5f;
        g.setColour(line.withMultipliedAlpha(s.isEnabled() ? 1.0f : 0.4f));
        g.fillRect(static_cast<float>(x), centre - 1.5f, static_cast<float>(w), 3.0f);
        g.setColour(green.withMultipliedAlpha(s.isEnabled() ? 1.0f : 0.4f));
        g.fillRect(static_cast<float>(x), centre - 1.5f, juce::jmax(0.0f, position - static_cast<float>(x)), 3.0f);
        g.setColour(s.hasKeyboardFocus(true) ? green : ink.withMultipliedAlpha(s.isEnabled() ? 1.0f : 0.4f));
        g.fillRoundedRectangle(position - 3, centre - 7, 6, 14, 1.5f);
    }
};

juce::String help(std::size_t key)
{
    using namespace parameters;
    const auto& s = specs()[key];
    juce::String h = s.name + ". ";
    if (s.kind == Kind::boolean) h += "Independent on/off switch.";
    else if (s.kind == Kind::choice) h += s.choices.replace("|", " / ") + ".";
    else h += juce::String(s.minimum) + " to " + juce::String(s.maximum) + " " + s.unit
            + ". Click the value to type; arrows adjust; double-click the track resets to default.";
    if (key >= voiceStart)
    {
        const auto f = (key - voiceStart) % voiceFieldCount;
        if (f == attack) h += " Raw SID rate: 2, 8, 16, 24, 38, 56, 68, 80, 100, 250, 500, 800, 1000, 3000, 5000, 8000 ms (nominal).";
        if (f == decay || f == release) h += " Raw SID rate: 6, 24, 48, 72, 114, 168, 204, 240, 300, 750, 1500, 2400, 3000, 9000, 15000, 24000 ms (nominal).";
        if (f == sustain) h += " Linear level, register / 15.";
        if (f == manualGate) h += " Manual source only. Panic/stop/recall disarm it; toggle off then on to rearm. The live GATE lamp reports the actual gate.";
        if (f == frequency) h += " Manual source only; Hz = register x chip clock / 16777216.";
        if (f == tuning) h += " MIDI/audition only. 0.01 semitone = 1 cent.";
        if (f == test) h += " Actual SID TEST bit; resets oscillator/noise and suppresses its waveform while on.";
        if (f == ring) h += " Hardware ring modulation acts on the triangle waveform.";
    }
    if (key == cutoff) h += " Hardware cutoff, not calibrated Hz; response depends on model.";
    if (key == voice3Direct) h += " Off removes only the unfiltered path; filter and sync/ring paths remain.";
    if (key == routeExternal) h += " Requires the host to activate EXT IN. Renoise's tested native instrument routing does not expose an input destination.";
    return h;
}

struct Control final : juce::Component
{
    juce::Label label;
    juce::Slider slider;
    juce::ComboBox choice;
    juce::TextButton toggle;
    std::unique_ptr<State::SliderAttachment> sliderAttachment;
    std::unique_ptr<State::ComboBoxAttachment> choiceAttachment;
    std::unique_ptr<State::ButtonAttachment> buttonAttachment;
    Control(State& state, std::size_t key, juce::String caption)
    {
        using namespace parameters;
        const auto& s = specs()[key];
        setComponentID("control." + s.id);
        label.setText(caption, juce::dontSendNotification);
        label.setFont(juce::FontOptions(10.0f));
        label.setColour(juce::Label::textColourId, muted);
        label.setBorderSize({0, 0, 0, 0});
        addAndMakeVisible(label);
        const auto tip = help(key);
        if (s.kind == Kind::boolean)
        {
            toggle.setButtonText(caption);
            toggle.setClickingTogglesState(true);
            toggle.setTooltip(tip);
            toggle.setComponentID(s.id); toggle.setTitle(s.name);
            addAndMakeVisible(toggle);
            buttonAttachment = std::make_unique<State::ButtonAttachment>(state, s.id, toggle);
        }
        else if (s.kind == Kind::choice || (key >= layeredChannel && key <= channel3))
        {
            if (s.kind == Kind::choice) choice.addItemList(juce::StringArray::fromTokens(s.choices, "|", ""), 1);
            else for (int i = 1; i <= 16; ++i) choice.addItem("CH " + juce::String(i), i);
            choice.setTooltip(tip);
            choice.setComponentID(s.id); choice.setTitle(s.name);
            choice.setWantsKeyboardFocus(true);
            addAndMakeVisible(choice);
            choiceAttachment = std::make_unique<State::ComboBoxAttachment>(state, s.id, choice);
        }
        else
        {
            slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, 90, 22);
            slider.setRange(s.minimum, s.maximum, s.step);
            slider.setDoubleClickReturnValue(true, s.initial);
            slider.setNumDecimalPlacesToDisplay(key == gain ? 1 : s.kind == Kind::continuous ? 2 : 0);
            if (s.unit == "st" || s.unit == "dB") slider.setTextValueSuffix(" " + s.unit);
            slider.setTooltip(tip);
            slider.setComponentID(s.id); slider.setTitle(s.name);
            slider.setWantsKeyboardFocus(true);
            slider.setScrollWheelEnabled(false);
            addAndMakeVisible(slider);
            sliderAttachment = std::make_unique<State::SliderAttachment>(state, s.id, slider);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        const bool isButton = buttonAttachment != nullptr;
        const int header = label.getText().isEmpty() || isButton ? 0 : 17;
        label.setBounds(r.removeFromTop(header));
        if (isButton) toggle.setBounds(r);
        else if (choiceAttachment) choice.setBounds(r);
        else
        {
            slider.setTextBoxStyle(juce::Slider::TextBoxAbove, false, juce::jmin(110, r.getWidth()), 22);
            slider.setBounds(r);
        }
    }
};
}

struct PluginEditor::Rack final : juce::Component, private juce::Timer, private juce::MidiKeyboardState::Listener
{
    PluginProcessor& processor;
    PluginEditor& editor;
    RackLook look;
    std::array<std::unique_ptr<Control>, parameters::count> controls;
    juce::TextButton panic{"Panic / reset"}, advanced{"Advanced"};
    juce::ComboBox target, zoom;
    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard{keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard};
    struct Hardware final : juce::Component
    {
        void paint(juce::Graphics& g) override
        {
            g.setColour(panel); g.fillRoundedRectangle(getLocalBounds().toFloat(), 7);
            g.setColour(green); g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1), 7, 2);
            text(g, "ADVANCED HARDWARE", 18, 12, 300, 24, 13, ink, true);
            text(g, "TEST holds the oscillator in reset while enabled.", 18, 46, 380, 20);
            text(g, "EXT IN needs host routing; unavailable in tested Renoise routing.", 18, 150, 410, 38);
        }
    } hardware;
    juce::TextButton closeAdvanced{"Close"};
    std::array<int, 128> heldOrder{};
    int orderSize = 0, targetVoice = 0, resetFlash = 0;
    unsigned epoch = 0, gates = 0;
    bool layered = false;
    std::array<bool, 3> manual{};

    Rack(PluginProcessor& p, PluginEditor& e) : processor(p), editor(e)
    {
        setLookAndFeel(&look);
        setFocusContainerType(FocusContainerType::focusContainer);
        using namespace parameters;
        auto add = [&](std::size_t key, const juce::String& title, int x, int y, int w, int h, juce::Component* parent = nullptr)
        {
            controls[key] = std::make_unique<Control>(p.state, key, title);
            auto& c = *controls[key];
            (parent ? parent : this)->addAndMakeVisible(c);
            c.setBounds(x, y, w, h);
        };
        add(gain, "OUTPUT dB", 295, 9, 230, 57);
        add(mode, "PLAY MODE", 560, 6, 160, 53);
        add(model, "SID MODEL", 744, 6, 112, 53);
        add(volume, "SID VOLUME  / 15", 882, 9, 110, 57);
        add(clock, "", 74, 88, 96, 26);
        add(layeredChannel, "LAYERED INPUT", 214, 78, 140, 41);
        panic.setComponentID("panic"); panic.setTooltip("Clear MIDI and audition notes; disarm manual holds. Saved patch settings stay available.");
        // Panic acts on press, including when a mouse-up is intercepted by the
        // host. Brief feedback makes the momentary action visible.
        panic.setTriggeredOnMouseDown(true);
        panic.onClick = [this] { releaseKeys(); processor.requestPanic(); resetFlash = 45; panic.setButtonText("Reset"); };
        addAndMakeVisible(panic); panic.setBounds(970, 88, 126, 28);
        for (int v = 0; v < 3; ++v)
        {
            const int y = 130 + v * 177;
            add(channel1 + v, "", 75, y + 39, 88, 26);
            const std::array<const char*, 4> waves{"TRI", "SAW", "PULSE", "NOISE"};
            for (int w = 0; w < 4; ++w)
            {
                auto key = voice(v, static_cast<VoiceField>(triangle + w));
                add(key, waves[static_cast<std::size_t>(w)], 182 + w * 63, y + 16, 58, 44);
                controls[key]->toggle.getProperties().set("wave", w + 1);
            }
            add(voice(v, source), "SOURCE", 453, y + 3, 86, 53);
            add(voice(v, tuning), "TUNE st", 550, y + 3, 92, 62);
            add(voice(v, frequency), "FREQ  / 65535", 550, y + 3, 92, 62);
            add(voice(v, manualGate), "Hold", 655, y + 21, 86, 35);
            add(voice(v, pulseWidth), "PULSE  / 4095", 44, y + 86, 124, 65);
            const std::array<const char*, 4> envelope{"Attack / 15", "Decay / 15", "Sustain / 15", "Release / 15"};
            for (int a = 0; a < 4; ++a)
                add(voice(v, static_cast<VoiceField>(attack + a)), envelope[static_cast<std::size_t>(a)], 188 + a * 84, y + 86, 70, 65);
            const auto modulator = juce::String(v == 0 ? 3 : v);
            add(voice(v, parameters::sync), "Sync < V" + modulator, 545, y + 91, 91, 27);
            add(voice(v, ring), "Ring < V" + modulator, 645, y + 91, 91, 27);
        }
        for (int v = 0; v < 3; ++v) add(route1 + v, "Voice " + juce::String(v + 1), 806 + v * 90, 202, 82, 32);
        add(cutoff, "CUTOFF  / 2047", 806, 252, 268, 80);
        add(resonance, "RESONANCE  / 15", 806, 354, 268, 64);
        add(lowPass, "LP", 806, 440, 82, 34); add(bandPass, "BP", 897, 440, 82, 34); add(highPass, "HP", 988, 440, 82, 34);
        add(voice3Direct, "Voice 3 direct output", 806, 594, 268, 30);
        addAndMakeVisible(target); target.setComponentID("audition.target");
        target.addItemList({"Voice 1", "Voice 2", "Voice 3"}, 1); target.setSelectedId(1);
        target.setTitle("Audition target voice"); target.setTooltip("3 parts: audition only the selected oscillator, independently of shared MIDI channels. Layered: audition all MIDI-source oscillators. Manual voices use Hold.");
        target.onChange = [this] { releaseKeys(); targetVoice = juce::jmax(0, target.getSelectedId() - 1); };
        target.setBounds(42, 704, 146, 30);
        keyboard.setComponentID("audition.keyboard"); keyboard.setTitle("Audition keyboard, MIDI C3 to B5");
        keyboard.setAvailableRange(48, 83); keyboard.setLowestVisibleKey(48); keyboard.setKeyWidth(27.0f);
        keyboard.setScrollButtonsVisible(false); keyboard.setOctaveForMiddleC(4);
        keyboard.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, panel);
        keyboard.setColour(juce::MidiKeyboardComponent::blackNoteColourId, ink);
        keyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, green.withAlpha(0.6f));
        keyboard.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, green.withAlpha(0.2f));
        // Mouse audition has explicit ownership. Tab/arrow navigation remains
        // available for parameter editing; host typing is not captured as notes.
        keyboard.setKeyPressBaseOctave(4); keyboard.clearKeyMappings();
        addAndMakeVisible(keyboard); keyboard.setBounds(229, 680, 567, 64);
        keyboardState.addListener(this);
        addAndMakeVisible(advanced); advanced.setBounds(832, 681, 144, 30);
        advanced.setComponentID("advanced"); advanced.setTooltip("Show hardware TEST/reset bits and external-input filter routing.");
        advanced.onClick = [this] { hardware.setVisible(!hardware.isVisible()); if (hardware.isVisible()) { releaseKeys(); hardware.toFront(false); if (closeAdvanced.isShowing()) closeAdvanced.grabKeyboardFocus(); } };
        zoom.addItemList({"80%", "100%", "125%", "150%"}, 1); zoom.setSelectedId(2);
        zoom.setTitle("Editor size"); zoom.setComponentID("editor.zoom"); zoom.setTooltip("Resize the scalable Rack editor. You can also drag its lower-right corner.");
        zoom.onChange = [this] { const double scales[]{0.8, 1.0, 1.25, 1.5}; const auto scale = scales[juce::jlimit(0, 3, zoom.getSelectedId() - 1)]; editor.setSize(juce::roundToInt(1120 * scale), juce::roundToInt(780 * scale)); };
        addAndMakeVisible(zoom); zoom.setBounds(1010, 24, 86, 30);
        addChildComponent(hardware); hardware.setBounds(650, 428, 446, 226);
        hardware.setFocusContainerType(FocusContainerType::focusContainer);
        hardware.setComponentID("advanced.panel");
        for (int v = 0; v < 3; ++v) add(voice(v, test), "V" + juce::String(v + 1) + " TEST", 18 + v * 136, 86, 126, 30, &hardware);
        add(routeExternal, "Route EXT IN to filter", 18, 184, 270, 28, &hardware);
        closeAdvanced.setComponentID("advanced.close"); closeAdvanced.onClick = [this] { hardware.setVisible(false); if (advanced.isShowing()) advanced.grabKeyboardFocus(); };
        hardware.addAndMakeVisible(closeAdvanced); closeAdvanced.setBounds(344, 14, 84, 28);
        epoch = processor.auditionEpoch();
        updateControls(); startTimerHz(30);
    }
    ~Rack() override
    {
        stopTimer(); releaseKeys(); keyboardState.removeListener(this); setLookAndFeel(nullptr);
    }
    float raw(std::size_t key) const { return processor.state.getRawParameterValue(parameters::specs()[key].id)->load(std::memory_order_relaxed); }
    void updateControls()
    {
        using namespace parameters;
        const bool newLayered = raw(mode) >= 0.5f;
        bool changed = newLayered != layered;
        layered = newLayered;
        controls[layeredChannel]->setEnabled(layered);
        target.setEnabled(!layered);
        for (int v = 0; v < 3; ++v)
        {
            bool isManual = raw(voice(v, source)) >= 0.5f;
            changed |= manual[static_cast<std::size_t>(v)] != isManual;
            manual[static_cast<std::size_t>(v)] = isManual;
            controls[channel1 + v]->setEnabled(!layered);
            controls[voice(v, tuning)]->setVisible(!isManual);
            controls[voice(v, frequency)]->setVisible(isManual);
            controls[voice(v, manualGate)]->setEnabled(isManual);
        }
        if (changed || epoch != processor.auditionEpoch()) releaseKeys();
        gates = processor.liveGates();
        repaint();
    }
    void timerCallback() override
    {
        if (resetFlash > 0 && --resetFlash == 0) panic.setButtonText("Panic / reset");
        updateControls();
    }
    void releaseKeys()
    {
        orderSize = 0; epoch = processor.auditionEpoch();
        keyboardState.allNotesOff(1);
        processor.audition(-1, 0, epoch);
    }
    void handleNoteOn(juce::MidiKeyboardState*, int, int note, float) override
    {
        if (epoch != processor.auditionEpoch()) { orderSize = 0; epoch = processor.auditionEpoch(); }
        remove(note); heldOrder[static_cast<std::size_t>(orderSize++)] = note; sendAudition();
    }
    void remove(int note)
    {
        for (int i = 0; i < orderSize; ++i)
            if (heldOrder[static_cast<std::size_t>(i)] == note)
            { for (int j = i + 1; j < orderSize; ++j) heldOrder[static_cast<std::size_t>(j - 1)] = heldOrder[static_cast<std::size_t>(j)]; --orderSize; break; }
    }
    void handleNoteOff(juce::MidiKeyboardState*, int, int note, float) override
    {
        if (epoch != processor.auditionEpoch()) { orderSize = 0; epoch = processor.auditionEpoch(); }
        remove(note); sendAudition();
    }
    void sendAudition() { processor.audition(orderSize ? heldOrder[static_cast<std::size_t>(orderSize - 1)] : -1, layered ? 7u : 1u << targetVoice, epoch); }
    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey) { releaseKeys(); hardware.setVisible(false); return true; }
        return false;
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(paper);
        text(g, "SID", 24, 15, 65, 46, 30, ink, true);
        text(g, "THREE VOICES. ONE CHIP.", 89, 27, 190, 16, 10, muted, true);
        g.setColour(line); g.drawHorizontalLine(76, 24, 1096);
        text(g, "CLOCK", 24, 88, 50, 26, 10, muted, true);
        text(g, layered ? "LAYERED / ONE INPUT" : "3 PARTS / INDEPENDENT CHANNELS", 378, 88, 380, 26, 11, green, true);
        text(g, "1 SID / 3 OSCILLATORS / 1 FILTER", 764, 89, 198, 24, 10, muted, true);
        for (int v = 0; v < 3; ++v)
        {
            const int y = 130 + v * 177;
            g.setColour(panel); g.fillRoundedRectangle(24, static_cast<float>(y), 736, 161, 7);
            g.setColour(line); g.drawRoundedRectangle(24.5f, static_cast<float>(y) + .5f, 735, 160, 7, 1);
            g.setColour(green); g.fillRoundedRectangle(25, static_cast<float>(y) + 1, 4, 159, 2);
            text(g, juce::String(v + 1), 44, y + 16, 25, 38, 27, green, true);
            text(g, "VOICE " + juce::String(v + 1), 75, y + 14, 100, 20, 12, ink, true);
            g.setColour(line); g.drawHorizontalLine(y + 75, 44, 740);
            const bool gate = (gates & (1u << v)) != 0;
            g.setColour(gate ? green : line); g.fillEllipse(550, static_cast<float>(y + 135), 6, 6);
            text(g, gate ? "GATE ON" : "GATE OFF", 563, y + 128, 78, 20, 10, gate ? green : muted, true);
            const bool routed = raw(parameters::route1 + v) >= .5f;
            text(g, routed ? "To filter: ON" : "To filter: OFF", 648, y + 128, 96, 20, 10, routed ? green : muted);
        }
        g.setColour(panel); g.fillRoundedRectangle(784, 130, 312, 515, 7);
        g.setColour(line); g.drawRoundedRectangle(784.5f, 130.5f, 311, 514, 7, 1);
        text(g, "ONE SHARED FILTER", 806, 146, 268, 26, 13, ink, true);
        text(g, "INPUT ROUTING / INDEPENDENT SWITCHES", 806, 177, 280, 18, 10);
        text(g, "FILTER MODE / COMBINATIONS ALLOWED", 806, 417, 280, 18, 10);
        text(g, "Cutoff response depends on the SID model.", 806, 502, 272, 20);
        text(g, "LP, BP and HP can be combined.", 806, 522, 272, 20);
        g.setColour(line); g.drawHorizontalLine(572, 806, 1074);
        g.setColour(panel); g.fillRoundedRectangle(24, 668, 1072, 86, 7);
        g.setColour(line); g.drawRoundedRectangle(24.5f, 668.5f, 1071, 85, 7, 1);
        text(g, layered ? "AUDITION / ALL MIDI VOICES" : "AUDITION TARGET", 42, 681, 178, 18, 10, muted, true);
        text(g, "TEST / EXT IN", 832, 716, 145, 18, 10);
        text(g, "Drag corner to resize", 965, 734, 125, 15, 9);
    }
};

PluginEditor::PluginEditor(PluginProcessor& processor) : AudioProcessorEditor(processor)
{
    rack = std::make_unique<Rack>(processor, *this);
    addAndMakeVisible(*rack);
    setResizable(true, true);
    setResizeLimits(896, 624, 1680, 1170);
    getConstrainer()->setFixedAspectRatio(1120.0 / 780.0);
    setSize(1120, 780);
}
PluginEditor::~PluginEditor() = default;
void PluginEditor::paint(juce::Graphics& g) { g.fillAll(paper); }
void PluginEditor::resized()
{
    if (!rack) return;
    const float scale = juce::jmin(static_cast<float>(getWidth()) / 1120.0f, static_cast<float>(getHeight()) / 780.0f);
    rack->zoom.setText(juce::String(juce::roundToInt(scale * 100)) + "%", juce::dontSendNotification);
    rack->setBounds(0, 0, 1120, 780);
    rack->setTransform(juce::AffineTransform::scale(scale));
}
}
