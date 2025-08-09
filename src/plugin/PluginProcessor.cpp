#include "PluginProcessor.h"
#include "../parameters/StateManager.h"
#include "PluginEditor.h"

// set up default parameters and presets
PluginProcessor::PluginProcessor() {
  state = std::make_unique<StateManager>(this);

  ParameterSet defaultPreset{0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f};
  presetManager.addPreset("Init", defaultPreset);
  presetManager.addPreset("Classic", {0.3f, 0.4f, 0.3f, 0.4f, 0.5f, 10.0f},
                          {"Hall", "Small"});
  presetManager.addPreset("Warm", {0.35f, 0.45f, 0.35f, 0.5f, 0.55f, 12.0f},
                          {"Hall", "Small"});
  presetManager.addPreset("Bright", {0.25f, 0.35f, 0.25f, 0.3f, 0.6f, 8.0f},
                          {"Hall", "Small"});
  presetManager.addPreset("Classic", {0.6f, 0.8f, 0.6f, 0.6f, 0.8f, 40.0f},
                          {"Hall", "Large"});
  presetManager.addPreset("Wide", {0.6f, 0.85f, 0.65f, 0.55f, 0.9f, 45.0f},
                          {"Hall", "Large"});
  presetManager.addPreset("Deep", {0.65f, 0.9f, 0.7f, 0.6f, 0.85f, 50.0f},
                          {"Hall", "Large"});
  presetManager.addPreset("Classic", {0.7f, 0.9f, 0.8f, 0.5f, 0.9f, 60.0f},
                          {"Hall", "Cathedral"});
  presetManager.addPreset("Grand", {0.75f, 0.95f, 0.85f, 0.5f, 1.0f, 65.0f},
                          {"Hall", "Cathedral"});
  presetManager.addPreset("Mystic", {0.7f, 0.9f, 0.8f, 0.6f, 0.95f, 70.0f},
                          {"Hall", "Cathedral"});
  presetManager.addPreset("Classic", {0.5f, 0.5f, 0.7f, 0.2f, 0.9f, 5.0f},
                          {"Plate", "Vintage"});
  presetManager.addPreset("Warm", {0.55f, 0.5f, 0.75f, 0.25f, 0.85f, 7.0f},
                          {"Plate", "Vintage"});
  presetManager.addPreset("Bright", {0.45f, 0.55f, 0.65f, 0.15f, 0.95f, 4.0f},
                          {"Plate", "Vintage"});
  presetManager.addPreset("Classic", {0.4f, 0.6f, 0.5f, 0.3f, 1.0f, 15.0f},
                          {"Plate", "Modern"});
  presetManager.addPreset("Smooth", {0.45f, 0.65f, 0.55f, 0.35f, 0.95f, 18.0f},
                          {"Plate", "Modern"});
  presetManager.addPreset("Shimmer", {0.5f, 0.7f, 0.6f, 0.2f, 1.0f, 20.0f},
                          {"Plate", "Modern"});
}

// allow default cleanup
PluginProcessor::~PluginProcessor() {}

// configure dsp modules and allocate buffers
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  currentSampleRate = sampleRate;
  juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32)samplesPerBlock,
                              (juce::uint32)getTotalNumOutputChannels()};
  reverb.prepare(spec);
  preDelayLine.prepare(spec);
  preDelayLine.setDelay(0.0f);

  delayBuffers.clear();
  delayBuffers.resize(getTotalNumOutputChannels(),
                      DelayBuffer((size_t)(sampleRate * 2)));

  earlyReflections.clear();
  earlyReflections.push_back({(size_t)(0.01 * sampleRate), 0.5f});
  earlyReflections.push_back({(size_t)(0.02 * sampleRate), 0.3f});

  pushParameterState();

  reverbGraph = ReverbNetworkGraph{};
  reverbGraph.addEdge(0, 1, 0.7f, 0.3f);
  reverbGraph.addEdge(1, 2, 0.7f, 0.2f);
  reverbGraph.addEdge(2, 3, 0.7f, 0.1f);
  if (reverbGraph.hasCycle())
    juce::Logger::writeToLog("Reverb network has feedback cycle");

  juce::AudioBuffer<float> irBuf(1, 2048);
  irBuf.clear();
  irBuf.setSample(0, 0, 1.0f);
  juce::dsp::AudioBlock<float> irBlock(irBuf);
  juce::dsp::ProcessContextReplacing<float> irContext(irBlock);
  preDelayLine.process(irContext);
  reverb.process(irContext);
  impulseResponse.resize(irBuf.getNumSamples());
  for (int i = 0; i < irBuf.getNumSamples(); ++i)
    impulseResponse[i] = irBuf.getSample(0, i);
}

// main audio processing routine
void PluginProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                   juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  auto mix = state->param_value(PARAM::MIX) / 100.0f;
  auto roomSize = state->param_value(PARAM::ROOM_SIZE) / 100.0f;
  auto decay = state->param_value(PARAM::DECAY) / 100.0f;
  auto damping = state->param_value(PARAM::DAMPING) / 100.0f;
  auto width = state->param_value(PARAM::WIDTH) / 100.0f;
  auto preDelayMs = state->param_value(PARAM::PRE_DELAY);

  juce::Reverb::Parameters params;
  params.roomSize = roomSize;
  params.damping = damping;
  params.width = width;
  params.dryLevel = 1.0f - mix;
  params.wetLevel = mix * decay;
  reverb.setParameters(params);

  auto preDelaySamples =
      preDelayMs * 0.001f * static_cast<float>(currentSampleRate);
  preDelayLine.setDelay(preDelaySamples);

  for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
    auto *data = buffer.getWritePointer(ch);
    for (int i = 0; i < buffer.getNumSamples(); ++i) {
      float input = data[i];
      delayBuffers[ch].push(input);
      float reflections = 0.0f;
      for (auto &tap : earlyReflections)
        reflections +=
            delayBuffers[ch].getDelayed(tap.delaySamples) * tap.amplitude;
      data[i] = input + reflections;
    }
  }

  juce::dsp::AudioBlock<float> block(buffer);
  juce::dsp::ProcessContextReplacing<float> context(block);
  preDelayLine.process(context);
  reverb.process(context);
  audioQueue.push(buffer);
  midiMessages.clear();
}

// reset internal dsp state
void PluginProcessor::reset() {
  reverb.reset();
  preDelayLine.reset();
  for (auto &db : delayBuffers)
    db.resize((size_t)(currentSampleRate * 2));
}

// store current state
void PluginProcessor::getStateInformation(juce::MemoryBlock &destData) {
  auto plugin_state = state->get_state();
  std::unique_ptr<juce::XmlElement> xml(plugin_state.createXml());
  copyXmlToBinary(*xml, destData);
}

// restore saved state
void PluginProcessor::setStateInformation(const void *data, int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));
  state->load_from(xmlState.get());
}

// create plugin editor instance
juce::AudioProcessorEditor *PluginProcessor::createEditor() {
  return new AudioPluginAudioProcessorEditor(*this);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new PluginProcessor();
}

// revert to previous parameter state
void PluginProcessor::undoParameters() {
  ParameterSet set;
  if (parameterHistory.undo(set) && set.size() >= 6) {
    state->set_parameter(PARAM::MIX, set[0] * 100.0f);
    state->set_parameter(PARAM::ROOM_SIZE, set[1] * 100.0f);
    state->set_parameter(PARAM::DECAY, set[2] * 100.0f);
    state->set_parameter(PARAM::DAMPING, set[3] * 100.0f);
    state->set_parameter(PARAM::WIDTH, set[4] * 100.0f);
    state->set_parameter(PARAM::PRE_DELAY, set[5]);
  }
}

// reapply a reverted parameter state
void PluginProcessor::redoParameters() {
  ParameterSet set;
  if (parameterHistory.redo(set) && set.size() >= 6) {
    state->set_parameter(PARAM::MIX, set[0] * 100.0f);
    state->set_parameter(PARAM::ROOM_SIZE, set[1] * 100.0f);
    state->set_parameter(PARAM::DECAY, set[2] * 100.0f);
    state->set_parameter(PARAM::DAMPING, set[3] * 100.0f);
    state->set_parameter(PARAM::WIDTH, set[4] * 100.0f);
    state->set_parameter(PARAM::PRE_DELAY, set[5]);
  }
}

// save current parameter values to history
void PluginProcessor::pushParameterState() {
  ParameterSet set{state->param_value(PARAM::MIX) / 100.0f,
                   state->param_value(PARAM::ROOM_SIZE) / 100.0f,
                   state->param_value(PARAM::DECAY) / 100.0f,
                   state->param_value(PARAM::DAMPING) / 100.0f,
                   state->param_value(PARAM::WIDTH) / 100.0f,
                   state->param_value(PARAM::PRE_DELAY)};
  parameterHistory.push(set);
}

// load preset by name
void PluginProcessor::loadPreset(const juce::String &name) {
  ParameterSet set;
  if (presetManager.getPreset(name, set) && set.size() >= 6) {
    state->set_parameter(PARAM::MIX, set[0] * 100.0f);
    state->set_parameter(PARAM::ROOM_SIZE, set[1] * 100.0f);
    state->set_parameter(PARAM::DECAY, set[2] * 100.0f);
    state->set_parameter(PARAM::DAMPING, set[3] * 100.0f);
    state->set_parameter(PARAM::WIDTH, set[4] * 100.0f);
    state->set_parameter(PARAM::PRE_DELAY, set[5]);
    pushParameterState();
  }
}

std::vector<juce::String> PluginProcessor::getCategoryNames() const {
  std::vector<juce::String> cats;
  presetManager.getLeafCategories(cats);
  return cats;
}

std::vector<juce::String>
PluginProcessor::getPresetNames(const juce::String &category) const {
  std::vector<juce::String> names;
  presetManager.getPresetsForCategory(category, names);
  return names;
}

// build pop-up menu for preset selection
void PluginProcessor::populatePresetMenu(
    juce::PopupMenu &menu, std::unordered_map<int, juce::String> &idMap,
    int &nextId) const {
  std::function<void(const PresetCategoryNode &, juce::PopupMenu &)> build =
      [&](const PresetCategoryNode &node, juce::PopupMenu &m) {
        for (const auto &child : node.children) {
          juce::PopupMenu sub;
          build(*child, sub);
          m.addSubMenu(child->name, sub);
        }
        for (const auto &p : node.presets) {
          int id = nextId++;
          m.addItem(id, p);
          idMap[id] = p;
        }
      };
  build(presetManager.root, menu);
}
