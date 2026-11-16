// SPDX-License-Identifier: AGPL-3.0-only
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <charconv>

namespace sidsynth
{
PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties().withInput("EXT IN", juce::AudioChannelSet::stereo(), false)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, parameters::stateTag, parameters::layout())
{
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = state.getRawParameterValue(parameters::specs()[i].id);
}

MidiConfiguration PluginProcessor::midiConfiguration() const noexcept
{
    MidiConfiguration config;
    config.mode = value(parameters::mode) < 0.5f ? PlayMode::threeParts : PlayMode::layered;
    config.layeredChannel = juce::jlimit(1, 16, static_cast<int>(value(parameters::layeredChannel)));
    for (int i = 0; i < 3; ++i)
        config.channels[static_cast<std::size_t>(i)] = juce::jlimit(1, 16, static_cast<int>(value(parameters::channel1 + i)));
    return config;
}

void PluginProcessor::audition(int note, unsigned voiceMask, unsigned epoch) noexcept
{
    const unsigned intent = note >= 0 && note <= 127 ? ((voiceMask & 7u) << 8) | static_cast<unsigned>(note + 1) : 0;
    previewRequest.store((static_cast<std::uint64_t>(epoch) << 32) | intent, std::memory_order_release);
}
void PluginProcessor::clearAudition() noexcept
{
    previewEpoch.fetch_add(1, std::memory_order_acq_rel);
    activePreview = 0;
}
void PluginProcessor::publishGates() noexcept
{
    unsigned gates = 0;
    for (int v = 0; v < 3; ++v) gates |= (engine.registerValue(v * 7 + 4) & 1u) << v;
    gateSnapshot.store(gates, std::memory_order_relaxed);
}
void PluginProcessor::clearPerformance() noexcept { router.reset(); engine.panic(); clearAudition(); }

void PluginProcessor::applyPatch() noexcept
{
    using namespace parameters;
    bool sourceChanged = false;
    for (int v = 0; v < 3; ++v)
        sourceChanged |= (value(voice(v, source)) >= 0.5f) != engine.voicePatch(v).manual;
    if (sourceChanged) clearPerformance();
    SidEngine::GlobalPatch global;
    global.cutoff = static_cast<std::uint16_t>(value(cutoff));
    global.resonance = static_cast<unsigned char>(value(resonance));
    global.routing = static_cast<unsigned char>((value(route1) >= 0.5f ? 1 : 0) | (value(route2) >= 0.5f ? 2 : 0)
        | (value(route3) >= 0.5f ? 4 : 0) | (value(routeExternal) >= 0.5f ? 8 : 0));
    global.modes = static_cast<unsigned char>((value(lowPass) >= 0.5f ? 1 : 0) | (value(bandPass) >= 0.5f ? 2 : 0)
        | (value(highPass) >= 0.5f ? 4 : 0));
    global.volume = static_cast<unsigned char>(value(volume));
    global.model8580 = value(model) >= 0.5f;
    global.ntsc = value(clock) >= 0.5f;
    global.voice3Direct = value(voice3Direct) >= 0.5f;
    const bool chipChanged = engine.setGlobalPatch(global);
    if (chipChanged) { router.reset(); clearAudition(); }
    for (int v = 0; v < 3; ++v)
    {
        SidEngine::VoicePatch patch;
        auto get = [&](VoiceField f) { return value(voice(v, f)); };
        patch.waveform = static_cast<unsigned char>((get(triangle) >= 0.5f ? 0x10 : 0) | (get(saw) >= 0.5f ? 0x20 : 0)
            | (get(pulse) >= 0.5f ? 0x40 : 0) | (get(noise) >= 0.5f ? 0x80 : 0));
        patch.attackDecay = static_cast<unsigned char>((static_cast<int>(get(attack)) << 4) | static_cast<int>(get(decay)));
        patch.sustainRelease = static_cast<unsigned char>((static_cast<int>(get(sustain)) << 4) | static_cast<int>(get(release)));
        patch.pulseWidth = static_cast<std::uint16_t>(get(pulseWidth));
        patch.tuningSemitones = get(tuning);
        patch.manualFrequency = static_cast<std::uint16_t>(get(frequency));
        patch.manual = get(source) >= 0.5f;
        patch.manualGate = get(manualGate) >= 0.5f;
        patch.sync = get(sync) >= 0.5f;
        patch.ring = get(ring) >= 0.5f;
        patch.test = get(test) >= 0.5f;
        engine.setVoicePatch(v, patch);
    }
    if (chipChanged) engine.panic(); // Configuration edits clear manual holds too.
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto input = layout.getMainInputChannelSet();
    const auto output = layout.getMainOutputChannelSet();
    return (input.isDisabled() || input == juce::AudioChannelSet::mono() || input == juce::AudioChannelSet::stereo())
        && (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo());
}

void PluginProcessor::prepareToPlay(double sampleRate, int)
{
    applyPatch();
    engine.prepare(sampleRate);
    router.configure(midiConfiguration());
    router.reset();
    clearAudition();
    publishGates();
    gain.reset(sampleRate, 0.01);
    gain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(value(parameters::gain)));
    panicRequested.store(false);
    wasPlaying = false;
}

void PluginProcessor::releaseResources() { requestPanic(); }
void PluginProcessor::reset() { requestPanic(); }

void PluginProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    bool stopped = false;
    if (auto* playhead = getPlayHead())
        if (const auto position = playhead->getPosition())
        {
            const bool playing = position->getIsPlaying();
            stopped = wasPlaying && !playing;
            wasPlaying = playing;
        }
    const bool globalReset = panicRequested.exchange(false, std::memory_order_acq_rel);
    const bool routingChanged = router.configure(midiConfiguration());
    applyPatch();
    if (globalReset || stopped || routingChanged) clearPerformance();
    const auto request = previewRequest.load(std::memory_order_acquire);
    unsigned preview = (request >> 32) == auditionEpoch() ? static_cast<unsigned>(request & 0x7ffu) : 0;
    if (preview != activePreview)
    {
        for (int v = 0; v < 3; ++v)
        {
            const unsigned bit = 1u << (v + 8);
            if ((preview & bit) != 0) engine.setVoice(v, static_cast<int>(preview & 255u) - 1, 0.0, true);
            else if ((activePreview & bit) != 0)
            {
                const auto held = router.voice(v);
                engine.setVoice(v, held.note, held.bendSemitones);
            }
        }
        activePreview = preview;
    }
    const int samples = audio.getNumSamples();
    if (samples == 0 || getTotalNumOutputChannels() == 0 || audio.getNumChannels() == 0)
    {
        midi.clear();
        publishGates();
        return;
    }
    gain.setTargetValue(juce::Decibels::decibelsToGain(value(parameters::gain)));
    auto* output = audio.getWritePointer(0);
    const float* inputLeft = getTotalNumInputChannels() > 0 ? audio.getReadPointer(0) : nullptr;
    const float* inputRight = getTotalNumInputChannels() > 1 ? audio.getReadPointer(1) : nullptr;
    auto render = [&](int start, int count)
    {
        engine.render(output + start, count, inputLeft == nullptr ? nullptr : inputLeft + start,
            inputRight == nullptr ? nullptr : inputRight + start);
    };
    int cursor = 0;
    for (const auto event : midi)
    {
        const int offset = juce::jlimit(cursor, samples, event.samplePosition);
        render(cursor, offset - cursor);
        cursor = offset;
        // Decode bytes directly: getMessage() can allocate for a large SysEx.
        if (event.samplePosition < 0 || event.samplePosition >= samples || event.numBytes < 1) continue;
        const auto result = router.handle(event.data, event.numBytes);
        if (result.systemReset) { engine.panic(); clearAudition(); preview = 0; continue; }
        if ((result.hardMuteMask & (preview >> 8)) != 0)
        {
            // CC120 cancels an audition too, without losing other host parts.
            clearAudition(); preview = 0;
        }
        for (int v = 0; v < 3; ++v)
        {
            if ((result.hardMuteMask & (1u << v)) != 0) engine.silenceVoice(v);
            else if ((preview & (1u << (v + 8))) == 0)
            {
                const auto performance = router.voice(v);
                engine.setVoice(v, performance.note, performance.bendSemitones, (result.retriggerMask & (1u << v)) != 0);
            }
        }
    }
    render(cursor, samples - cursor);
    for (int i = 0; i < samples; ++i) output[i] *= gain.getNextValue();
    for (int channel = 1; channel < audio.getNumChannels(); ++channel) audio.copyFrom(channel, 0, output, samples);
    midi.clear();
    publishGates();
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() { return new PluginEditor(*this); }

void PluginProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    juce::XmlElement xml(parameters::stateTag);
    xml.setAttribute("schemaVersion", parameters::schemaVersion);
    for (std::size_t i = 0; i < values.size(); ++i)
        xml.setAttribute(parameters::specs()[i].id, static_cast<double>(value(i)));
    copyXmlToBinary(xml, destination);
}

void PluginProcessor::setStateInformation(const void* data, int size)
{
    if (size <= 0 || size > 1024 * 1024 || data == nullptr) return;
    auto xml = getXmlFromBinary(data, size);
    if (!xml || !xml->hasTagName(parameters::stateTag)) return;
    const auto versionText = xml->getStringAttribute("schemaVersion");
    if (versionText != "1" && versionText != "2" && versionText != "3") return;
    const int version = versionText.getIntValue();
    std::array<float, parameters::count> restored{};
    // Validate the whole patch before applying anything. Old schemas interpret
    // only their own fields; the new controls take compatible default values.
    for (std::size_t i = 0; i < restored.size(); ++i)
    {
        const auto& spec = parameters::specs()[i];
        double parsed = spec.initial;
        if (spec.version <= version && xml->hasAttribute(spec.id))
        {
            const auto text = xml->getStringAttribute(spec.id).trim();
            const char* start = text.toRawUTF8();
            const char* end = start + text.getNumBytesAsUTF8();
            const auto result = std::from_chars(start, end, parsed);
            if (result.ec != std::errc{} || result.ptr != end || !std::isfinite(parsed)) return;
        }
        parsed = juce::jlimit(static_cast<double>(spec.minimum), static_cast<double>(spec.maximum), parsed);
        if (spec.kind != parameters::Kind::continuous) parsed = std::round(parsed);
        restored[i] = static_cast<float>(parsed);
    }
    for (std::size_t i = 0; i < restored.size(); ++i)
    {
        auto* parameter = state.getParameter(parameters::specs()[i].id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(restored[i]));
    }
    requestPanic(); // A saved manual hold is disarmed, as are live MIDI notes.
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new sidsynth::PluginProcessor(); }
