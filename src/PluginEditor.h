// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "PluginProcessor.h"

namespace sidsynth
{
class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor(PluginProcessor&);
    ~PluginEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    struct Rack;
    std::unique_ptr<Rack> rack;
    juce::TooltipWindow tooltips{this, 450};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};
}
