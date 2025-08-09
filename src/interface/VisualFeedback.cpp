#include "VisualFeedback.h"
#include <algorithm>
#include <juce_audio_utils/juce_audio_utils.h>

// initialise visual components
VisualFeedback::VisualFeedback()
    : spectrogramImage(juce::Image::RGB, 512, 256, true), fifo(512, 0.0f),
      fftData(1024, 0.0f) {
  waveformVisualiser.setSamplesPerBlock(256);
  waveformVisualiser.setBufferSize(1024);
  addAndMakeVisible(waveformVisualiser);
  startTimerHz(60);
}

// queue incoming audio samples
void VisualFeedback::pushBuffer(const juce::AudioBuffer<float> &buffer) {
  waveformVisualiser.pushBuffer(buffer);
  queue.push(buffer);
}

// draw waveform and spectrogram
void VisualFeedback::paint(juce::Graphics &g) {
  g.fillAll(juce::Colours::black);
  g.drawImage(spectrogramImage,
              getLocalBounds().removeFromTop(getHeight() / 2).toFloat());
}

// layout child components
void VisualFeedback::resized() {
  auto area = getLocalBounds();
  waveformVisualiser.setBounds(area.removeFromBottom(area.getHeight() / 2));
}

// process queued audio and update spectrogram
void VisualFeedback::timerCallback() {
  juce::AudioBuffer<float> buffer;
  if (queue.pop(buffer)) {
    auto *channelData = buffer.getReadPointer(0);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
      auto sample = channelData[i];
      fifo[fifoIndex++] = sample;
      if (fifoIndex == fifo.size()) {
        std::fill(fftData.begin(), fftData.end(), 0.0f);
        std::copy(fifo.begin(), fifo.end(), fftData.begin());
        fft.performFrequencyOnlyForwardTransform(fftData.data());
        drawNextLineOfSpectrogram();
        fifoIndex = 0;
      }
    }
  }
}

// draw a column of spectral data
void VisualFeedback::drawNextLineOfSpectrogram() {
  auto rightHandEdge = spectrogramImage.getWidth() - 1;
  auto imageHeight = spectrogramImage.getHeight();

  spectrogramImage.moveImageSection(0, 0, 1, 0, rightHandEdge, imageHeight);

  for (int y = 0; y < imageHeight; ++y) {
    auto skewedProportionY =
        1.0f - std::exp(std::log(y / (float)imageHeight) * 0.2f);
    auto fftIndex = juce::jlimit(0, (int)fftData.size() / 2,
                                 (int)(skewedProportionY * fftData.size() / 2));
    auto level = juce::jmap(fftData[fftIndex], 0.0f, 10.0f, 0.0f, 1.0f);
    spectrogramImage.setPixelAt(
        rightHandEdge, y, juce::Colour::fromHSV(level, 1.0f, level, 1.0f));
  }

  repaint();
}
