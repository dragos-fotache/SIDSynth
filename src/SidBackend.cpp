// SPDX-License-Identifier: AGPL-3.0-only
#include "SidBackend.h"
#include "SID.h"
#include "Filter6581.h"
#include "Filter8580.h"
#include "WaveformCalculator.h"
#include "resample/Resampler.h"
#include <array>
#include <algorithm>

namespace sidsynth
{
struct SidBackend::Cache
{
    struct Model
    {
        std::array<float, 256> envelope{};
        std::array<float, 4096> oscillator{};
        rc_matrix_t pulldown;
        int scale = 0, ttl = 0;
    };
    std::array<Model, 2> models;
    rc_matrix_t waveform;
    std::array<std::unique_ptr<reSIDfp::Resampler>, 2> resamplers;
    int activeClock = -1;
    bool active8580 = true;
};

SidBackend::SidBackend() : cache(std::make_unique<Cache>()), chip(std::make_unique<reSIDfp::SID>())
{
    // Upstream setChipModel builds DAC arrays and locks a shared waveform cache.
    // Call it only here, retaining both configurations for audio-thread copies.
    auto* calculator = reSIDfp::WaveformCalculator::getInstance();
    cache->waveform = calculator->getWaveTable();
    for (int i = 0; i < 2; ++i)
    {
        const auto model = i == 0 ? reSIDfp::MOS6581 : reSIDfp::CSG8580;
        chip->setChipModel(model);
        auto& saved = cache->models[static_cast<std::size_t>(i)];
        std::copy_n(chip->envDAC, saved.envelope.size(), saved.envelope.begin());
        std::copy_n(chip->oscDAC, saved.oscillator.size(), saved.oscillator.begin());
        saved.pulldown = calculator->buildPulldownTable(model, reSIDfp::AVERAGE);
        saved.scale = chip->scaleFactor;
        saved.ttl = chip->modelTTL;
    }
    chip->setNoiseLevel(1); // Upstream level 0 has an undefined negative shift.
}
SidBackend::~SidBackend() = default;

void SidBackend::prepare(double sampleRate)
{
    for (int i = 0; i < 2; ++i)
    {
        chip->setSamplingParameters(i == 0 ? 985248.0 : 1022730.0, reSIDfp::RESAMPLE, sampleRate);
        cache->resamplers[static_cast<std::size_t>(i)] = std::move(chip->resampler);
    }
    cache->activeClock = -1;
    configure(cache->active8580, false);
}

void SidBackend::configure(bool model8580, bool ntsc) noexcept
{
    if (cache->active8580 != model8580)
    {
        const auto& saved = cache->models[model8580 ? 1 : 0];
        chip->model = model8580 ? reSIDfp::CSG8580 : reSIDfp::MOS6581;
        chip->filter = model8580 ? static_cast<reSIDfp::Filter*>(chip->filter8580) : static_cast<reSIDfp::Filter*>(chip->filter6581);
        chip->scaleFactor = saved.scale;
        chip->modelTTL = saved.ttl;
        std::copy(saved.envelope.begin(), saved.envelope.end(), chip->envDAC);
        std::copy(saved.oscillator.begin(), saved.oscillator.end(), chip->oscDAC);
        for (auto& voice : chip->voice)
        {
            voice.wave()->setModel(!model8580);
            voice.wave()->setWaveformModels(cache->waveform);
            voice.wave()->setPulldownModels(saved.pulldown);
        }
        chip->filter->restart();
        cache->active8580 = model8580;
    }
    const int next = ntsc ? 1 : 0;
    if (cache->activeClock != next)
    {
        if (cache->activeClock >= 0)
            chip->resampler.swap(cache->resamplers[static_cast<std::size_t>(cache->activeClock)]);
        chip->resampler.swap(cache->resamplers[static_cast<std::size_t>(next)]);
        chip->externalFilter.setClockFrequency(ntsc ? 1022730.0 : 985248.0);
        cache->activeClock = next;
    }
}
void SidBackend::reset() noexcept { chip->input(0); chip->reset(); }
void SidBackend::write(int address, unsigned char value) noexcept { chip->write(address, value); }
void SidBackend::input(int value) noexcept { chip->input(value); }
void SidBackend::clock(short* output, int count) noexcept { if (count > 0) chip->clock(output, count); }
}
