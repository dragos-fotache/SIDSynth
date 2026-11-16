// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace sidsynth::parameters
{
inline constexpr auto gainId = "output.gainDb";
inline constexpr auto modeId = "midi.mode";
inline constexpr auto layeredChannelId = "midi.layeredChannel";
inline constexpr std::array<const char*, 3> channelIds{"voice1.midiChannel", "voice2.midiChannel", "voice3.midiChannel"};
inline constexpr auto stateTag = "SIDSynthPatch";
inline constexpr int schemaVersion = 3;
inline constexpr float defaultGainDb = -12.0f;

// IDs and version hints of the first six stay unchanged.
enum Key : std::size_t
{
    gain, mode, layeredChannel, channel1, channel2, channel3,
    cutoff, resonance, route1, route2, route3, routeExternal,
    lowPass, bandPass, highPass, volume, model, clock, voice3Direct, voiceStart
};
enum VoiceField : std::size_t
{
    source, frequency, tuning, pulseWidth, attack, decay, sustain, release,
    triangle, saw, pulse, noise, manualGate, sync, ring, test, voiceFieldCount
};
constexpr std::size_t voice(int v, VoiceField field) { return voiceStart + static_cast<std::size_t>(v) * voiceFieldCount + field; }
inline constexpr std::size_t count = voiceStart + 3 * voiceFieldCount;
enum class Kind { continuous, integer, boolean, choice };
struct Spec
{
    juce::String id, name;
    float minimum = 0, maximum = 1, initial = 0, step = 1;
    int version = 3;
    Kind kind = Kind::integer;
    juce::String choices, unit;
};

inline const std::array<Spec, count>& specs()
{
    static const auto result = []
    {
        std::array<Spec, count> s{};
        s[gain] = {gainId, "Output", -60, 0, defaultGainDb, 0.1f, 1, Kind::continuous, {}, "dB"};
        s[mode] = {modeId, "Play mode", 0, 1, 0, 1, 2, Kind::choice, "3 parts|Layered", {}};
        s[layeredChannel] = {layeredChannelId, "Layered MIDI channel", 1, 16, 1, 1, 2, Kind::integer, {}, {}};
        for (int v = 0; v < 3; ++v)
            s[channel1 + v] = {channelIds[static_cast<std::size_t>(v)], "Voice " + juce::String(v + 1) + " MIDI channel",
                1, 16, static_cast<float>(v + 1), 1, 2, Kind::integer, {}, {}};
        s[cutoff] = {"filter.cutoff", "Filter cutoff", 0, 2047, 2047, 1, 3, Kind::integer, {}, "reg"};
        s[resonance] = {"filter.resonance", "Filter resonance", 0, 15, 0, 1, 3, Kind::integer, {}, "reg"};
        for (int v = 0; v < 3; ++v)
            s[route1 + v] = {"filter.voice" + juce::String(v + 1), "Filter voice " + juce::String(v + 1), 0, 1, 0, 1, 3, Kind::boolean, {}, {}};
        s[routeExternal] = {"filter.external", "Filter external input", 0, 1, 0, 1, 3, Kind::boolean, {}, {}};
        s[lowPass] = {"filter.lowPass", "Filter low pass", 0, 1, 0, 1, 3, Kind::boolean, {}, {}};
        s[bandPass] = {"filter.bandPass", "Filter band pass", 0, 1, 0, 1, 3, Kind::boolean, {}, {}};
        s[highPass] = {"filter.highPass", "Filter high pass", 0, 1, 0, 1, 3, Kind::boolean, {}, {}};
        s[volume] = {"chip.volume", "SID master volume", 0, 15, 15, 1, 3, Kind::integer, {}, "reg"};
        s[model] = {"chip.model", "SID model", 0, 1, 1, 1, 3, Kind::choice, "6581|8580", {}};
        s[clock] = {"chip.clock", "SID clock", 0, 1, 0, 1, 3, Kind::choice, "PAL|NTSC", {}};
        s[voice3Direct] = {"chip.voice3Direct", "Voice 3 direct output", 0, 1, 1, 1, 3, Kind::boolean, {}, {}};
        const std::array<const char*, voiceFieldCount> suffix{"source", "frequency", "tuning", "pulseWidth", "attack", "decay",
            "sustain", "release", "triangle", "saw", "pulse", "noise", "manualGate", "sync", "ring", "test"};
        const std::array<const char*, voiceFieldCount> names{"source", "manual frequency", "tuning", "pulse width", "attack", "decay",
            "sustain", "release", "triangle", "saw", "pulse", "noise", "manual hold", "sync", "ring", "test / reset"};
        for (int v = 0; v < 3; ++v)
            for (std::size_t f = 0; f < voiceFieldCount; ++f)
            {
                const auto field = static_cast<VoiceField>(f);
                auto& spec = s[voice(v, field)];
                spec = {"voice" + juce::String(v + 1) + "." + suffix[f], "Voice " + juce::String(v + 1) + " " + names[f],
                    0, 1, 0, 1, 3, f >= triangle ? Kind::boolean : Kind::integer, {}, {}};
                if (field == source) { spec.kind = Kind::choice; spec.choices = "MIDI|Manual"; }
                if (field == frequency) { spec.maximum = 65535; spec.initial = 7493; spec.unit = "reg"; }
                if (field == tuning) { spec.minimum = -96; spec.maximum = 96; spec.step = 0.01f; spec.kind = Kind::continuous; spec.unit = "st"; }
                if (field == pulseWidth) { spec.maximum = 4095; spec.initial = 2048; spec.unit = "reg"; }
                if (field >= attack && field <= release) { spec.maximum = 15; spec.unit = "reg"; }
                if (field == decay) spec.initial = 4;
                if (field == sustain) spec.initial = 12;
                if (field == release) spec.initial = 5;
                if (field == saw) spec.initial = 1;
            }
        return s;
    }();
    return result;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    for (const auto& s : specs())
    {
        const juce::ParameterID id{s.id, s.version};
        if (s.kind == Kind::boolean)
            result.add(std::make_unique<juce::AudioParameterBool>(id, s.name, s.initial != 0));
        else if (s.kind == Kind::choice)
            result.add(std::make_unique<juce::AudioParameterChoice>(id, s.name,
                juce::StringArray::fromTokens(s.choices, "|", ""), static_cast<int>(s.initial)));
        else if (s.kind == Kind::integer)
            result.add(std::make_unique<juce::AudioParameterInt>(id, s.name, static_cast<int>(s.minimum),
                static_cast<int>(s.maximum), static_cast<int>(s.initial), juce::AudioParameterIntAttributes().withLabel(s.unit)));
        else
            result.add(std::make_unique<juce::AudioParameterFloat>(id, s.name,
                juce::NormalisableRange<float>{s.minimum, s.maximum, s.step}, s.initial,
                juce::AudioParameterFloatAttributes().withLabel(s.unit)));
    }
    return result;
}
}
