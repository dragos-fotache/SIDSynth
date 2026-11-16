// SPDX-License-Identifier: AGPL-3.0-only
// Included inside OfflineTests.cpp's test namespace.
std::vector<float> noiseRecoveryRender(int rate, int block, bool model8580, int voice)
{
    Processor p;
    using namespace sidsynth::parameters;
    auto set = [&](std::size_t key, float value) { setParameter(p, specs()[key].id.toRawUTF8(), value); };
    set(gain, 0); set(model, model8580 ? 1.0f : 0.0f);
    set(voice3Direct, 1); set(lowPass, 1); set(cutoff, 280);
    set(sidsynth::parameters::voice(voice, noise), 1); // Saw is initially on.
    set(sidsynth::parameters::voice(voice, sustain), 15);
    set(sidsynth::parameters::voice(voice, release), 0);
    // Exercise Manual as well as MIDI ownership without rearming either one
    // when the stuck generator recovers.
    const bool manual = voice == 1;
    if (manual)
    {
        set(sidsynth::parameters::voice(voice, source), 1);
        set(sidsynth::parameters::voice(voice, manualGate), 1);
    }
    p.prepareToPlay(rate, block);
    // Preparation disarms saved Manual Hold; an explicit edge starts it.
    if (manual) { set(sidsynth::parameters::voice(voice, manualGate), 0); }
    juce::AudioBuffer<float> audio(2, block);
    juce::MidiBuffer midi;
    if (manual) { process(p, audio, midi); set(sidsynth::parameters::voice(voice, manualGate), 1); }
    constexpr int segment = 8192;
    std::vector<float> result(segment * 3);
    for (int cursor = 0; cursor < static_cast<int>(result.size());)
    {
        if (cursor == segment) set(sidsynth::parameters::voice(voice, saw), 0);
        if (cursor == segment * 2) set(route1 + voice, 1);
        const int count = std::min(block, segment - cursor % segment);
        audio.setSize(2, count);
        if (cursor == 0 && !manual)
            midi.addEvent(MidiMessage::noteOn(voice + 1, 69, (juce::uint8)100), 13);
        process(p, audio, midi);
        std::copy_n(audio.getReadPointer(0), count, result.begin() + cursor);
        require(p.liveGates() == (1u << voice), "Noise recovery changed voice gate ownership");
        if (!manual) require(p.sidEngine().activeNote(voice) == 69, "Noise recovery lost the held MIDI note");
        cursor += count;
    }
    require(rms(result, segment + segment / 2, segment * 2) > .003, "Noise alone remained latched silent after deselecting Saw");
    require(rms(result, segment * 2 + segment / 2, segment * 3) > .003, "Recovered noise did not pass through the filter");
    return result;
}

void noiseRecoveryTests()
{
    for (int rate : {44100, 48000, 96000})
        for (bool model8580 : {false, true})
            for (int voice = 0; voice < 3; ++voice)
            {
                const auto reference = noiseRecoveryRender(rate, 257, model8580, voice);
                for (int block : {64, 1024})
                    require(reference == noiseRecoveryRender(rate, block, model8580, voice),
                        "Noise recovery depends on callback size");
            }

    // Recover an inaudible Voice 3 while Voice 1 uses it for sync and ring.
    // Compare against unmodified upstream: its noise stays locked, but the
    // audible phase/envelope/filter history must remain exactly the same.
    for (bool model8580 : {false, true})
        for (unsigned char combination : {0x80, 0x90, 0xa0, 0xc0, 0xf0})
        {
            sidsynth::SidBackend cached;
            reSIDfp::residfp original;
            cached.prepare(48000); cached.configure(model8580, false);
            original.noiseLevel(1);
            original.setChipModel(model8580 ? reSIDfp::CSG8580 : reSIDfp::MOS6581);
            original.setSamplingParameters(985248, reSIDfp::RESAMPLE, 48000);
            cached.reset(); original.reset();
            auto write = [&](int address, unsigned char value) { cached.write(address, value); original.write(address, value); };
            for (int voice : {0, 2})
            {
                write(voice * 7, 71); write(voice * 7 + 1, static_cast<unsigned char>(voice == 0 ? 29 : 43));
                write(voice * 7 + 3, 8); write(voice * 7 + 5, 0); write(voice * 7 + 6, 0xf0);
            }
            write(4, 0x17); // Triangle, sync, ring, gate.
            write(18, static_cast<unsigned char>(combination | 1));
            write(0x15, 3); write(0x16, 160); write(0x17, 1);
            // Healthy noise is audible so an unnecessary reseed cannot hide
            // behind Voice 3 direct-off in this reference comparison.
            write(0x18, combination == 0x80 ? 0x1f : 0x9f);
            std::array<short, 8192> actual{}, expected{};
            cached.clock(actual.data(), static_cast<int>(actual.size()));
            original.clock(expected.data(), static_cast<int>(expected.size()));
            require(actual == expected, "Combined noise changed before recovery");
            // Also cover returning through another waveform and through None.
            write(18, 0x21); write(18, 1); write(18, 0x81);
            cached.clock(actual.data(), static_cast<int>(actual.size()));
            original.clock(expected.data(), static_cast<int>(expected.size()));
            require(actual == expected, "Noise recovery reset another oscillator, its modulator phase, envelope or shared filter");
            write(18, 0x89); // Explicit TEST continues to hold the oscillator.
            cached.clock(actual.data(), 64); original.clock(expected.data(), 64);
            require(std::equal(actual.begin(), actual.begin() + 64, expected.begin()), "Noise recovery overrode explicit TEST");
        }
    std::cout << "PASS: noise recovery, MIDI/Manual ownership, filter, block consistency and shared-chip isolation\n";
}
