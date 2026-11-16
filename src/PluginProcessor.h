// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "Parameters.h"
#include "SidEngine.h"
#include "MidiRouter.h"
#include <atomic>

namespace sidsynth
{
class PluginProcessor final : public juce::AudioProcessor
{
public:
    PluginProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "SIDSynth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    // The slowest SID release takes roughly 24 seconds at the PAL clock.
    // Leave margin for the ADSR delay bug and the shared filter/DC-block tail.
    double getTailLengthSeconds() const override { return 30.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "First SID"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    void requestPanic() noexcept { panicRequested.store(true, std::memory_order_release); }
    // Message-thread audition intent. Epochs prevent pre-reset held keys from
    // resurrecting notes. The audio thread never reads keyboard/UI objects.
    unsigned auditionEpoch() const noexcept { return previewEpoch.load(std::memory_order_acquire); }
    void audition(int note, unsigned voiceMask, unsigned epoch) noexcept;
    unsigned liveGates() const noexcept { return gateSnapshot.load(std::memory_order_relaxed); }
    // Audio-thread diagnostics used by the offline behavior checks.
    const SidEngine& sidEngine() const noexcept { return engine; }
    juce::AudioProcessorValueTreeState state;

private:
    MidiConfiguration midiConfiguration() const noexcept;
    void clearPerformance() noexcept;
    void clearAudition() noexcept;
    void publishGates() noexcept;
    float value(std::size_t key) const noexcept { return values[key]->load(std::memory_order_relaxed); }
    void applyPatch() noexcept;
    SidEngine engine;
    MidiRouter router;
    std::array<std::atomic<float>*, parameters::count> values{};
    std::atomic<bool> panicRequested{false};
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    std::atomic<std::uint64_t> previewRequest{0};
    std::atomic<unsigned> previewEpoch{0}, gateSnapshot{0};
    unsigned activePreview = 0;
    juce::SmoothedValue<float> gain;
    bool wasPlaying = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
}
