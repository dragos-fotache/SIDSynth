// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include <memory>
namespace reSIDfp { class SID; }
namespace sidsynth
{
// Narrow adapter for the pinned core. Exactly one SID; prebuilt configurations
// contain only lookup tables and resamplers, never extra oscillators/filters.
class SidBackend
{
public:
    SidBackend();
    ~SidBackend();
    void prepare(double sampleRate);
    void configure(bool model8580, bool ntsc) noexcept;
    void reset() noexcept;
    void write(int address, unsigned char value) noexcept;
    void input(int value) noexcept;
    void clock(short* output, int count) noexcept;
private:
    struct Cache;
    std::unique_ptr<Cache> cache;
    std::unique_ptr<reSIDfp::SID> chip;
};
}
