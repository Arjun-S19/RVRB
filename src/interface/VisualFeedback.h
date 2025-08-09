#pragma once

#include "../audio/DataStructures.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

// component for waveform and spectrogram display
class VisualFeedback : public juce::Component, private juce::Timer {
public:
  VisualFeedback();

  // enqueue audio data for visualisation
  void pushBuffer(const juce::AudioBuffer<float> &buffer);

  // render waveform and spectrogram
  void paint(juce::Graphics &g) override;
  // adjust component bounds
  void resized() override;

private:
  // periodic processing of queued audio
  void timerCallback() override;
  // draw next vertical line in spectrogram
  void drawNextLineOfSpectrogram();

  juce::AudioVisualiserComponent waveformVisualiser{1};
  juce::Image spectrogramImage;
  juce::dsp::FFT fft{9};
  std::vector<float> fifo;
  std::vector<float> fftData;
  int fifoIndex{0};
  AudioBlockQueue queue;
};
