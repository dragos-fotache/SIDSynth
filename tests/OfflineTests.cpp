// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
#include "SidBackend.h"
#include <residfp/residfp.h>
#include <pthread.h>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <stdexcept>
#include <vector>

// Count C++ allocations in the actual processor callback, including its core.
// Buffers, MIDI events, state and processors are constructed outside the guard.
namespace {
thread_local bool inCallback = false;
thread_local int allocations = 0, cAllocations = 0, deallocations = 0, mutexLocks = 0;
}
extern "C" {
void* __real_malloc(std::size_t);
void* __real_calloc(std::size_t, std::size_t);
void* __real_realloc(void*, std::size_t);
void __real_free(void*);
int __real_pthread_mutex_lock(pthread_mutex_t*);
void* __wrap_malloc(std::size_t n) { if (inCallback) ++cAllocations; return __real_malloc(n); }
void* __wrap_calloc(std::size_t n, std::size_t size) { if (inCallback) ++cAllocations; return __real_calloc(n, size); }
void* __wrap_realloc(void* p, std::size_t n) { if (inCallback) ++cAllocations; return __real_realloc(p, n); }
void __wrap_free(void* p) { if (inCallback && p != nullptr) ++deallocations; __real_free(p); }
int __wrap_pthread_mutex_lock(pthread_mutex_t* p) { if (inCallback) ++mutexLocks; return __real_pthread_mutex_lock(p); }
}
void* operator new(std::size_t n)
{
    if (inCallback) ++allocations;
    if (auto* p = std::malloc(std::max(n, std::size_t{1}))) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace
{
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
using Processor = sidsynth::PluginProcessor;
struct Event { int sample; juce::MidiMessage message; };
using juce::MidiMessage;

void setParameter(Processor& processor, const char* id, float value)
{
    auto* parameter = processor.state.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

void configure(Processor& processor, const sidsynth::MidiConfiguration& config)
{
    using namespace sidsynth::parameters;
    setParameter(processor, modeId, config.mode == sidsynth::PlayMode::layered ? 1.0f : 0.0f);
    setParameter(processor, layeredChannelId, static_cast<float>(config.layeredChannel));
    for (std::size_t i = 0; i < 3; ++i)
        setParameter(processor, channelIds[i], static_cast<float>(config.channels[i]));
}

void process(Processor& processor, juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    inCallback = true;
    processor.processBlock(audio, midi);
    inCallback = false;
    require(allocations == 0, "Audio callback allocated memory");
    require(cAllocations == 0 && deallocations == 0, "Audio callback called a C allocator/deallocator");
    require(mutexLocks == 0, "Audio callback locked a pthread mutex");
    require(midi.isEmpty(), "Instrument leaked MIDI to output");
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const float sample = audio.getSample(0, i);
        require(std::isfinite(sample) && std::abs(sample) <= 1.0f, "Invalid/clipped audio");
        if (audio.getNumChannels() == 2)
            require(sample == audio.getSample(1, i), "Stereo outputs differ");
    }
}

std::vector<float> render(double rate, int block, int length, const std::vector<Event>& events,
                          const sidsynth::MidiConfiguration& config = {})
{
    Processor processor;
    configure(processor, config);
    processor.prepareToPlay(rate, block);
    std::vector<float> result(static_cast<std::size_t>(length));
    for (int cursor = 0; cursor < length; cursor += block)
    {
        const int count = std::min(block, length - cursor);
        juce::AudioBuffer<float> audio(2, count);
        juce::MidiBuffer midi;
        for (const auto& event : events)
            if (event.sample >= cursor && event.sample < cursor + count)
                midi.addEvent(event.message, event.sample - cursor);
        process(processor, audio, midi);
        std::copy_n(audio.getReadPointer(0), count, result.begin() + cursor);
    }
    return result;
}

void routerTests()
{
    sidsynth::MidiRouter router;
    auto send = [&](const MidiMessage& message) { return router.handle(message.getRawData(), message.getRawDataSize()); };
    auto on = [&](int ch, int n) { return send(MidiMessage::noteOn(ch, n, (juce::uint8)100)); };
    auto off = [&](int ch, int n) { return send(MidiMessage::noteOff(ch, n)); };
    auto cc = [&](int ch, int control, int value) { return send(MidiMessage::controllerEvent(ch, control, value)); };
    auto notes = [&](int a, int b, int c) {
        require(router.voice(0).note == a && router.voice(1).note == b && router.voice(2).note == c,
            "Unexpected note ownership/fallback/channel isolation");
    };
    require(on(1, 60).retriggerMask == 1, "Note attack did not target voice 1");
    on(2, 64); on(3, 67); notes(60, 64, 67);
    on(1, 72); off(2, 60); off(1, 99); notes(72, 64, 67);
    off(1, 72); notes(60, 64, 67);
    on(1, 60); off(1, 60); notes(60, 64, 67); // Two ons require two offs.
    off(1, 60); notes(-1, 64, 67);
    off(1, 60); notes(-1, 64, 67);
    on(1, 50); on(1, 52); on(1, 50); off(1, 50); notes(50, 64, 67);
    off(1, 50); notes(52, 64, 67);
    send(MidiMessage::noteOn(1, 52, (juce::uint8)0)); notes(-1, 64, 67);
    on(1, 60); cc(1, 64, 127); on(1, 72); off(1, 72); notes(72, 64, 67);
    cc(2, 64, 0); notes(72, 64, 67);
    cc(1, 64, 0); notes(60, 64, 67);
    send(MidiMessage::pitchWheel(2, 16383));
    require(router.voice(1).bendSemitones == 2.0 && router.voice(0).bendSemitones == 0.0,
        "Pitch bend leaked between channels");
    send(MidiMessage::pitchWheel(2, 0));
    require(router.voice(1).bendSemitones == -2.0, "Negative bend range incorrect");
    cc(2, 64, 127); off(2, 64); cc(2, 121, 0); notes(60, -1, 67);
    require(router.voice(1).bendSemitones == 0.0, "Reset controllers retained pitch bend");
    cc(1, 64, 127); cc(1, 123, 0); notes(60, -1, 67);
    cc(1, 64, 0); notes(-1, -1, 67);
    require(cc(3, 120, 0).hardMuteMask == 4, "All sound off targeted another part");
    notes(-1, -1, -1);

    // Every pitch can be held; repeating a pitch must not consume a new slot.
    for (int n = 0; n < 128; ++n) on(1, n);
    for (int n = 0; n < 128; ++n) on(1, n);
    for (int n = 127; n >= 0; --n)
    {
        off(1, n); require(router.voice(0).note == n, "Repeated pitch released too early");
        off(1, n); require(router.voice(0).note == n - 1, "Full stack lost held-note fallback");
    }
    sidsynth::MidiConfiguration config;
    config.channels = {16, 16, 4};
    require(router.configure(config), "Channel change was not recognized");
    require(on(16, 69).retriggerMask == 3, "Shared channel assignment did not target both voices");
    on(4, 72); notes(69, 69, 72);
    require(!router.configure(config), "Unchanged configuration cleared ownership");
    notes(69, 69, 72);
    config.mode = sidsynth::PlayMode::layered;
    config.layeredChannel = 10;
    router.configure(config); notes(-1, -1, -1);
    on(16, 60); notes(-1, -1, -1);
    require(on(10, 69).retriggerMask == 7, "Layered attack missed oscillators");
    notes(69, 69, 69);
    send(MidiMessage::pitchWheel(10, 16383));
    require(router.voice(2).bendSemitones == 2.0, "Layered pitch bend missed voice 3");
    require(cc(10, 120, 0).hardMuteMask == 7, "Layered all-sound-off missed voices");
    notes(-1, -1, -1);
    on(10, 70); send(MidiMessage(0xff)); notes(-1, -1, -1);
    require(router.voice(0).bendSemitones == 0.0, "System reset retained bend");
    const std::uint8_t malformed[]{0x99, 0xff, 100};
    router.handle(malformed, 3); router.handle(malformed, 2); router.handle(nullptr, 0);
    notes(-1, -1, -1);
}

double rms(const std::vector<float>& data, int start, int end)
{
    double sum = 0.0;
    for (int i = start; i < end; ++i) sum += data[static_cast<std::size_t>(i)] * data[static_cast<std::size_t>(i)];
    return std::sqrt(sum / (end - start));
}

double pitch(const std::vector<float>& data, int rate)
{
    // Autocorrelation with quadratic lag interpolation, after the attack/decay.
    auto correlation = [&](int lag) {
        double result = 0;
        for (int i = rate / 4; i < rate / 2; ++i)
            result += data[static_cast<std::size_t>(i)] * data[static_cast<std::size_t>(i + lag)];
        return result;
    };
    int best = rate / 500;
    for (int lag = best + 1; lag <= rate / 400; ++lag)
        if (correlation(lag) > correlation(best)) best = lag;
    const auto left = correlation(best - 1), centre = correlation(best), right = correlation(best + 1);
    const double delta = 0.5 * (left - right) / (left - 2.0 * centre + right);
    return rate / (best + delta);
}

void writeWav(const char* path, const std::vector<float>& data, int rate)
{
    std::ofstream stream(path, std::ios::binary);
    require(stream.good(), "Cannot create audition WAV");
    auto u16 = [&](unsigned n) { stream.put(static_cast<char>(n)); stream.put(static_cast<char>(n >> 8)); };
    auto u32 = [&](unsigned n) { u16(n & 0xffff); u16(n >> 16); };
    const auto bytes = static_cast<unsigned>(data.size() * 2);
    stream.write("RIFF", 4); u32(36 + bytes); stream.write("WAVEfmt ", 8); u32(16);
    u16(1); u16(1); u32(static_cast<unsigned>(rate)); u32(static_cast<unsigned>(rate * 2)); u16(2); u16(16);
    stream.write("data", 4); u32(bytes);
    for (float sample : data)
        u16(static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(sample * 32767.0f))));
    require(stream.good(), "Failed writing audition WAV");
}

juce::MemoryBlock stateXml(const juce::XmlElement& xml)
{
    juce::MemoryBlock block;
    juce::AudioProcessor::copyXmlToBinary(xml, block);
    return block;
}

void stateTests()
{
    Processor source;
    auto* gain = source.state.getParameter(sidsynth::parameters::gainId);
    gain->setValueNotifyingHost(gain->convertTo0to1(-24.0f));
    juce::MemoryBlock saved;
    source.getStateInformation(saved);
    Processor restored;
    restored.prepareToPlay(48000, 256);
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    auto level = [&] { return restored.state.getRawParameterValue(sidsynth::parameters::gainId)->load(); };
    require(std::abs(level() + 24.0f) < 0.001f, "Saved output level did not restore");
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer midi;
    process(restored, audio, midi);
    require(audio.getMagnitude(0, 256) == 0.0f, "State restored a live note");

    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
    process(restored, audio, midi);
    require(audio.getMagnitude(0, 256) > 0.0f, "State test note was silent");
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    process(restored, audio, midi);
    require(audio.getMagnitude(0, 256) == 0.0f, "Restore failed to clear active notes");

    juce::XmlElement xml(sidsynth::parameters::stateTag);
    xml.setAttribute("schemaVersion", 99);
    xml.setAttribute(sidsynth::parameters::gainId, 0.0);
    auto future = stateXml(xml);
    restored.setStateInformation(future.getData(), static_cast<int>(future.getSize()));
    require(std::abs(level() + 24.0f) < 0.001f, "Unknown schema changed the patch");
    restored.setStateInformation("invalid", 7);
    restored.setStateInformation(saved.getData(), 5);
    require(std::abs(level() + 24.0f) < 0.001f, "Malformed state changed the patch");
    xml.setAttribute("schemaVersion", 1);
    xml.removeAttribute(sidsynth::parameters::gainId);
    xml.setAttribute("futureField", "ignored");
    auto missing = stateXml(xml);
    restored.setStateInformation(missing.getData(), static_cast<int>(missing.getSize()));
    require(std::abs(level() + 12.0f) < 0.001f, "Missing parameter did not use default");
    xml.setAttribute(sidsynth::parameters::gainId, 100.0);
    auto outside = stateXml(xml);
    restored.setStateInformation(outside.getData(), static_cast<int>(outside.getSize()));
    require(level() == 0.0f, "Out-of-range saved gain was not clamped");
}

void routingStateTests()
{
    using namespace sidsynth::parameters;
    Processor source, restored;
    sidsynth::MidiConfiguration config;
    config.mode = sidsynth::PlayMode::layered;
    config.layeredChannel = 16;
    config.channels = {4, 10, 15};
    configure(source, config);
    setParameter(source, gainId, -18.0f);
    juce::MemoryBlock saved;
    source.getStateInformation(saved);
    restored.prepareToPlay(48000, 512);
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    auto value = [&](const char* id) { return restored.state.getRawParameterValue(id)->load(); };
    require(value(modeId) == 1 && value(layeredChannelId) == 16 && value(channelIds[0]) == 4
        && value(channelIds[1]) == 10 && value(channelIds[2]) == 15, "Routing state did not round trip");
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
    process(restored, audio, midi);
    require(audio.getMagnitude(0, 512) == 0, "Restored layered input accepted the wrong channel");
    midi.addEvent(MidiMessage::noteOn(16, 69, (juce::uint8)100), 0);
    midi.addEvent(MidiMessage::controllerEvent(16, 64, 127), 10);
    process(restored, audio, midi);
    require(audio.getMagnitude(0, 512) > 0, "Restored layered channel did not play");
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    midi.addEvent(MidiMessage::noteOff(16, 69), 0);
    midi.addEvent(MidiMessage::controllerEvent(16, 64, 0), 1);
    process(restored, audio, midi);
    require(audio.getMagnitude(0, 512) == 0, "Restore retained sustained/held ownership");

    juce::XmlElement xml(stateTag);
    xml.setAttribute("schemaVersion", 1);
    xml.setAttribute(gainId, -24.0);
    auto legacy = stateXml(xml);
    restored.setStateInformation(legacy.getData(), static_cast<int>(legacy.getSize()));
    require(value(gainId) == -24 && value(modeId) == 0 && value(layeredChannelId) == 1
        && value(channelIds[0]) == 1 && value(channelIds[1]) == 2 && value(channelIds[2]) == 3,
        "Stage 1 migration did not preserve its gain and MIDI defaults");

    xml.setAttribute("schemaVersion", 2);
    xml.setAttribute(modeId, 7.0);
    xml.setAttribute(layeredChannelId, 0.0);
    xml.setAttribute(channelIds[0], 99.0);
    xml.setAttribute(channelIds[1], 4.4);
    auto outside = stateXml(xml);
    restored.setStateInformation(outside.getData(), static_cast<int>(outside.getSize()));
    require(value(modeId) == 1 && value(layeredChannelId) == 1 && value(channelIds[0]) == 16
        && value(channelIds[1]) == 4 && value(channelIds[2]) == 3, "Routing clamp/default behavior failed");
    for (const char* invalid : {"nan", "inf", "invalid", "4oops"})
    {
        xml.setAttribute(gainId, -6.0);
        xml.setAttribute(channelIds[1], invalid);
        auto bad = stateXml(xml);
        restored.setStateInformation(bad.getData(), static_cast<int>(bad.getSize()));
        require(value(gainId) == -24 && value(channelIds[1]) == 4, "Invalid routing partially changed the patch");
    }
}

void transitionTests()
{
    using namespace sidsynth::parameters;
    const std::array<const char*, 5> ids{modeId, layeredChannelId, channelIds[0], channelIds[1], channelIds[2]};
    for (const auto* id : ids)
    {
        Processor processor;
        processor.prepareToPlay(48000, 512);
        juce::AudioBuffer<float> audio(2, 512);
        juce::MidiBuffer midi;
        for (int channel = 1; channel <= 3; ++channel)
        {
            midi.addEvent(MidiMessage::noteOn(channel, 60, (juce::uint8)100), 0);
            midi.addEvent(MidiMessage::controllerEvent(channel, 64, 127), 5);
            midi.addEvent(MidiMessage::noteOn(channel, 72, (juce::uint8)100), 30);
        }
        process(processor, audio, midi);
        setParameter(processor, id, id == modeId ? 1.0f : 16.0f);
        process(processor, audio, midi);
        require(audio.getMagnitude(0, 512) == 0, "Routing change did not cut held notes");
        for (int channel = 1; channel <= 3; ++channel)
        {
            midi.addEvent(MidiMessage::noteOff(channel, 72), 0);
            midi.addEvent(MidiMessage::controllerEvent(channel, 64, 0), 5);
            midi.addEvent(MidiMessage::pitchWheel(channel, 16383), 10);
        }
        process(processor, audio, midi);
        require(audio.getMagnitude(0, 512) == 0, "Old note ownership reappeared after routing change");
        const int channel = id == channelIds[0] ? 16 : 1;
        midi.addEvent(MidiMessage::noteOn(channel, 69, (juce::uint8)100), 0);
        process(processor, audio, midi);
        require(audio.getMagnitude(0, 512) > 0, "New routing did not recover after reset");
    }
}

void engineVoiceTests()
{
    sidsynth::SidEngine engine;
    engine.prepare(48000);
    auto upper = engine.voicePatch(1);
    upper.waveform = 0x10; upper.tuningSemitones = 12; upper.attackDecay = 0x12;
    auto lower = engine.voicePatch(2);
    lower.waveform = 0x40; lower.tuningSemitones = -12; lower.sustainRelease = 0xa3;
    engine.setVoicePatch(1, upper); engine.setVoicePatch(2, lower);
    for (int i = 0; i < 3; ++i) engine.setVoice(i, 69);
    auto hz = [&](int voice) { return engine.frequencyRegister(voice) * engine.palClock / 16777216.0; };
    require(std::abs(hz(0) - 440) < 0.04 && std::abs(hz(1) - 880) < 0.04 && std::abs(hz(2) - 220) < 0.04,
        "Layered note overwrote individual oscillator tuning");
    require(engine.registerValue(4) == 0x21 && engine.registerValue(11) == 0x11 && engine.registerValue(18) == 0x41
        && engine.registerValue(12) == 0x12 && engine.registerValue(20) == 0xa3,
        "Individual waveform/envelope patch was lost");
    const auto otherFrequency = engine.frequencyRegister(1);
    engine.setVoice(0, -1);
    require(engine.registerValue(4) == 0x20 && engine.activeNote(1) == 69 && engine.registerValue(11) == 0x11,
        "Releasing voice 1 released voice 2");
    engine.silenceVoice(2);
    require(engine.registerValue(18) == 8 && engine.frequencyRegister(1) == otherFrequency && engine.registerValue(11) == 0x11,
        "Channel hard mute reset another oscillator");
    engine.setVoice(2, 69, 2.0);
    require(engine.registerValue(18) == 0x41 && hz(2) > 246 && hz(2) < 248, "Muted voice/bend did not recover");
    engine.panic();
    for (int i = 0; i < 3; ++i) engine.setVoice(i, 69);
    require(engine.registerValue(11) == 0x11 && engine.registerValue(18) == 0x41 && std::abs(hz(1) - 880) < 0.04,
        "Performance reset changed oscillator patches");
    engine.setVoice(0, 0, -2.0);
    auto lowest = engine.voicePatch(0);
    lowest.tuningSemitones = -96;
    engine.setVoicePatch(0, lowest);
    require(engine.frequencyRegister(0) == 0 && engine.registerValue(0) == 0 && engine.registerValue(1) == 0,
        "Tuning down to register zero retained the previous hardware frequency");
}

void multiPartTests(const char* auditionPath)
{
    using namespace sidsynth;
    const std::vector<Event> independent{
        {113, MidiMessage::noteOn(1, 60, (juce::uint8)100)},
        {527, MidiMessage::noteOn(2, 64, (juce::uint8)100)},
        {1024, MidiMessage::noteOn(3, 67, (juce::uint8)100)},
        {4111, MidiMessage::noteOn(1, 72, (juce::uint8)80)},
        {5203, MidiMessage::noteOff(1, 72)},
        {6000, MidiMessage::controllerEvent(2, 64, 127)},
        {7117, MidiMessage::noteOff(2, 64)},
        {8192, MidiMessage::pitchWheel(3, 16383)},
        {9001, MidiMessage::noteOn(1, 60, (juce::uint8)80)},
        {10280, MidiMessage::noteOff(1, 60)},
        {13003, MidiMessage::controllerEvent(2, 64, 0)},
        {14017, MidiMessage::allSoundOff(3)},
        {15000, MidiMessage::noteOff(1, 60)}};
    MidiConfiguration layered;
    layered.mode = PlayMode::layered; layered.layeredChannel = 16;
    const std::vector<Event> layers{
        {113, MidiMessage::noteOn(16, 69, (juce::uint8)100)},
        {4111, MidiMessage::noteOn(16, 72, (juce::uint8)100)},
        {5203, MidiMessage::noteOff(16, 72)},
        {6000, MidiMessage::controllerEvent(16, 64, 127)},
        {7117, MidiMessage::noteOff(16, 69)},
        {8192, MidiMessage::pitchWheel(16, 0)},
        {13003, MidiMessage::controllerEvent(16, 64, 0)},
        {15001, MidiMessage::allSoundOff(16)}};
    for (int rate : {44100, 48000, 96000})
        for (bool useLayers : {false, true})
        {
            const auto& events = useLayers ? layers : independent;
            const auto config = useLayers ? layered : MidiConfiguration{};
            const auto baseline = render(rate, 257, rate, events, config);
            require(rms(baseline, 0, 113) == 0 && rms(baseline, 2000, 4000) > 0.005,
                "Three-voice audio missing or preceded event offset");
            for (int block : {64, 1024})
                require(baseline == render(rate, block, rate, events, config), "Two-mode timeline depends on block size");
        }

    const auto combined = render(48000, 257, 32000, {{113, MidiMessage::noteOn(16, 69, (juce::uint8)100)},
        {18000, MidiMessage::noteOff(16, 69)}}, layered);
    const auto three = render(48000, 257, 32000, {{113, MidiMessage::noteOn(1, 69, (juce::uint8)100)},
        {113, MidiMessage::noteOn(2, 69, (juce::uint8)100)}, {113, MidiMessage::noteOn(3, 69, (juce::uint8)100)},
        {18000, MidiMessage::noteOff(1, 69)}, {18000, MidiMessage::noteOff(2, 69)}, {18000, MidiMessage::noteOff(3, 69)}});
    require(combined == three, "Layered mode is not the same three oscillators playing together");
    for (int channel = 1; channel <= 3; ++channel)
    {
        const auto solo = render(48000, 257, 48000, {{113, MidiMessage::noteOn(channel, 69, (juce::uint8)100)}});
        require(rms(solo, 12000, 24000) > 0.005 && std::abs(pitch(solo, 48000) - 440) < 1,
            "An independent MIDI part did not produce the expected pitch");
    }
    const auto releasedOthers = render(48000, 257, 48000, {{0, MidiMessage::noteOn(1, 69, (juce::uint8)100)},
        {0, MidiMessage::noteOn(2, 60, (juce::uint8)100)}, {0, MidiMessage::noteOn(3, 64, (juce::uint8)100)},
        {1000, MidiMessage::noteOff(2, 60)}, {1200, MidiMessage::allSoundOff(3)}});
    require(rms(releasedOthers, 24000, 48000) > 0.005 && std::abs(pitch(releasedOthers, 48000) - 440) < 1,
        "Releasing/muting other parts interrupted the held part");
    const auto fallback = render(48000, 257, 48000, {{0, MidiMessage::noteOn(1, 69, (juce::uint8)100)},
        {1000, MidiMessage::noteOn(1, 72, (juce::uint8)100)}, {2000, MidiMessage::noteOff(1, 72)}});
    require(std::abs(pitch(fallback, 48000) - 440) < 1 && rms(fallback, 24000, 48000) > 0.005,
        "Processor did not return to the held A4 after releasing the newer note");
    const auto bent = render(48000, 257, 48000, {{0, MidiMessage::noteOn(2, 69, (juce::uint8)100)},
        {4000, MidiMessage::pitchWheel(2, 16383)}});
    require(std::abs(pitch(bent, 48000) - 493.883) < 1, "Processor did not apply channel pitch bend to audio");
    // Leave inspectable audio artifacts for the separate listening check.
    if (auditionPath != nullptr)
    {
        const auto directory = juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(auditionPath)).getParentDirectory();
        writeWav(directory.getChildFile("stage2-three-parts.wav").getFullPathName().toRawUTF8(),
            render(48000, 257, 96000, independent), 48000);
        writeWav(directory.getChildFile("stage2-layered.wav").getFullPathName().toRawUTF8(),
            render(48000, 257, 96000, layers, layered), 48000);
    }
}

void panicTests()
{
    Processor processor;
    processor.prepareToPlay(48000, 512);
    juce::AudioBuffer<float> audio(2, 512);
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
    process(processor, audio, midi);
    require(audio.getMagnitude(0, 512) > 0.0f, "No initial note");
    midi.addEvent(juce::MidiMessage::allSoundOff(1), 173);
    process(processor, audio, midi);
    require(audio.getMagnitude(0, 173) > 0.0f && audio.getMagnitude(173, 339) == 0.0f,
        "MIDI panic ignored its event offset");
    for (int action = 0; action < 3; ++action)
    {
        midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
        process(processor, audio, midi);
        if (action == 0) processor.requestPanic();
        if (action == 1) processor.reset();
        if (action == 2) { processor.releaseResources(); processor.prepareToPlay(44100, 512); }
        process(processor, audio, midi);
        require(audio.getMagnitude(0, 512) == 0.0f, "Panic/reset retained a note");
    }
    juce::AudioBuffer<float> empty(2, 0);
    process(processor, empty, midi);
    struct PlayHead final : juce::AudioPlayHead
    {
        bool playing = true;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo position;
            position.setIsPlaying(playing);
            return position;
        }
    } playhead;
    processor.setPlayHead(&playhead);
    midi.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8)100), 0);
    process(processor, audio, midi);
    playhead.playing = false;
    process(processor, audio, midi);
    require(audio.getMagnitude(0, 512) == 0.0f, "Transport stop retained a note");
    processor.setPlayHead(nullptr);

    std::vector<juce::uint8> sysex(64000, 42);
    midi.addEvent(juce::MidiMessage::createSysExMessage(sysex.data(), static_cast<int>(sysex.size())), 17);
    process(processor, audio, midi);
    require(audio.getMagnitude(0, 512) == 0.0f, "Ignored SysEx produced audio");
}
#include "Stage3Tests.h"
#include "Stage4Tests.h"
#include "Stage5Tests.h"
#include "Benchmark.h"
}

int main(int argc, char** argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI juce;
        if (argc > 1 && std::string(argv[1]) == "--benchmark") return benchmark();
        using juce::MidiMessage;
        std::vector<float> audition;
        for (const int rate : {44100, 48000, 96000})
        {
            const int on = 113, off = rate * 4 / 5 + 37, end = rate * 2;
            const std::vector<Event> notes{{on, MidiMessage::noteOn(1, 69, (juce::uint8)100)},
                {off, MidiMessage::noteOff(1, 69)}};
            auto audio = render(rate, 257, end, notes);
            require(rms(audio, 0, on) == 0.0, "Audio preceded note offset");
            const double held = rms(audio, rate / 4, rate / 2);
            const double tail = rms(audio, rate * 3 / 2, end);
            const double hz = pitch(audio, rate);
            std::cout << rate << " Hz: A4=" << hz << " Hz, held RMS=" << held << ", released RMS=" << tail << '\n';
            require(held > 0.005, "Note is inaudible");
            require(rms(audio, off + rate / 100, off + rate / 20) > held * 0.05,
                "Note-off cut the sound instead of preserving the SID release");
            require(tail < 0.0002 && tail < held * 0.01, "Note-off did not release");
            require(std::abs(hz - 440.0) < 1.0, "A4 pitch is incorrect");
            for (const int block : {64, 1024})
            {
                const auto other = render(rate, block, end, notes);
                require(audio == other, "Equivalent event timeline depends on host block size");
            }
            if (rate == 48000) audition = std::move(audio);
        }
        const auto baseline = render(48000, 257, 16000, {{0, MidiMessage::noteOn(1, 69, (juce::uint8)100)}});
        auto shifted = render(48000, 257, 16113, {{113, MidiMessage::noteOn(1, 69, (juce::uint8)100)}});
        require(std::equal(baseline.begin(), baseline.end(), shifted.begin() + 113), "Note-on shifted from requested offset");
        auto isolated = render(48000, 257, 16000, {{0, MidiMessage::noteOn(1, 69, (juce::uint8)100)},
            {130, MidiMessage::noteOff(16, 69)}, {220, MidiMessage::noteOff(1, 60)},
            {400, MidiMessage::noteOn(16, 72, (juce::uint8)100)}, {500, MidiMessage::allSoundOff(16)}});
        require(baseline == isolated, "Unowned/channel-16 event altered channel-1 note");
        const auto zeroVelocity = render(48000, 257, 48000, {{0, MidiMessage::noteOn(1, 69, (juce::uint8)100)},
            {10000, MidiMessage::noteOn(1, 69, (juce::uint8)0)}});
        require(rms(zeroVelocity, 40000, 48000) < 0.0002, "Velocity-zero note did not release");
        sidsynth::SidEngine engine;
        engine.prepare(48000);
        engine.setVoice(0, 69);
        require(std::abs(engine.frequencyRegister(0) * engine.palClock / 16777216.0 - 440.0) < 0.04,
            "A4 register does not encode concert pitch");
        engine.setVoice(0, 127);
        require(engine.frequencyRegister(0) == 65535, "High MIDI pitch wrapped instead of saturating");
        engine.setVoice(0, 0);
        require(engine.frequencyRegister(0) > 0 && engine.frequencyRegister(0) < 200, "Low MIDI pitch was lost");
        routerTests();
        engineVoiceTests();
        multiPartTests(argc > 1 ? argv[1] : nullptr);
        transitionTests();
        routingStateTests();
        stateTests();
        panicTests();
        stage3RegisterTests();
        stage3OwnershipTests();
        stage3StateTests();
        stage3BackendTests();
        stage3InteractionTests();
        stage3UiPanicTest();
        stage4AuditionTests();
        stage4EditorTests(argc > 2 ? argv[2] : nullptr);
        stage5Tests();
        stage3AudioTests(argc > 1 ? argv[1] : nullptr);
        if (argc > 1) writeWav(argv[1], audition, 48000);
        if (argc > 2)
        {
            Processor processor;
            std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
            auto preview = editor->createComponentSnapshot(editor->getLocalBounds());
            juce::File imageFile{juce::String(argv[2])};
            auto stream = imageFile.createOutputStream();
            require(stream != nullptr, "Cannot create editor preview");
            stream->setPosition(0);
            stream->truncate();
            require(juce::PNGImageFormat().writeImageToStream(preview, *stream), "Editor preview failed");
        }
        std::cout << "PASS: two modes, held/repeated notes, sustain/bend, routing transitions, timing, "
                  << "block consistency, release, schema migration/state, panic, complete SID registers, manual ownership, model/clock, EXT IN; "
                  << allocations << " C++ callback allocations, " << cAllocations << " C allocator calls, "
                  << deallocations << " frees, " << mutexLocks << " pthread mutex locks.\n";
        return 0;
    }
    catch (const std::exception& e) { inCallback = false; std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
