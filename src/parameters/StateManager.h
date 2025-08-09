#pragma once

class PluginProcessor;

#include <shared_mutex>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

#include "ParameterDefines.h"

// manages plugin state, parameters, and presets
class StateManager : public juce::ValueTree::Listener,
                     public juce::AudioProcessorValueTreeState::Listener {
public:
  StateManager(PluginProcessor *proc);
  ~StateManager() override;

  // parameter queries and edits
  float param_value(size_t param_id);
  juce::RangedAudioParameter *get_parameter(size_t param_id);
  void begin_change_gesture(size_t param_id);
  void end_change_gesture(size_t param_id);
  void set_parameter(size_t param_id, float value);
  void set_parameter_normalized(size_t param_id, float normalized_value);
  void randomize_parameter(size_t param_id, float min = 0.0f, float max = 1.0f);
  void reset_parameter(size_t param_id);
  void init();
  void randomize_parameters();
  juce::String get_parameter_text(size_t param_id);

  // state handling
  juce::ValueTree get_state();
  void save_preset(juce::String preset_name);
  void load_preset(juce::String preset_name);
  void load_from(juce::XmlElement *xml);
  void set_preset_name(juce::String preset_name);
  juce::String get_preset_name();
  void update_preset_modified();
  bool get_parameter_modified(size_t param_id, bool exchange_value = false);

  // undo support
  void undo();
  void redo();
  juce::UndoManager *get_undo_manager();

  // value tree and parameter callbacks
  void valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged,
                                const juce::Identifier &property) override;
  void parameterChanged(const juce::String &parameterID,
                        float newValue) override;

  // component registration for updates
  void register_component(size_t param_id, juce::Component *component,
                          std::function<void()> custom_callback = {});
  void unregister_component(size_t param_id, juce::Component *component);
  std::unordered_map<juce::Component *, std::function<void()>> &
  get_callbacks(size_t param_id) {
    return param_to_callback[param_id];
  }

  static inline const juce::Identifier PARAMETERS_ID{"PARAMETERS"};
  static inline const juce::Identifier PRESET_ID{"PRESET"};
  static inline const juce::Identifier PRESET_NAME_ID{"PRESET_NAME"};
  static inline const juce::Identifier PRESET_MODIFIED_ID{"PRESET_MODIFIED"};
  static inline const juce::Identifier PROPERTIES_ID{"PROPERTIES"};
  static inline const juce::Identifier STATE_ID{"STATE"};

  const juce::File PRESETS_DIR;
  const juce::String PRESET_EXTENSION;
  const juce::String DEFAULT_PRESET{"INIT"};

  std::atomic<bool> any_parameter_changed{false};
  std::atomic<bool> preset_modified{true};

private:
  void thread_safe_set_value_tree_property(juce::ValueTree tree,
                                           const juce::Identifier &name,
                                           const juce::var &new_value,
                                           juce::UndoManager *undo_manager_);
  juce::ValueTree state_tree;
  std::unique_ptr<juce::AudioProcessorValueTreeState> param_tree_ptr;
  juce::ValueTree property_tree;
  std::unordered_map<juce::String, std::atomic<float>> property_atomics;
  std::unordered_map<juce::String, std::atomic<bool>> parameter_modified_flags;
  std::unordered_map<juce::Component *, std::function<void()>>
      param_to_callback[TOTAL_NUMBER_PARAMETERS] = {};

  juce::ValueTree preset_tree;

  juce::Random rng;

  juce::UndoManager undo_manager;

  std::shared_mutex state_mutex;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StateManager)
};
