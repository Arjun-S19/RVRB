#include "PluginEditor.h"
#include "../parameters/StateManager.h"
#include <unordered_map>

// rotary slider graphic
void CleanLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y,
                                        int width, int height,
                                        float sliderPosProportional,
                                        float rotaryStartAngle,
                                        float rotaryEndAngle,
                                        juce::Slider &slider) {
  auto radius = juce::jmin(width, height) / 2.0f - 4.0f;
  auto centreX = x + width * 0.5f;
  auto centreY = y + height * 0.5f;
  auto angle = rotaryStartAngle +
               sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

  g.setColour(juce::Colours::darkgrey);
  g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f,
                radius * 2.0f);
  g.setColour(juce::Colours::silver);
  g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f,
                radius * 2.0f, 2.0f);

  juce::Path p;
  p.addRectangle(centreX - 2.0f, centreY - radius, 4.0f, radius);
  juce::AffineTransform t =
      juce::AffineTransform::rotation(angle, centreX, centreY);
  p.applyTransform(t);
  g.setColour(juce::Colours::white);
  g.fillPath(p);
}

// set up controls and callbacks
AudioPluginAudioProcessorEditor::AudioPluginAudioProcessorEditor(
    PluginProcessor &p)
    : AudioProcessorEditor(&p), processorRef(p) {
  setSize(600, 400);
  state = processorRef.state.get();

  setupSlider(mixSlider, mixLabel, "Mix", PARAM::MIX);
  setupSlider(roomSizeSlider, roomSizeLabel, "Room", PARAM::ROOM_SIZE);
  setupSlider(decaySlider, decayLabel, "Decay", PARAM::DECAY);
  setupSlider(dampingSlider, dampingLabel, "Damp", PARAM::DAMPING);
  setupSlider(widthSlider, widthLabel, "Width", PARAM::WIDTH);
  setupSlider(preDelaySlider, preDelayLabel, "PreDelay", PARAM::PRE_DELAY);

  presetButton.onClick = [this] {
    juce::PopupMenu menu;
    std::unordered_map<int, juce::String> idMap;
    int nextId = 1;
    processorRef.populatePresetMenu(menu, idMap, nextId);
    menu.showMenuAsync(juce::PopupMenu::Options(),
                       [this, idMap](int result) mutable {
                         auto it = idMap.find(result);
                         if (it != idMap.end())
                           processorRef.loadPreset(it->second);
                       });
  };
  addAndMakeVisible(presetButton);

  undoButton.onClick = [this] { processorRef.undoParameters(); };
  redoButton.onClick = [this] { processorRef.redoParameters(); };
  addAndMakeVisible(undoButton);
  addAndMakeVisible(redoButton);
  addAndMakeVisible(visual);

  startTimerHz(60);
}

// release look and feel assignments
AudioPluginAudioProcessorEditor::~AudioPluginAudioProcessorEditor() {
  mixSlider.setLookAndFeel(nullptr);
  roomSizeSlider.setLookAndFeel(nullptr);
  decaySlider.setLookAndFeel(nullptr);
  dampingSlider.setLookAndFeel(nullptr);
  widthSlider.setLookAndFeel(nullptr);
  preDelaySlider.setLookAndFeel(nullptr);
}

// draw background and title
void AudioPluginAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff202020));
  g.setColour(juce::Colours::white);
  g.setFont(juce::Font("Coolvetica", 50.0f, juce::Font::bold));
  auto topBarBounds = getLocalBounds().reduced(20).removeFromTop(40);
  g.drawFittedText("RVRB", 20, topBarBounds.getY(), 100,
                   topBarBounds.getHeight(), juce::Justification::centredLeft,
                   1);
}

// position controls
void AudioPluginAudioProcessorEditor::resized() {
  auto area = getLocalBounds().reduced(20);
  auto topBar = area.removeFromTop(40);
  topBar.removeFromLeft(80);
  presetButton.setBounds(topBar.removeFromRight(120));
  redoButton.setBounds(topBar.removeFromRight(40));
  undoButton.setBounds(topBar.removeFromRight(40));
  auto sliderArea = area.removeFromBottom(170);
  auto w = sliderArea.getWidth() / 6;
  mixSlider.setBounds(sliderArea.removeFromLeft(w));
  roomSizeSlider.setBounds(sliderArea.removeFromLeft(w));
  decaySlider.setBounds(sliderArea.removeFromLeft(w));
  dampingSlider.setBounds(sliderArea.removeFromLeft(w));
  widthSlider.setBounds(sliderArea.removeFromLeft(w));
  preDelaySlider.setBounds(sliderArea);

  auto labelHeight = 20;
  const int labelOffset = -40;
  mixLabel.setBounds(mixSlider.getX(), mixSlider.getBottom() + labelOffset,
                     mixSlider.getWidth(), labelHeight);
  roomSizeLabel.setBounds(roomSizeSlider.getX(),
                          roomSizeSlider.getBottom() + labelOffset,
                          roomSizeSlider.getWidth(), labelHeight);
  decayLabel.setBounds(decaySlider.getX(),
                       decaySlider.getBottom() + labelOffset,
                       decaySlider.getWidth(), labelHeight);
  dampingLabel.setBounds(dampingSlider.getX(),
                         dampingSlider.getBottom() + labelOffset,
                         dampingSlider.getWidth(), labelHeight);
  widthLabel.setBounds(widthSlider.getX(),
                       widthSlider.getBottom() + labelOffset,
                       widthSlider.getWidth(), labelHeight);
  preDelayLabel.setBounds(preDelaySlider.getX(),
                          preDelaySlider.getBottom() + labelOffset,
                          preDelaySlider.getWidth(), labelHeight);

  visual.setBounds(area.reduced(10));
}

// update ui from audio processor
void AudioPluginAudioProcessorEditor::timerCallback() {
  mixSlider.setValue(state->param_value(PARAM::MIX),
                     juce::dontSendNotification);
  roomSizeSlider.setValue(state->param_value(PARAM::ROOM_SIZE),
                          juce::dontSendNotification);
  decaySlider.setValue(state->param_value(PARAM::DECAY),
                       juce::dontSendNotification);
  dampingSlider.setValue(state->param_value(PARAM::DAMPING),
                         juce::dontSendNotification);
  widthSlider.setValue(state->param_value(PARAM::WIDTH),
                       juce::dontSendNotification);
  preDelaySlider.setValue(state->param_value(PARAM::PRE_DELAY),
                          juce::dontSendNotification);

  juce::AudioBuffer<float> block;
  int processed = 0;
  const int maxBlocks = 2;
  while (processed < maxBlocks && processorRef.getNextAudioBlock(block)) {
    visual.pushBuffer(block);
    ++processed;
  }
}

// configure a slider for parameter control
void AudioPluginAudioProcessorEditor::setupSlider(juce::Slider &slider,
                                                  juce::Label &label,
                                                  const juce::String &name,
                                                  size_t paramId) {
  slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
  slider.setRange(PARAMETER_RANGES[paramId].start,
                  PARAMETER_RANGES[paramId].end, 0.1);
  slider.setValue(state->param_value(paramId));
  slider.setLookAndFeel(&lookAndFeel);
  slider.setNumDecimalPlacesToDisplay(1);
  slider.onValueChange = [this, paramId, &slider] {
    state->set_parameter(paramId, static_cast<float>(slider.getValue()));
  };
  slider.onDragEnd = [this] { processorRef.pushParameterState(); };
  addAndMakeVisible(slider);

  label.setText(name, juce::dontSendNotification);
  label.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(label);
}
