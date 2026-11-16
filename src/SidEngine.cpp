// SPDX-License-Identifier: AGPL-3.0-only
#include "SidEngine.h"
#include "SidBackend.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sidsynth
{
SidEngine::SidEngine() : chip(std::make_unique<SidBackend>()) {}
SidEngine::~SidEngine() = default;

void SidEngine::prepare(double sampleRate)
{
    if (!std::isfinite(sampleRate) || sampleRate < 8000.0 || sampleRate > 192000.0)
        throw std::invalid_argument("SIDSynth supports sample rates from 8 to 192 kHz");
    prepared = false;
    chip->prepare(sampleRate);
    chip->configure(global.model8580, global.ntsc);
    dcCoefficient = std::exp(-2.0 * 3.14159265358979323846 * 5.0 / sampleRate);
    prepared = true;
    panic();
    for (int left = static_cast<int>(sampleRate); left > 0;)
    {
        const int count = std::min(left, static_cast<int>(scratch.size()));
        chip->clock(scratch.data(), count);
        previousInput = scratch[static_cast<std::size_t>(count - 1)] / 32768.0;
        left -= count;
    }
}

void SidEngine::panic() noexcept
{
    chip->reset();
    registers.fill(0);
    voices = {};
    for (int i = 0; i < 3; ++i)
    {
        writePatch(i);
        updateFrequency(i);
        writeControl(i, false);
    }
    writeGlobal();
    sounding = false;
    previousInput = previousOutput = 0.0;
}

void SidEngine::writePatch(int voice) noexcept
{
    const auto& patch = patches[static_cast<std::size_t>(voice)];
    const int base = voice * 7;
    writeRegister(base + 2, static_cast<unsigned char>(patch.pulseWidth & 0xff));
    writeRegister(base + 3, static_cast<unsigned char>((patch.pulseWidth >> 8) & 0x0f));
    writeRegister(base + 5, patch.attackDecay);
    writeRegister(base + 6, patch.sustainRelease);
}

void SidEngine::writeControl(int voice, bool gate) noexcept
{
    const auto& patch = patches[static_cast<std::size_t>(voice)];
    writeRegister(voice * 7 + 4, static_cast<unsigned char>(patch.waveform | (gate ? 1 : 0)
        | (patch.sync ? 2 : 0) | (patch.ring ? 4 : 0) | (patch.test ? 8 : 0)));
}

void SidEngine::updateFrequency(int voice) noexcept
{
    auto& current = voices[static_cast<std::size_t>(voice)];
    const auto& patch = patches[static_cast<std::size_t>(voice)];
    auto frequency = current.frequency;
    if (patch.manual) frequency = patch.manualFrequency;
    else if (current.note >= 0)
    {
        const double hz = 440.0 * std::exp2((current.note - 69 + patch.tuningSemitones + current.bend) / 12.0);
        frequency = static_cast<std::uint16_t>(std::lround(std::clamp(hz * 16777216.0 / clockFrequency(), 0.0, 65535.0)));
    }
    // Also write zero: retuning a held note below register resolution is valid.
    writeRegister(voice * 7, static_cast<unsigned char>(frequency & 0xff));
    writeRegister(voice * 7 + 1, static_cast<unsigned char>(frequency >> 8));
    current.frequency = frequency;
}

void SidEngine::setVoicePatch(int voice, VoicePatch patch) noexcept
{
    if (voice < 0 || voice >= 3 || !std::isfinite(patch.tuningSemitones)) return;
    patch.waveform &= 0xf0;
    patch.pulseWidth = std::min<std::uint16_t>(patch.pulseWidth, 4095);
    patch.tuningSemitones = std::clamp(patch.tuningSemitones, -96.0, 96.0);
    auto& saved = patches[static_cast<std::size_t>(voice)];
    auto& current = voices[static_cast<std::size_t>(voice)];
    const bool manualEdge = patch.manual && patch.manualGate && (!saved.manualGate || !saved.manual);
    if (patch.manual != saved.manual) { current.note = -1; current.frequency = 0; current.manualArmed = false; current.hardMuted = false; }
    if (!patch.manualGate) current.manualArmed = false;
    if (manualEdge) { current.manualArmed = true; current.muted = false; current.hardMuted = false; sounding = true; }
    saved = patch;
    writePatch(voice);
    updateFrequency(voice);
    if (current.hardMuted && !manualEdge)
    {
        // Do not undo CC120 on unrelated parameter changes.
        // Fresh/reset voices may still supply sync via their frequency register.
        return;
    }
    writeControl(voice, patch.manual ? current.manualArmed : current.note >= 0);
}

void SidEngine::writeGlobal() noexcept
{
    writeRegister(0x15, static_cast<unsigned char>(global.cutoff & 7));
    writeRegister(0x16, static_cast<unsigned char>(global.cutoff >> 3));
    writeRegister(0x17, static_cast<unsigned char>((global.resonance << 4) | global.routing));
    writeRegister(0x18, static_cast<unsigned char>(global.volume | (global.modes << 4) | (global.voice3Direct ? 0 : 0x80)));
}

bool SidEngine::setGlobalPatch(GlobalPatch patch) noexcept
{
    patch.cutoff = std::min<std::uint16_t>(patch.cutoff, 2047);
    patch.resonance &= 15; patch.routing &= 15; patch.modes &= 7; patch.volume &= 15;
    const bool changed = patch.model8580 != global.model8580 || patch.ntsc != global.ntsc;
    global = patch;
    if (prepared && changed) { chip->configure(global.model8580, global.ntsc); panic(); }
    else writeGlobal();
    return changed;
}

void SidEngine::setVoice(int voice, int note, double bendSemitones, bool retrigger) noexcept
{
    if (voice < 0 || voice >= 3 || note > 127 || !prepared || !std::isfinite(bendSemitones)) return;
    const auto& patch = patches[static_cast<std::size_t>(voice)];
    if (patch.manual) return;
    auto& current = voices[static_cast<std::size_t>(voice)];
    if (note < 0)
    {
        if (current.note >= 0) writeControl(voice, false);
        current.note = -1;
        return;
    }
    if (retrigger) writeControl(voice, false);
    const bool gateOn = current.note < 0 || current.muted || retrigger;
    current.note = note;
    current.bend = bendSemitones;
    updateFrequency(voice);
    if (gateOn) writeControl(voice, true);
    current.muted = false;
    current.hardMuted = false;
    sounding = true;
}

void SidEngine::silenceVoice(int voice) noexcept
{
    if (voice < 0 || voice >= 3) return;
    auto& current = voices[static_cast<std::size_t>(voice)];
    writeRegister(voice * 7 + 4, 0x08);
    current.note = -1;
    current.muted = true;
    current.hardMuted = true;
    current.manualArmed = false;
    if (std::all_of(voices.begin(), voices.end(), [](const auto& v) { return v.muted; }))
        panic();
}

void SidEngine::writeRegister(int address, unsigned char value) noexcept
{
    if (registers[static_cast<std::size_t>(address)] == value) return;
    registers[static_cast<std::size_t>(address)] = value;
    chip->write(address, value);
}
float SidEngine::filterSample(short sample) noexcept
{
    const double input = sample / 32768.0;
    const double filtered = input - previousInput + dcCoefficient * previousOutput;
    previousInput = input;
    previousOutput = filtered;
    return static_cast<float>(filtered);
}

void SidEngine::render(float* output, int samples, const float* inputLeft, const float* inputRight) noexcept
{
    if (!prepared) { std::fill_n(output, samples, 0.0f); return; }
    if (inputLeft != nullptr)
    {
        // Causal sample-and-hold upsampling: each host frame is held at every
        // SID cycle until the next output frame. Read before overwriting an
        // in-place input/output buffer; stereo input is averaged to mono.
        for (int i = 0; i < samples; ++i)
        {
            const double left = std::isfinite(inputLeft[i]) ? std::clamp(inputLeft[i], -1.0f, 1.0f) : 0.0;
            const double right = inputRight == nullptr ? left : (std::isfinite(inputRight[i]) ? std::clamp(inputRight[i], -1.0f, 1.0f) : 0.0);
            const auto input = static_cast<int>(std::lround((left + right) * 0.5 * 32767.0));
            if (input != 0) sounding = true;
            chip->input(input);
            if (sounding) { chip->clock(scratch.data(), 1); output[i] = filterSample(scratch[0]); }
            else output[i] = 0;
        }
        chip->input(0);
        return;
    }
    if (!sounding) { std::fill_n(output, samples, 0.0f); return; }
    while (samples > 0)
    {
        const int count = std::min(samples, static_cast<int>(scratch.size()));
        chip->clock(scratch.data(), count);
        for (int i = 0; i < count; ++i) output[i] = filterSample(scratch[static_cast<std::size_t>(i)]);
        output += count;
        samples -= count;
    }
}
}
