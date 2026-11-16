// SPDX-License-Identifier: AGPL-3.0-only
// Included inside OfflineTests.cpp's test namespace so the same callback
// allocation guard and audio validators cover all Stage 3 behavior.
void stage3RegisterTests()
{
    using namespace sidsynth::parameters;
    Processor p;
    p.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer midi;
    auto set = [&](std::size_t key, float v) { setParameter(p, specs()[key].id.toRawUTF8(), v); };
    for (int v = 0; v < 3; ++v)
    {
        set(voice(v, triangle), 1); set(voice(v, pulse), 1); set(voice(v, noise), 1);
        set(voice(v, sidsynth::parameters::sync), 1); set(voice(v, ring), 1); set(voice(v, test), 1);
        set(voice(v, pulseWidth), 4095);
        set(voice(v, attack), 15); set(voice(v, decay), 14);
        set(voice(v, sustain), 13); set(voice(v, release), 12);
    }
    set(cutoff, 2047); set(resonance, 15);
    set(route1, 1); set(route2, 1); set(route3, 1); set(routeExternal, 1);
    set(lowPass, 1); set(bandPass, 1); set(highPass, 1); set(voice3Direct, 0); set(volume, 9);
    for (int v = 0; v < 3; ++v) midi.addEvent(MidiMessage::noteOn(v + 1, 69, (juce::uint8)100), 17);
    process(p, audio, midi);
    const auto& sid = p.sidEngine();
    for (int v = 0; v < 3; ++v)
    {
        require(sid.registerValue(v * 7 + 4) == 255, "Independent waveform/control bits did not combine");
        require(sid.registerValue(v * 7 + 2) == 255 && sid.registerValue(v * 7 + 3) == 15, "Pulse width lost 12-bit resolution");
        require(sid.registerValue(v * 7 + 5) == 0xfe && sid.registerValue(v * 7 + 6) == 0xdc, "ADSR nibbles mapped incorrectly");
    }
    require(sid.registerValue(0x15) == 7 && sid.registerValue(0x16) == 255, "Cutoff lost 11-bit resolution");
    require(sid.registerValue(0x17) == 255 && sid.registerValue(0x18) == 0xf9, "Shared filter modes/routing/volume are wrong");
    set(cutoff, 8); set(resonance, 0); set(route2, 0); set(bandPass, 0); set(voice3Direct, 1); set(volume, 0);
    set(voice(0, pulseWidth), 0); set(voice(0, noise), 0); set(voice(0, test), 0);
    process(p, audio, midi);
    require(sid.registerValue(0x15) == 0 && sid.registerValue(0x16) == 1, "Cutoff low/high register split is wrong");
    require(sid.registerValue(0x17) == 13 && sid.registerValue(0x18) == 0x50, "Independent filter toggles damaged other bits");
    require(sid.registerValue(4) == 0x77 && sid.registerValue(2) == 0 && sid.registerValue(3) == 0, "Independent oscillator toggles damaged other bits");
    set(volume, 15);
    set(voice(0, test), 1); set(voice(1, test), 1); set(voice(2, test), 1);
    // Repeated live model/clock automation must neither allocate nor keep notes.
    for (int i = 0; i < 8; ++i)
    {
        set(model, static_cast<float>(i & 1)); set(sidsynth::parameters::clock, static_cast<float>((i / 2) & 1));
        process(p, audio, midi);
        require(audio.getMagnitude(0, 256) == 0 && sid.activeNote(0) == -1, "Chip configuration change kept a note");
        set(voice(0, test), 0);
        midi.addEvent(MidiMessage::noteOn(1, 69, (juce::uint8)100), 31);
        process(p, audio, midi);
        require(std::abs(sid.frequencyRegister(0) * sid.clockFrequency() / 16777216.0 - 440.0) < 0.04,
            "MIDI tuning did not follow the selected clock");
    }
}

void stage3OwnershipTests()
{
    using namespace sidsynth::parameters;
    Processor p;
    p.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    auto set = [&](std::size_t key, float v) { setParameter(p, specs()[key].id.toRawUTF8(), v); };
    const auto& sid = p.sidEngine();
    set(voice(0, frequency), 12345); set(voice(0, manualGate), 1);
    process(p, audio, midi);
    require(audio.getMagnitude(0, 512) == 0 && (sid.registerValue(4) & 1) == 0, "Manual controls competed with MIDI source");
    set(voice(0, source), 1);
    process(p, audio, midi);
    require(sid.frequencyRegister(0) == 12345 && (sid.registerValue(4) & 1) != 0 && audio.getMagnitude(0, 512) > 0,
        "Selecting manual source did not activate its explicit hold");
    midi.addEvent(MidiMessage::noteOn(1, 20, (juce::uint8)100), 113);
    midi.addEvent(MidiMessage::pitchWheel(1, 16383), 217);
    midi.addEvent(MidiMessage::noteOff(1, 20), 319);
    set(voice(0, tuning), 36);
    process(p, audio, midi);
    require(sid.frequencyRegister(0) == 12345 && (sid.registerValue(4) & 1) != 0, "MIDI/tuning changed manual frequency or gate");
    set(voice(0, frequency), 65535); process(p, audio, midi);
    require(sid.frequencyRegister(0) == 65535, "Maximum manual frequency wrapped");
    set(voice(0, frequency), 0); process(p, audio, midi);
    require(sid.frequencyRegister(0) == 0 && (sid.registerValue(4) & 1) != 0, "Zero manual frequency cleared explicit gate");
    p.requestPanic(); process(p, audio, midi);
    process(p, audio, midi);
    require(audio.getMagnitude(0, 512) == 0 && (sid.registerValue(4) & 1) == 0, "Panic rearmed a saved manual hold");
    set(voice(0, pulseWidth), 2000); process(p, audio, midi);
    require(audio.getMagnitude(0, 512) == 0, "Unrelated parameter rearmed manual hold after panic");
    set(voice(0, manualGate), 0); process(p, audio, midi);
    set(voice(0, frequency), 7493); set(voice(0, manualGate), 1); process(p, audio, midi);
    require((sid.registerValue(4) & 1) != 0, "Manual hold did not rearm after an explicit edge");
    juce::MemoryBlock saved;
    p.getStateInformation(saved);
    p.setStateInformation(saved.getData(), static_cast<int>(saved.getSize())); process(p, audio, midi);
    require(audio.getMagnitude(0, 512) == 0 && (sid.registerValue(4) & 1) == 0, "State restore restarted a manual hold");
    set(voice(0, source), 0); set(voice(0, tuning), 0); process(p, audio, midi);
    require(audio.getMagnitude(0, 512) == 0, "Source transition resurrected old MIDI ownership");
    midi.addEvent(MidiMessage::noteOn(1, 69, (juce::uint8)100), 113); process(p, audio, midi);
    require(sid.activeNote(0) == 69 && std::abs(sid.frequencyRegister(0) * sid.clockFrequency() / 16777216.0 - 440) < 0.04,
        "Source transition retained old bend or failed to accept a fresh note");
    set(voice(0, tuning), 12); process(p, audio, midi);
    require(std::abs(sid.frequencyRegister(0) * sid.clockFrequency() / 16777216.0 - 880) < 0.04
        && (sid.registerValue(4) & 1) != 0, "Live tuning failed or released the MIDI gate");
}

void stage3StateTests()
{
    using namespace sidsynth::parameters;
    Processor original, restored;
    std::array<float, count> expected{};
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& spec = specs()[i];
        const float v = spec.minimum + (spec.maximum - spec.minimum) * 0.71f;
        setParameter(original, spec.id.toRawUTF8(), v);
        expected[i] = original.state.getRawParameterValue(spec.id)->load();
    }
    juce::MemoryBlock saved;
    original.getStateInformation(saved);
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    for (std::size_t i = 0; i < count; ++i)
        require(std::abs(restored.state.getRawParameterValue(specs()[i].id)->load() - expected[i]) < 0.001f,
            "Complete patch failed state round trip");
    auto xml = juce::AudioProcessor::getXmlFromBinary(saved.getData(), static_cast<int>(saved.getSize()));
    require(xml != nullptr, "Saved patch envelope is invalid");
    xml->setAttribute("voice3.release", "nan");
    xml->setAttribute(gainId, -1.0);
    auto invalid = stateXml(*xml);
    restored.setStateInformation(invalid.getData(), static_cast<int>(invalid.getSize()));
    require(restored.state.getRawParameterValue(gainId)->load() == expected[gain], "Invalid late patch field partially applied state");
    xml->setAttribute("schemaVersion", 2);
    auto old = stateXml(*xml); // Stage 2 ignores all Stage 3 fields, including nan.
    restored.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
    for (std::size_t i = voiceStart; i < count; ++i)
        require(restored.state.getRawParameterValue(specs()[i].id)->load() == specs()[i].initial, "Schema 2 did not default SID controls");
    require(restored.state.getRawParameterValue("chip.model")->load() == 1, "Legacy patch changed SID model");
    xml->setAttribute("schemaVersion", 3); xml->setAttribute("voice3.release", 999);
    xml->setAttribute("voice1.frequency", -123); xml->setAttribute("filter.cutoff", 99999);
    auto clamped = stateXml(*xml);
    restored.setStateInformation(clamped.getData(), static_cast<int>(clamped.getSize()));
    require(restored.state.getRawParameterValue("voice3.release")->load() == 15
        && restored.state.getRawParameterValue("voice1.frequency")->load() == 0
        && restored.state.getRawParameterValue("filter.cutoff")->load() == 2047, "Hardware ranges were not clamped on restore");
}

std::vector<float> stage3Render(int rate, int block, bool ntsc, bool model8580, bool external)
{
    using namespace sidsynth::parameters;
    Processor p;
    if (external)
    {
        auto layout = p.getBusesLayout();
        layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
        require(p.setBusesLayout(layout), "Optional stereo input layout was rejected");
    }
    setParameter(p, "chip.clock", ntsc ? 1.0f : 0.0f);
    setParameter(p, "chip.model", model8580 ? 1.0f : 0.0f);
    setParameter(p, "filter.voice1", 1); setParameter(p, "filter.voice2", 1);
    setParameter(p, "filter.external", 1); setParameter(p, "filter.lowPass", 1);
    setParameter(p, "filter.cutoff", 1100); setParameter(p, "filter.resonance", 8);
    setParameter(p, "voice1.sync", 1); setParameter(p, "voice2.saw", 0);
    setParameter(p, "voice2.triangle", 1); setParameter(p, "voice2.ring", 1);
    setParameter(p, "voice3.source", 1); setParameter(p, "voice3.frequency", 9876);
    p.prepareToPlay(rate, block);
    const int end = rate / 3;
    std::vector<float> result(static_cast<std::size_t>(end));
    const std::vector<Event> events{{113, MidiMessage::noteOn(1, 69, (juce::uint8)100)},
        {197, MidiMessage::noteOn(2, 57, (juce::uint8)100)}, {rate / 7, MidiMessage::pitchWheel(1, 12345)},
        {rate / 5, MidiMessage::noteOff(1, 69)}};
    // Parameter automation is applied at these exact absolute boundaries;
    // split host blocks there to test the documented boundary-based interface.
    const int automationAt = rate / 6;
    for (int cursor = 0; cursor < end;)
    {
        if (cursor == automationAt) { setParameter(p, "filter.cutoff", 700); setParameter(p, "voice1.pulse", 1); }
        const int n = std::min({block, end - cursor, cursor < automationAt ? automationAt - cursor : end - cursor});
        juce::AudioBuffer<float> audio(2, n);
        audio.clear();
        if (external)
            for (int i = 0; i < n; ++i)
            {
                const float sample = static_cast<float>(0.1 * std::sin(2 * 3.141592653589793 * 320 * (cursor + i) / rate));
                audio.setSample(0, i, sample); audio.setSample(1, i, sample * 0.5f);
            }
        juce::MidiBuffer midi;
        for (const auto& e : events) if (e.sample >= cursor && e.sample < cursor + n) midi.addEvent(e.message, e.sample - cursor);
        process(p, audio, midi);
        std::copy_n(audio.getReadPointer(0), n, result.begin() + cursor);
        cursor += n;
    }
    return result;
}

void stage3AudioTests(const char* auditionPath)
{
    for (const int rate : {44100, 48000, 96000})
        for (const bool ntsc : {false, true})
            for (const bool model8580 : {false, true})
            {
                const auto reference = stage3Render(rate, 257, ntsc, model8580, false);
                require(rms(reference, rate / 10, rate / 4) > 0.0001, "Filtered sync/ring patch was silent");
                for (const int block : {64, 1024})
                    require(reference == stage3Render(rate, block, ntsc, model8580, false), "SID controls/automation depend on block size");
            }
    const auto external = stage3Render(48000, 257, false, true, true);
    require(external == stage3Render(48000, 64, false, true, true)
        && external == stage3Render(48000, 1024, false, true, true), "External input depends on blocks or MIDI segmentation");
    if (auditionPath)
    {
        auto file = juce::File(juce::String(auditionPath)).getSiblingFile("stage3-filter-sync-ring.wav");
        const auto data = stage3Render(48000, 257, false, true, false);
        writeWav(file.getFullPathName().toRawUTF8(), data, 48000);
    }
    // Input-only signal must pass through the actual shared SID signal path.
    Processor p;
    auto layout = p.getBusesLayout();
    layout.inputBuses.set(0, juce::AudioChannelSet::mono());
    layout.outputBuses.set(0, juce::AudioChannelSet::mono());
    require(p.setBusesLayout(layout), "Mono EXT IN/output layout rejected");
    p.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> audio(1, 512); juce::MidiBuffer midi;
    for (int k = 0; k < 4; ++k)
    {
        for (int i = 0; i < 512; ++i) audio.setSample(0, i, static_cast<float>(0.2 * std::sin(2 * 3.141592653589793 * 440 * (k * 512 + i) / 48000)));
        process(p, audio, midi);
    }
    require(audio.getMagnitude(0, 512) > 0.001, "External input without a MIDI note is silent");
    setParameter(p, "chip.volume", 0);
    for (int k = 0; k < 50; ++k) { audio.clear(); process(p, audio, midi); }
    require(audio.getMagnitude(0, 512) < 0.0002, "SID volume zero did not suppress external input");
}

void stage3BackendTests()
{
    // The cached path must produce exactly the same SID samples as upstream's
    // allocating public setters for every model/clock combination and switches.
    sidsynth::SidBackend cached;
    reSIDfp::residfp original;
    cached.prepare(48000);
    original.noiseLevel(1);
    std::array<short, 4096> actual{}, expected{};
    for (int configuration = 0; configuration < 8; ++configuration)
    {
        const bool model8580 = (configuration & 1) != 0, ntsc = (configuration & 2) != 0;
        cached.configure(model8580, ntsc);
        original.setChipModel(model8580 ? reSIDfp::CSG8580 : reSIDfp::MOS6581);
        original.setSamplingParameters(ntsc ? 1022730.0 : 985248.0, reSIDfp::RESAMPLE, 48000);
        cached.reset(); original.reset(); original.input(0);
        auto write = [&](int address, unsigned char value) { cached.write(address, value); original.write(address, value); };
        for (int v = 0; v < 3; ++v)
        {
            write(v * 7, static_cast<unsigned char>(71 + v * 20)); write(v * 7 + 1, static_cast<unsigned char>(20 + v * 7));
            write(v * 7 + 2, 37); write(v * 7 + 3, 8); write(v * 7 + 5, 0x04); write(v * 7 + 6, 0xc5);
            write(v * 7 + 4, static_cast<unsigned char>(v == 0 ? 0x33 : (v == 1 ? 0x15 : 0x41)));
        }
        write(0x15, 3); write(0x16, 160); write(0x17, 0x97); write(0x18, 0x7f);
        cached.clock(actual.data(), static_cast<int>(actual.size()));
        original.clock(expected.data(), static_cast<int>(expected.size()));
        require(actual == expected, "Cached realtime configuration differs from pinned upstream audio");
    }
}

void stage3InteractionTests()
{
    auto sound = [](bool syncOn, bool ringOn, bool filterVoice3, bool direct3, bool play3)
    {
        Processor p;
        setParameter(p, "voice1.saw", ringOn ? 0.0f : 1.0f);
        setParameter(p, "voice1.triangle", ringOn ? 1.0f : 0.0f);
        setParameter(p, "voice1.sync", syncOn ? 1.0f : 0.0f);
        setParameter(p, "voice1.ring", ringOn ? 1.0f : 0.0f);
        setParameter(p, "chip.voice3Direct", direct3 ? 1.0f : 0.0f);
        setParameter(p, "filter.voice3", filterVoice3 ? 1.0f : 0.0f);
        setParameter(p, "filter.lowPass", filterVoice3 ? 1.0f : 0.0f);
        p.prepareToPlay(48000, 512);
        std::vector<float> result(8192);
        for (int cursor = 0; cursor < 8192; cursor += 512)
        {
            juce::AudioBuffer<float> audio(2, 512); juce::MidiBuffer midi;
            if (cursor == 0)
            {
                midi.addEvent(MidiMessage::noteOn(1, 69, (juce::uint8)100), 13);
                if (play3) midi.addEvent(MidiMessage::noteOn(3, 62, (juce::uint8)100), 13);
            }
            process(p, audio, midi);
            std::copy_n(audio.getReadPointer(0), 512, result.begin() + cursor);
        }
        return result;
    };
    const auto solo = sound(false, false, false, false, false);
    require(solo == sound(false, false, false, false, true), "Voice 3 direct-off leaked its unfiltered output");
    const auto synced = sound(true, false, false, false, true);
    require(synced != sound(true, false, false, false, false), "Voice 3 could not sync voice 1 with direct output off");
    require(sound(false, true, false, false, true) != sound(false, true, false, false, false),
        "Voice 3 could not ring-modulate voice 1 with direct output off");
    require(sound(false, false, true, false, true) != solo, "Voice 3 direct-off incorrectly removed its filtered path");
}

void stage3UiPanicTest()
{
    Processor p;
    p.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> audio(2, 512); juce::MidiBuffer midi;
    midi.addEvent(MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
    process(p, audio, midi);
    require(audio.getMagnitude(0, 512) > 0, "UI Panic setup is silent");
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    bool clicked = false;
    std::function<void(juce::Component&)> click = [&](juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* button = dynamic_cast<juce::TextButton*>(child))
                if (button->getButtonText() == "Panic / reset") { button->onClick(); clicked = true; }
            click(*child);
        }
    };
    click(*editor);
    require(clicked, "Rack editor Panic action is not connected");
    process(p, audio, midi);
    require(audio.getMagnitude(0, 512) == 0, "Rack editor Panic action did not silence the processor");
}
