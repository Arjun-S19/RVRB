#pragma once

#include "../interface/VisualFeedback.h"
#include "../parameters/ParameterDefines.h"
#include "PluginProcessor.h"
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

// look and feel for rotary sliders
class CleanLookAndFeel : public juce::LookAndFeel_V4 {
public:
  void drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height,
                        float sliderPosProportional, float rotaryStartAngle,
                        float rotaryEndAngle, juce::Slider &slider) override;
};

// main editor component for the plugin
class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer {
public:
  explicit AudioPluginAudioProcessorEditor(PluginProcessor &);
  ~AudioPluginAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  // refresh ui elements from processor state
  void timerCallback() override;
  // configure a slider and its label
  void setupSlider(juce::Slider &slider, juce::Label &label,
                   const juce::String &name, size_t paramId);

  PluginProcessor &processorRef;
  StateManager *state{nullptr};

  CleanLookAndFeel lookAndFeel;

  juce::Slider mixSlider, roomSizeSlider, decaySlider, dampingSlider,
      widthSlider, preDelaySlider;
  juce::Label mixLabel, roomSizeLabel, decayLabel, dampingLabel, widthLabel,
      preDelayLabel;
  juce::TextButton presetButton{"Presets"};
  juce::TextButton undoButton{"<"}, redoButton{">"};
  VisualFeedback visual;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
