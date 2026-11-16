// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <memory>
#include <cstdint>

namespace sidsynth
{
class SidBackend;
// One complete chip per engine. No JUCE/editor or saved performance state.
class SidEngine
{
public:
    struct VoicePatch
    {
        unsigned char waveform = 0x20;
        unsigned char attackDecay = 0x04;
        unsigned char sustainRelease = 0xc5;
        std::uint16_t pulseWidth = 2048;
        double tuningSemitones = 0.0;
        std::uint16_t manualFrequency = 7493;
        bool manual = false, manualGate = false, sync = false, ring = false, test = false;
    };
    struct GlobalPatch
    {
        std::uint16_t cutoff = 2047;
        unsigned char resonance = 0, routing = 0, modes = 0, volume = 15;
        bool model8580 = true, ntsc = false, voice3Direct = true;
    };
    static constexpr double palClock = 985248.0, ntscClock = 1022730.0;
    SidEngine();
    ~SidEngine();
    SidEngine(const SidEngine&) = delete;
    SidEngine& operator=(const SidEngine&) = delete;
    void prepare(double sampleRate);
    void panic() noexcept;
    void setVoice(int voice, int note, double bendSemitones = 0.0, bool retrigger = false) noexcept;
    void silenceVoice(int voice) noexcept;
    void setVoicePatch(int voice, VoicePatch) noexcept;
    // Returns true when model/clock change resets chip/performance state.
    bool setGlobalPatch(GlobalPatch) noexcept;
    const VoicePatch& voicePatch(int voice) const noexcept { return patches[static_cast<std::size_t>(voice)]; }
    const GlobalPatch& globalPatch() const noexcept { return global; }
    void render(float* output, int samples, const float* inputLeft = nullptr, const float* inputRight = nullptr) noexcept;
    int activeNote(int voice) const noexcept { return voices[static_cast<std::size_t>(voice)].note; }
    std::uint16_t frequencyRegister(int voice) const noexcept { return voices[static_cast<std::size_t>(voice)].frequency; }
    double clockFrequency() const noexcept { return global.ntsc ? ntscClock : palClock; }
    unsigned char registerValue(int address) const noexcept { return registers[static_cast<std::size_t>(address)]; }

private:
    std::unique_ptr<SidBackend> chip;
    std::array<short, 256> scratch{};
    struct Voice
    {
        int note = -1;
        double bend = 0.0;
        std::uint16_t frequency = 0;
        bool muted = true, manualArmed = false, hardMuted = false;
    };
    void writePatch(int voice) noexcept;
    void writeGlobal() noexcept;
    void writeControl(int voice, bool gate) noexcept;
    void updateFrequency(int voice) noexcept;
    void writeRegister(int address, unsigned char value) noexcept;
    float filterSample(short input) noexcept;
    std::array<unsigned char, 25> registers{};
    std::array<VoicePatch, 3> patches{};
    GlobalPatch global;
    std::array<Voice, 3> voices{};
    bool prepared = false, sounding = false;
    double dcCoefficient = 0.0, previousInput = 0.0, previousOutput = 0.0;
};
}
