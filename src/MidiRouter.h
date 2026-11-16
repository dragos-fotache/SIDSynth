// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <array>
#include <cstdint>

namespace sidsynth
{
enum class PlayMode { threeParts = 0, layered = 1 };

struct MidiConfiguration
{
    PlayMode mode = PlayMode::threeParts;
    std::array<int, 3> channels{1, 2, 3};
    int layeredChannel = 1;
    bool operator==(const MidiConfiguration& other) const noexcept
    {
        return mode == other.mode && channels == other.channels && layeredChannel == other.layeredChannel;
    }
};

// Fixed storage, bounded work, and no dependency on JUCE, the editor or the chip.
class MidiRouter
{
public:
    struct Voice { int note; double bendSemitones; };
    struct Result
    {
        unsigned retriggerMask = 0;
        unsigned hardMuteMask = 0;
        bool systemReset = false;
    };
    bool configure(const MidiConfiguration&) noexcept;
    void reset() noexcept;
    Result handle(const std::uint8_t* data, int size) noexcept;
    Voice voice(int index) const noexcept;

private:
    struct Channel
    {
        std::array<std::uint32_t, 128> held{};
        std::array<int, 128> order{};
        int size = 0;
        int wheel = 8192;
        bool sustain = false;
        void remove(int note) noexcept;
        void releaseSustained() noexcept;
    };
    unsigned maskForChannel(int channel) const noexcept;
    MidiConfiguration configuration;
    std::array<Channel, 16> channels{};
};
}
