// SPDX-License-Identifier: AGPL-3.0-only
#include "MidiRouter.h"
#include <algorithm>
#include <limits>

namespace sidsynth
{
void MidiRouter::Channel::remove(int note) noexcept
{
    for (int i = 0; i < size; ++i)
        if (order[static_cast<std::size_t>(i)] == note)
        {
            for (int j = i + 1; j < size; ++j)
                order[static_cast<std::size_t>(j - 1)] = order[static_cast<std::size_t>(j)];
            --size;
            return;
        }
}

void MidiRouter::Channel::releaseSustained() noexcept
{
    for (int i = size - 1; i >= 0; --i)
        if (held[static_cast<std::size_t>(order[static_cast<std::size_t>(i)])] == 0)
            remove(order[static_cast<std::size_t>(i)]);
}

bool MidiRouter::configure(const MidiConfiguration& requested) noexcept
{
    auto next = requested;
    if (next.mode != PlayMode::layered) next.mode = PlayMode::threeParts;
    next.layeredChannel = std::clamp(next.layeredChannel, 1, 16);
    for (auto& channel : next.channels) channel = std::clamp(channel, 1, 16);
    if (configuration == next)
        return false;
    configuration = next;
    reset();
    return true;
}

void MidiRouter::reset() noexcept { channels = {}; }

unsigned MidiRouter::maskForChannel(int channel) const noexcept
{
    if (configuration.mode == PlayMode::layered)
        return configuration.layeredChannel == channel ? 7u : 0u;
    unsigned mask = 0;
    for (int i = 0; i < 3; ++i)
        if (configuration.channels[static_cast<std::size_t>(i)] == channel)
            mask |= 1u << i;
    return mask;
}

MidiRouter::Voice MidiRouter::voice(int index) const noexcept
{
    const int channel = configuration.mode == PlayMode::layered
        ? configuration.layeredChannel : configuration.channels[static_cast<std::size_t>(index)];
    const auto& state = channels[static_cast<std::size_t>(channel - 1)];
    const double bend = (state.wheel - 8192) * (state.wheel < 8192 ? 2.0 / 8192.0 : 2.0 / 8191.0);
    return {state.size == 0 ? -1 : state.order[static_cast<std::size_t>(state.size - 1)], bend};
}

MidiRouter::Result MidiRouter::handle(const std::uint8_t* data, int size) noexcept
{
    Result result;
    if (size < 1 || data == nullptr)
        return result;
    if (data[0] == 0xff)
    {
        reset();
        result.systemReset = true;
        return result;
    }
    if (data[0] < 0x80 || data[0] >= 0xf0 || size < 3 || data[1] > 127 || data[2] > 127)
        return result;
    const int channel = (data[0] & 0x0f) + 1;
    const unsigned mask = maskForChannel(channel);
    if (mask == 0)
        return result;
    auto& state = channels[static_cast<std::size_t>(channel - 1)];
    const int kind = data[0] & 0xf0;
    const int note = data[1];
    auto& count = state.held[static_cast<std::size_t>(note)];
    if (kind == 0x90 && data[2] != 0)
    {
        // Saturate rather than wrapping after an unreasonable number of repeats.
        if (count == std::numeric_limits<std::uint32_t>::max())
            return result;
        ++count;
        state.remove(note);
        state.order[static_cast<std::size_t>(state.size++)] = note;
        result.retriggerMask = mask;
    }
    else if (kind == 0x80 || (kind == 0x90 && data[2] == 0))
    {
        if (count > 0 && --count == 0 && !state.sustain)
            state.remove(note);
    }
    else if (kind == 0xe0)
        state.wheel = data[1] | (data[2] << 7);
    else if (kind == 0xb0)
    {
        switch (data[1])
        {
            case 64:
                state.sustain = data[2] >= 64;
                if (!state.sustain) state.releaseSustained();
                break;
            case 120:
                state.held.fill(0);
                state.size = 0;
                state.sustain = false;
                result.hardMuteMask = mask;
                break;
            case 121:
                state.wheel = 8192;
                state.sustain = false;
                state.releaseSustained();
                break;
            case 123:
                state.held.fill(0);
                if (!state.sustain) state.size = 0;
                break;
            default: break;
        }
    }
    return result;
}
}
