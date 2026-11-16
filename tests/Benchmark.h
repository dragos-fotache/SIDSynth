// SPDX-License-Identifier: AGPL-3.0-only
// Release-only measurements: call the real callback without test guards or GUI.
int benchmark()
{
    using Clock = std::chrono::steady_clock;
    std::cout << "sample_rate,block,scenario,audio_seconds,cpu_seconds,cpu_percent_one_core,wall_p50_us,wall_p99_us,wall_max_us\n";
    for (int rate : {44100, 48000, 96000})
        for (int block : {64, 257, 1024})
            for (int scenario = 0; scenario < 5; ++scenario)
            {
                Processor processor;
                if (scenario == 3)
                {
                    auto layout = processor.getBusesLayout();
                    layout.inputBuses.set(0, juce::AudioChannelSet::stereo());
                    require(processor.setBusesLayout(layout), "Benchmark EXT IN layout rejected");
                }
                setParameter(processor, "chip.model", scenario == 1 ? 0.0f : 1.0f);
                setParameter(processor, "filter.voice1", 1);
                setParameter(processor, "filter.voice2", 1);
                setParameter(processor, "filter.voice3", 1);
                setParameter(processor, "filter.external", 1);
                setParameter(processor, "filter.lowPass", 1);
                setParameter(processor, "filter.resonance", 12);
                setParameter(processor, "filter.cutoff", 950);
                setParameter(processor, "voice1.sync", 1);
                setParameter(processor, "voice2.ring", 1);
                setParameter(processor, "voice2.triangle", 1);
                processor.prepareToPlay(rate, block);
                juce::AudioBuffer<float> audio(2, block);
                juce::MidiBuffer midi;
                if (scenario != 0)
                    for (int v = 0; v < 3; ++v)
                        midi.addEvent(MidiMessage::noteOn(v + 1, 48 + v * 7, (juce::uint8)100), 0);
                processor.processBlock(audio, midi);
                if (scenario == 4)
                {
                    for (int v = 0; v < 3; ++v)
                        midi.addEvent(MidiMessage::noteOff(v + 1, 48 + v * 7), 0);
                    for (int n = 0; n < rate / block; ++n) processor.processBlock(audio, midi);
                }
                const int callbacks = rate * 3 / block;
                std::vector<double> durations;
                durations.reserve(static_cast<std::size_t>(callbacks));
                double cpu = 0;
                for (int i = -100; i < callbacks; ++i)
                {
                    audio.clear();
                    if (scenario == 3)
                        for (int c = 0; c < 2; ++c)
                            for (int s = 0; s < block; ++s) audio.setSample(c, s, .1f);
                    const auto cpuStart = std::clock();
                    const auto start = Clock::now();
                    processor.processBlock(audio, midi);
                    const auto elapsed = std::chrono::duration<double, std::micro>(Clock::now() - start).count();
                    const auto cpuElapsed = static_cast<double>(std::clock() - cpuStart) / CLOCKS_PER_SEC;
                    if (i >= 0) { durations.push_back(elapsed); cpu += cpuElapsed; }
                }
                std::sort(durations.begin(), durations.end());
                const double seconds = static_cast<double>(callbacks * block) / rate;
                std::cout << rate << ',' << block << ',' << std::array<const char*, 5>{"fresh-idle", "6581", "8580", "ext-in", "released"}[scenario]
                    << ',' << seconds << ',' << cpu << ',' << cpu / seconds * 100
                    << ',' << durations[durations.size() / 2] << ',' << durations[durations.size() * 99 / 100] << ',' << durations.back() << '\n';
            }
    return 0;
}
