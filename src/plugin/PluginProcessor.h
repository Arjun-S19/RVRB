#pragma once

class StateManager;

#include "../audio/DataStructures.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessorBase.h"
#include <atomic>
#include <functional>
#include <unordered_map>

// main audio processor handling dsp and preset logic
class PluginProcessor : public PluginProcessorBase {
public:
  PluginProcessor();
  ~PluginProcessor() override;

  // prepare dsp components
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  // process incoming audio and midi
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
  // reset internal state
  void reset() override;
  // save current state to memory block
  void getStateInformation(juce::MemoryBlock &destData) override;
  // restore state from memory block
  void setStateInformation(const void *data, int sizeInBytes) override;
  juce::AudioProcessorEditor *createEditor() override;
  std::unique_ptr<StateManager> state;

  // fetch processed audio for visualisation
  bool getNextAudioBlock(juce::AudioBuffer<float> &block) {
    return audioQueue.pop(block);
  }
  const std::vector<float> &getImpulseResponse() const {
    return impulseResponse;
  }
  void undoParameters();
  void redoParameters();
  void pushParameterState();
  void loadPreset(const juce::String &name);
  std::vector<juce::String> getCategoryNames() const;
  std::vector<juce::String> getPresetNames(const juce::String &category) const;
  void populatePresetMenu(juce::PopupMenu &menu,
                          std::unordered_map<int, juce::String> &idMap,
                          int &nextId) const;

private:
  juce::dsp::Reverb reverb;
  juce::dsp::DelayLine<float> preDelayLine{2 * 44100};
  double currentSampleRate{44100.0};

  std::vector<DelayBuffer> delayBuffers;
  std::vector<EarlyReflection> earlyReflections;
  AudioBlockQueue audioQueue;
  ParameterHistory parameterHistory;
  PresetManager presetManager;
  ReverbNetworkGraph reverbGraph;
  std::vector<float> impulseResponse;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
