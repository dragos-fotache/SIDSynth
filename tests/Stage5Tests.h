// SPDX-License-Identifier: AGPL-3.0-only
// Included inside OfflineTests.cpp's test namespace.
void stage5Tests()
{
    // Hosts must allow the slow hardware release to continue beyond one second.
    Processor longRelease;
    setParameter(longRelease, "voice1.sustain", 15);
    setParameter(longRelease, "voice1.release", 15);
    longRelease.prepareToPlay(48000, 1024);
    juce::AudioBuffer<float> tail(2, 1024);
    juce::MidiBuffer tailMidi;
    tailMidi.addEvent(MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
    process(longRelease, tail, tailMidi);
    for (int n = 0; n < 10; ++n) process(longRelease, tail, tailMidi);
    tailMidi.addEvent(MidiMessage::noteOff(1, 69), 0);
    for (int n = 0; n < 94; ++n) process(longRelease, tail, tailMidi);
    require(tail.getMagnitude(0, 1024) > .001f && longRelease.liveGates() == 0,
        "Slow SID release was cut after note-off");
    require(longRelease.getTailLengthSeconds() >= 25, "Host tail declaration can truncate the slow SID release");

    // Variable callbacks (including empty callbacks) must not introduce clicks,
    // lose events at boundaries, or alter the chip's fractional clock history.
    const std::vector<Event> events{
        {0, MidiMessage::noteOn(1, 69, (juce::uint8)100)},
        {1, MidiMessage::noteOn(2, 60, (juce::uint8)100)},
        {64, MidiMessage::noteOn(3, 64, (juce::uint8)100)},
        {1023, MidiMessage::pitchWheel(2, 16000)},
        {4097, MidiMessage::noteOff(1, 69)},
        {7001, MidiMessage::noteOn(1, 72, (juce::uint8)100)},
        {12000, MidiMessage::allSoundOff(2)},
        {18001, MidiMessage::noteOff(3, 64)}};
    for (int rate : {44100, 48000, 96000})
    {
        const auto reference = render(rate, 257, 24000, events);
        Processor processor;
        processor.prepareToPlay(rate, 1024);
        std::vector<float> actual(reference.size());
        constexpr int sizes[]{1, 64, 0, 257, 31, 1024, 3, 128};
        int cursor = 0, callback = 0;
        while (cursor < static_cast<int>(actual.size()))
        {
            const int count = std::min(sizes[callback++ % 8], static_cast<int>(actual.size()) - cursor);
            juce::AudioBuffer<float> audio(2, count);
            juce::MidiBuffer midi;
            for (const auto& event : events)
                if (event.sample >= cursor && event.sample < cursor + count)
                    midi.addEvent(event.message, event.sample - cursor);
            process(processor, audio, midi);
            std::copy_n(audio.getReadPointer(0), count, actual.begin() + cursor);
            cursor += count;
        }
        require(actual == reference, "Variable/empty callbacks changed the event timeline or introduced a discontinuity");
    }

    // Host rate/buffer reconfiguration must clear MIDI, pedal, bend and preview
    // ownership. Fresh input must recover at concert pitch after every change.
    Processor reused;
    juce::MemoryBlock patch;
    reused.getStateInformation(patch);
    for (int rate : {48000, 96000, 44100, 48000})
    {
        reused.releaseResources();
        reused.prepareToPlay(rate, 257);
        juce::AudioBuffer<float> audio(2, 257);
        juce::MidiBuffer midi;
        process(reused, audio, midi);
        require(audio.getMagnitude(0, 257) == 0 && reused.liveGates() == 0,
            "Rate change resurrected performance state");
        std::vector<float> note(static_cast<std::size_t>(rate));
        for (int cursor = 0; cursor < rate; cursor += 257)
        {
            const int count = std::min(257, rate - cursor);
            audio.setSize(2, count);
            if (cursor == 0) midi.addEvent(MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
            process(reused, audio, midi);
            std::copy_n(audio.getReadPointer(0), count, note.begin() + cursor);
        }
        require(std::abs(pitch(note, rate) - 440) < 1, "Reconfigured processor lost concert pitch");
        midi.addEvent(MidiMessage::controllerEvent(1, 64, 127), 0);
        midi.addEvent(MidiMessage::pitchWheel(1, 16383), 1);
        reused.audition(60, 2, reused.auditionEpoch());
        process(reused, audio, midi);
        reused.setStateInformation(patch.getData(), static_cast<int>(patch.getSize()));
        process(reused, audio, midi);
        require(audio.getMagnitude(0, audio.getNumSamples()) == 0, "Recall during playback did not silence performance");
    }

    // Interleave a second core's MIDI, resets, model/clock and state changes.
    // It must neither affect the first core nor share its performance history.
    Processor isolated, neighbour;
    isolated.prepareToPlay(48000, 257);
    neighbour.prepareToPlay(96000, 64);
    juce::AudioBuffer<float> a(2, 257), b(2, 64);
    juce::MidiBuffer ma, mb;
    const auto reference = render(48000, 257, 257 * 80, {{0, MidiMessage::noteOn(1, 69, (juce::uint8)100)}});
    for (int block = 0; block < 80; ++block)
    {
        setParameter(neighbour, "chip.model", static_cast<float>(block % 2));
        setParameter(neighbour, "chip.clock", static_cast<float>((block / 2) % 2));
        if (block % 3 == 0) neighbour.requestPanic();
        mb.addEvent(MidiMessage::noteOn(2, 40 + block % 30, (juce::uint8)100), 7);
        process(neighbour, b, mb);
        if (block == 0) ma.addEvent(MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
        process(isolated, a, ma);
        require(std::equal(a.getReadPointer(0), a.getReadPointer(0) + 257, reference.begin() + block * 257),
            "Another plugin instance changed the first chip's samples");
    }
    std::cout << "PASS: variable/empty callbacks, live rate changes, recall and multiple-core isolation\n";
}
