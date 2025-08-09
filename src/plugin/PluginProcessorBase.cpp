#include "PluginProcessorBase.h"

// set up default bus configuration
PluginProcessorBase::PluginProcessorBase()
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
#if NEEDS_SIDECHAIN
              .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
      ) {
}

// allow default cleanup
PluginProcessorBase::~PluginProcessorBase() {}

// plugin name
const juce::String PluginProcessorBase::getName() const {
  return JucePlugin_Name;
}

// midi input capability
bool PluginProcessorBase::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

// midi output capability
bool PluginProcessorBase::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

// checks if it's a midi effect
bool PluginProcessorBase::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

// reported tail length
double PluginProcessorBase::getTailLengthSeconds() const { return 10.0; }

int PluginProcessorBase::getNumPrograms() { return 1; }

int PluginProcessorBase::getCurrentProgram() { return 0; }

void PluginProcessorBase::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String PluginProcessorBase::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void PluginProcessorBase::changeProgramName(int index,
                                            const juce::String &newName) {
  juce::ignoreUnused(index, newName);
}

// release any resources
void PluginProcessorBase::releaseResources() {}

// validate supported bus layouts
bool PluginProcessorBase::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

#if !JucePlugin_IsSynth
#if NEEDS_SIDECHAIN
  if (layouts.getChannelSet(true, 1) != layouts.getMainInputChannelSet()) {
    return false;
  }
#endif
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#else
#if NEEDS_SIDECHAIN
  if (layouts.getChannelSet(true, 0) != layouts.getMainOutputChannelSet()) {
    return false;
  }
#endif
#endif

  return true;
#endif
}

// always provide an editor
bool PluginProcessorBase::hasEditor() const { return true; }
