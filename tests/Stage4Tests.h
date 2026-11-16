// SPDX-License-Identifier: AGPL-3.0-only
// Editor/processor integration, including concurrent host and audition ownership.
juce::Component* findControl(juce::Component& parent, const juce::String& id)
{
    if (parent.getComponentID() == id) return &parent;
    for (auto* child : parent.getChildren()) if (auto* result = findControl(*child, id)) return result;
    return nullptr;
}

void stage4AuditionTests()
{
    using namespace sidsynth::parameters;
    Processor p; p.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> audio(2, 256); juce::MidiBuffer midi;
    // Both parts share CH1, but preview explicitly targets V2 only.
    setParameter(p, channelIds[1], 1); process(p, audio, midi);
    p.audition(69, 2, p.auditionEpoch()); process(p, audio, midi);
    require(p.liveGates() == 2 && p.sidEngine().activeNote(1) == 69, "Audition target leaked through shared MIDI channels");
    midi.addEvent(MidiMessage::noteOn(1, 60, (juce::uint8)100), 113); process(p, audio, midi);
    require(p.sidEngine().activeNote(0) == 60 && p.sidEngine().activeNote(1) == 69, "Host MIDI stole preview or preview blocked another part");
    p.audition(-1, 0, p.auditionEpoch()); process(p, audio, midi);
    require(p.sidEngine().activeNote(1) == 60, "Audition release lost the host's held note");
    p.audition(72, 2, p.auditionEpoch()); process(p, audio, midi);
    midi.addEvent(MidiMessage::noteOff(1, 60), 83); process(p, audio, midi);
    require(p.liveGates() == 2, "Host note-off released the audition voice");
    p.audition(-1, 0, p.auditionEpoch()); process(p, audio, midi);
    require(p.liveGates() == 0, "Audition release resurrected a released host note");
    // Layered intent, parameter ownership and state restore/reset epochs.
    p.audition(65, 7, p.auditionEpoch()); process(p, audio, midi);
    require(p.liveGates() == 7, "Layered audition failed to play three oscillators");
    auto oldEpoch = p.auditionEpoch();
    juce::MemoryBlock patch; p.getStateInformation(patch);
    p.setStateInformation(patch.getData(), static_cast<int>(patch.getSize())); process(p, audio, midi);
    p.audition(65, 7, oldEpoch); process(p, audio, midi);
    require(p.liveGates() == 0 && audio.getMagnitude(0, 256) == 0, "Recall rearmed a stale keyboard request");
    p.audition(67, 7, p.auditionEpoch()); process(p, audio, midi);
    require(p.liveGates() == 7, "Fresh audition failed after recall");
    p.requestPanic(); process(p, audio, midi); process(p, audio, midi);
    require(p.liveGates() == 0 && audio.getMagnitude(0, 256) == 0, "Panic retained preview intent");
    p.audition(70, 7, p.auditionEpoch()); process(p, audio, midi);
    setParameter(p, "voice2.source", 1); process(p, audio, midi);
    require(p.liveGates() == 0, "Source edit retained preview");
    p.audition(70, 7, p.auditionEpoch()); process(p, audio, midi);
    require(p.liveGates() == 5, "Audition competed with a manual voice");
    midi.addEvent(MidiMessage::allSoundOff(1), 30); process(p, audio, midi);
    require(p.liveGates() == 0, "CC120 retained audition");
    setParameter(p, "voice2.source", 0); process(p, audio, midi);
    p.audition(70, 1, p.auditionEpoch()); process(p, audio, midi);
    setParameter(p, "chip.clock", 1); process(p, audio, midi);
    require(p.liveGates() == 0, "Clock edit retained preview");
    p.audition(70, 1, p.auditionEpoch()); process(p, audio, midi);
    setParameter(p, "voice1.midiChannel", 16); process(p, audio, midi);
    require(p.liveGates() == 0, "Routing edit retained preview");
}

void stage4EditorTests(const char* previewPath)
{
    using namespace sidsynth::parameters;
    Processor p; p.prepareToPlay(48000, 256);
    juce::AudioBuffer<float> audio(2, 256); juce::MidiBuffer midi;
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    // All 67 real Rack controls write the processor, and host edits return to
    // those same widgets. This catches swapped/missing attachments.
    for (std::size_t key = 0; key < count; ++key)
    {
        const auto& s = specs()[key];
        auto* component = findControl(*editor, s.id);
        require(component != nullptr, "Rack is missing a saved parameter");
        const float targetValue = s.initial == s.maximum ? s.minimum : s.maximum;
        if (auto* slider = dynamic_cast<juce::Slider*>(component)) slider->setValue(targetValue, juce::sendNotificationSync);
        else if (auto* combo = dynamic_cast<juce::ComboBox*>(component)) combo->setSelectedItemIndex(static_cast<int>(targetValue - s.minimum), juce::sendNotificationSync);
        else if (auto* button = dynamic_cast<juce::Button*>(component)) button->setToggleState(targetValue > 0, juce::sendNotificationSync);
        require(std::abs(p.state.getRawParameterValue(s.id)->load() - targetValue) < .001f, "Rack edit did not reach its parameter");
        setParameter(p, s.id.toRawUTF8(), s.initial);
        if (auto* slider = dynamic_cast<juce::Slider*>(component)) require(std::abs(slider->getValue() - s.initial) < .001, "Host numeric edit did not reach Rack");
        else if (auto* combo = dynamic_cast<juce::ComboBox*>(component)) require(combo->getSelectedItemIndex() == static_cast<int>(s.initial - s.minimum), "Host choice edit did not reach Rack");
        else if (auto* button = dynamic_cast<juce::Button*>(component)) require(button->getToggleState() == (s.initial > 0), "Host switch edit did not reach Rack");
    }
    auto* cutoffSlider = dynamic_cast<juce::Slider*>(findControl(*editor, "filter.cutoff"));
    juce::Label* number = nullptr;
    for (auto* child : cutoffSlider->getChildren()) if (auto* l = dynamic_cast<juce::Label*>(child)) number = l;
    require(number != nullptr && number->isEditable(), "Numeric entry is unavailable");
    number->setText("820", juce::sendNotificationSync);
    require(p.state.getRawParameterValue("filter.cutoff")->load() == 820, "Typed register value failed");
    number->setText("9999", juce::sendNotificationSync);
    require(p.state.getRawParameterValue("filter.cutoff")->load() == 2047, "Typed register value escaped hardware range");
    process(p, audio, midi);
    p.audition(69, 1, p.auditionEpoch()); process(p, audio, midi);
    editor.reset(); process(p, audio, midi);
    require(p.liveGates() == 0, "Closing editor left audition held");
    editor.reset(p.createEditor());
    auto* advanced = dynamic_cast<juce::TextButton*>(findControl(*editor, "advanced"));
    auto* popup = findControl(*editor, "advanced.panel");
    require(advanced && popup && !popup->isVisible(), "Advanced controls are not discoverable");
    advanced->onClick(); require(popup->isVisible(), "Advanced hardware did not open");
    if (previewPath)
    {
        const juce::File base{juce::String(previewPath)};
        auto save = [&](const juce::String& suffix)
        {
            auto path = base.getSiblingFile(base.getFileNameWithoutExtension() + suffix + ".png");
            auto stream = path.createOutputStream(); require(stream != nullptr, "Cannot write Rack preview");
            stream->setPosition(0); stream->truncate();
            require(juce::PNGImageFormat().writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()), *stream), "Cannot render Rack preview");
        };
        save("-advanced");
        popup->setVisible(false);
        for (const auto scale : {.8, 1.0, 1.25, 1.5})
        {
            editor->setSize(juce::roundToInt(1120 * scale), juce::roundToInt(780 * scale));
            save("-" + juce::String(juce::roundToInt(scale * 100)));
        }
    }
}
