#include "StateManager.h"
#include "../plugin/PluginProcessor.h"
#include "../plugin/ProjectInfo.h"
#include <cassert>

// prepare parameter tree and preset storage
StateManager::StateManager(PluginProcessor *proc)
    : PRESETS_DIR(
          juce::File::getSpecialLocation(
              juce::File::SpecialLocationType::userMusicDirectory)
              .getChildFile(juce::String(JucePlugin_Manufacturer) + "_plugins")
              .getChildFile(JucePlugin_Name)
              .getChildFile("presets")),
      PRESET_EXTENSION("." + juce::String(JucePlugin_Name).toLowerCase()) {
  std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
  property_tree = juce::ValueTree(PROPERTIES_ID);

  for (size_t p_id = 0; p_id < PARAM::TOTAL_NUMBER_PARAMETERS; ++p_id) {
    if (PARAMETER_AUTOMATABLE[p_id]) {
      auto attributes =
          juce::AudioParameterFloatAttributes()
              .withLabel("")
              .withCategory(
                  juce::AudioProcessorParameter::Category::genericParameter)
              .withStringFromValueFunction([p_id](float value,
                                                  int maximumStringLength) {
                auto to_string_size = PARAMETER_TO_STRING_ARRS[p_id].size();
                juce::String res;
                if (to_string_size > 0 &&
                    (unsigned int)value < to_string_size) {
                  res = PARAMETER_TO_STRING_ARRS[p_id][(unsigned long)(value)];
                } else {
                  std::stringstream ss;
                  ss << std::fixed << std::setprecision(2) << value;
                  res = juce::String(ss.str());
                }
                auto output = (res + " " + PARAMETER_SUFFIXES[p_id]);
                return maximumStringLength > 0
                           ? output.substring(0, maximumStringLength)
                           : output;
              })
              .withValueFromStringFunction([p_id](juce::String text) {
                text = text.upToFirstOccurrenceOf(
                    " " + PARAMETER_SUFFIXES[p_id], false, true);
                auto to_string_size = PARAMETER_TO_STRING_ARRS[p_id].size();
                if (to_string_size > 0) {
                  auto beg = PARAMETER_TO_STRING_ARRS[p_id].begin();
                  auto end = PARAMETER_TO_STRING_ARRS[p_id].end();
                  auto it = std::find(beg, end, text);
                  if (it == end) {
                    DBG("ERROR: Could not find text in "
                        "PARAMETER_TO_STRING_ARRS");
                    return text.getFloatValue();
                  }
                  return float(it - beg);
                }
                return text.getFloatValue();
              });

      params.push_back(std::make_unique<juce::AudioParameterFloat>(
          juce::ParameterID{PARAMETER_NAMES[p_id], ProjectInfo::versionNumber},
          PARAMETER_NICKNAMES[p_id], PARAMETER_RANGES[p_id],
          PARAMETER_DEFAULTS[p_id], attributes));
    } else {
      property_tree.setProperty(PARAMETER_IDS[p_id], PARAMETER_DEFAULTS[p_id],
                                nullptr);
      property_atomics[PARAMETER_NAMES[p_id]].store(PARAMETER_DEFAULTS[p_id]);
    }
    parameter_modified_flags[PARAMETER_NAMES[p_id]].store(false);
  }

  param_tree_ptr.reset(new juce::AudioProcessorValueTreeState(
      *proc, &undo_manager, PARAMETERS_ID, {params.begin(), params.end()}));
  property_tree.addListener(this);

  for (size_t p_id = 0; p_id < PARAM::TOTAL_NUMBER_PARAMETERS; ++p_id) {
    if (PARAMETER_AUTOMATABLE[p_id]) {
      param_tree_ptr->addParameterListener(PARAMETER_NAMES[p_id], this);
    }
  }

  preset_tree = juce::ValueTree(PRESET_ID);
  preset_tree.setProperty(PRESET_NAME_ID, DEFAULT_PRESET, nullptr);
}

// remove listeners on destruction
StateManager::~StateManager() {
  property_tree.removeListener(this);
  for (size_t p_id = 0; p_id < PARAM::TOTAL_NUMBER_PARAMETERS; ++p_id) {
    if (PARAMETER_AUTOMATABLE[p_id])
      param_tree_ptr->removeParameterListener(PARAMETER_NAMES[p_id], this);
  }
}

// read current value for a parameter or property
float StateManager::param_value(size_t param_id) {
  if (PARAMETER_AUTOMATABLE[param_id])
    return param_tree_ptr->getRawParameterValue(PARAMETER_NAMES[param_id])
        ->load();
  return property_atomics[PARAMETER_NAMES[param_id]].load();
}

// build complete state tree
juce::ValueTree StateManager::get_state() {
  std::unique_lock<std::shared_mutex> lock(state_mutex);
  state_tree = juce::ValueTree(STATE_ID);
  state_tree.appendChild(param_tree_ptr->copyState(), nullptr);
  state_tree.appendChild(property_tree.createCopy(), nullptr);
  state_tree.appendChild(preset_tree.createCopy(), nullptr);
  return state_tree;
}

// save state to preset file
void StateManager::save_preset(juce::String preset_name) {
  {
    thread_safe_set_value_tree_property(preset_tree, PRESET_NAME_ID,
                                        preset_name, nullptr);
    thread_safe_set_value_tree_property(preset_tree, PRESET_MODIFIED_ID, false,
                                        nullptr);
  }
  auto file =
      PRESETS_DIR.getChildFile(preset_name).withFileExtension(PRESET_EXTENSION);
  if (!PRESETS_DIR.exists()) {
    PRESETS_DIR.createDirectory();
  }
  if (!file.existsAsFile()) {
    file.create();
  }

  auto plugin_state = get_state();

  std::unique_ptr<juce::XmlElement> xml(plugin_state.createXml());
  auto temp = juce::File::createTempFile("preset_temp");
  xml->writeTo(temp);
  temp.replaceFileIn(file);
}

// load state from preset file
void StateManager::load_preset(juce::String preset_name) {
  auto file =
      PRESETS_DIR.getChildFile(preset_name).withFileExtension(PRESET_EXTENSION);
  if (file.existsAsFile()) {
    std::unique_ptr<juce::XmlElement> xmlState = juce::XmlDocument::parse(file);
    load_from(xmlState.get());
  }
}

// load state from xml element
void StateManager::load_from(juce::XmlElement *xml) {
  if (xml != nullptr && xml->hasTagName(STATE_ID)) {
    auto new_tree = juce::ValueTree::fromXml(*xml);
    param_tree_ptr->replaceState(new_tree.getChildWithName(PARAMETERS_ID));
    std::unique_lock<std::shared_mutex> lock(state_mutex);
    property_tree.copyPropertiesFrom(new_tree.getChildWithName(PROPERTIES_ID),
                                     &undo_manager);
    preset_tree.copyPropertiesFrom(new_tree.getChildWithName(PRESET_ID),
                                   &undo_manager);
    preset_modified.store(false);
  }
}

// assign a preset name
void StateManager::set_preset_name(juce::String preset_name) {
  thread_safe_set_value_tree_property(preset_tree, PRESET_NAME_ID, preset_name,
                                      &undo_manager);
}

// obtain current preset name, marking if modified
juce::String StateManager::get_preset_name() {
  std::shared_lock<std::shared_mutex> lock(state_mutex);
  if (bool(preset_tree.getProperty(PRESET_MODIFIED_ID)))
    return preset_tree.getProperty(PRESET_NAME_ID).toString() + "*";
  return preset_tree.getProperty(PRESET_NAME_ID).toString();
}

// mark preset as modified if needed
void StateManager::update_preset_modified() {
  if (preset_modified.exchange(false))
    thread_safe_set_value_tree_property(preset_tree, PRESET_MODIFIED_ID, true,
                                        nullptr);
}

// check and optionally reset modified flag for a parameter
bool StateManager::get_parameter_modified(size_t param_id,
                                          bool exchange_value) {
  return parameter_modified_flags[PARAMETER_NAMES[param_id]].exchange(
      exchange_value);
}

// undo and redo operations
void StateManager::undo() { undo_manager.undo(); }
void StateManager::redo() { undo_manager.redo(); }

// access parameter object
juce::RangedAudioParameter *StateManager::get_parameter(size_t param_id) {
  assert(PARAMETER_AUTOMATABLE[param_id]);
  return param_tree_ptr->getParameter(PARAMETER_NAMES[param_id]);
}

// notify host that a gesture begins
void StateManager::begin_change_gesture(size_t param_id) {
  undo_manager.beginNewTransaction();
  if (PARAMETER_AUTOMATABLE[param_id]) {
    auto parameter = get_parameter(param_id);
    parameter->beginChangeGesture();
  }
}

// notify host that a gesture ends
void StateManager::end_change_gesture(size_t param_id) {
  if (PARAMETER_AUTOMATABLE[param_id]) {
    auto parameter = get_parameter(param_id);
    parameter->endChangeGesture();
  }
}

// set parameter using absolute value
void StateManager::set_parameter(size_t param_id, float value) {
  if (PARAMETER_AUTOMATABLE[param_id]) {
    auto range = PARAMETER_RANGES[param_id];
    auto normalized_value = range.convertTo0to1(range.snapToLegalValue(value));
    set_parameter_normalized(param_id, normalized_value);
  } else {
    thread_safe_set_value_tree_property(property_tree, PARAMETER_IDS[param_id],
                                        value, &undo_manager);
  }
}

// set parameter using normalized value
void StateManager::set_parameter_normalized(size_t param_id,
                                            float normalized_value) {
  normalized_value = std::clamp(normalized_value, 0.0f, 1.0f);
  if (PARAMETER_AUTOMATABLE[param_id]) {
    auto parameter = get_parameter(param_id);
    parameter->setValueNotifyingHost(normalized_value);
  } else {
    auto unnormalized_value =
        PARAMETER_RANGES[param_id].convertFrom0to1(normalized_value);
    set_parameter(param_id, unnormalized_value);
  }
}
// assign a random value within range
void StateManager::randomize_parameter(size_t param_id, float min, float max) {
  jassert(min >= 0.0f && max <= 1.0f && max >= min);
  auto value = rng.nextFloat() * (max - min) + min;
  set_parameter_normalized(param_id, value);
}

// get parameter value as text
juce::String StateManager::get_parameter_text(size_t param_id) {
  return get_parameter(param_id)->getText(
      PARAMETER_RANGES[param_id].convertTo0to1(param_value(param_id)), 20);
}

// reset parameter to default
void StateManager::reset_parameter(size_t param_id) {
  set_parameter(param_id, PARAMETER_DEFAULTS[param_id]);
}

// initialize all parameters and preset name
void StateManager::init() {
  for (size_t i = 0; i < PARAM::TOTAL_NUMBER_PARAMETERS; ++i)
    reset_parameter(i);

  set_preset_name(DEFAULT_PRESET);
  thread_safe_set_value_tree_property(preset_tree, PRESET_MODIFIED_ID, false,
                                      &undo_manager);
  preset_modified.store(false);
}

// randomize all parameters
void StateManager::randomize_parameters() {
  for (size_t i = 0; i < PARAM::TOTAL_NUMBER_PARAMETERS; ++i)
    randomize_parameter(i);
}

// access undo manager
juce::UndoManager *StateManager::get_undo_manager() { return &undo_manager; }

// respond to value tree property changes
void StateManager::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property) {
  if (treeWhosePropertyHasChanged != preset_tree) {
    preset_modified.store(true);
    any_parameter_changed.store(true);
    if (treeWhosePropertyHasChanged == property_tree) {
      float changed_property_value;
      {
        std::shared_lock<std::shared_mutex> lock(state_mutex);
        changed_property_value = float(property_tree.getProperty(property));
      }
      property_atomics[property.toString()].store(changed_property_value);
      parameter_modified_flags[property.toString()].store(true);
    }
  }
}

// react to parameter changes
void StateManager::parameterChanged(const juce::String &parameterID,
                                    float newValue) {
  preset_modified.store(true);
  any_parameter_changed.store(true);
  parameter_modified_flags[parameterID].store(true);
  juce::ignoreUnused(newValue);
}

// register a component for automatic updates
void StateManager::register_component(size_t param_id,
                                      juce::Component *component,
                                      std::function<void()> custom_callback) {
  assert(param_id <= TOTAL_NUMBER_PARAMETERS);
  assert(param_to_callback[param_id].find(component) ==
         param_to_callback[param_id].end());
  if (custom_callback)
    param_to_callback[param_id][component] = custom_callback;
  else
    param_to_callback[param_id][component] = [component]() {
      component->repaint();
    };
}

// remove a registered component
void StateManager::unregister_component(size_t param_id,
                                        juce::Component *component) {
  assert(param_id <= TOTAL_NUMBER_PARAMETERS);
  param_to_callback[param_id].erase(component);
}

// set a value tree property under lock
void StateManager::thread_safe_set_value_tree_property(
    juce::ValueTree tree, const juce::Identifier &name,
    const juce::var &new_value, juce::UndoManager *undo_manager_) {
  std::shared_lock<std::shared_mutex> lock(state_mutex);
  tree.setProperty(name, new_value, undo_manager_);
}
