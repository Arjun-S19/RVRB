#pragma once

#include <algorithm>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <memory>
#include <mutex>
#include <queue>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// circular buffer for basic delay line
class DelayBuffer {
public:
  explicit DelayBuffer(size_t size = 48000) { resize(size); }
  void resize(size_t size) {
    buffer.assign(size, 0.0f);
    writePos = 0;
  }
  void push(float sample) {
    buffer[writePos] = sample;
    writePos = (writePos + 1) % buffer.size();
  }
  float getDelayed(size_t delaySamples) const {
    size_t index = (writePos + buffer.size() - (delaySamples % buffer.size())) %
                   buffer.size();
    return buffer[index];
  }

private:
  std::vector<float> buffer;
  size_t writePos{0};
};

enum { MaxEarlyReflections = 32 };
// holds delay and gain for an early reflection
struct EarlyReflection {
  size_t delaySamples{};
  float amplitude{};
};

// lock-protected queue to pass audio blocks between threads
class AudioBlockQueue {
public:
  explicit AudioBlockQueue(size_t maxBlocks = 50) : maxSize(maxBlocks) {}

  void push(const juce::AudioBuffer<float> &block) {
    juce::AudioBuffer<float> copy(block.getNumChannels(),
                                  block.getNumSamples());
    copy.makeCopyOf(block);
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.size() >= maxSize)
      queue.pop();
    queue.push(std::move(copy));
  }
  bool pop(juce::AudioBuffer<float> &block) {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.empty())
      return false;
    block = std::move(queue.front());
    queue.pop();
    return true;
  }

private:
  std::queue<juce::AudioBuffer<float>> queue;
  size_t maxSize;
  std::mutex mutex;
};

using ParameterSet = std::vector<float>;
// handles undo and redo for parameter sets
class ParameterHistory {
public:
  void push(const ParameterSet &set) {
    undoStack.push(set);
    while (!redoStack.empty())
      redoStack.pop();
  }
  bool undo(ParameterSet &set) {
    if (undoStack.size() < 2)
      return false;
    redoStack.push(undoStack.top());
    undoStack.pop();
    set = undoStack.top();
    return true;
  }
  bool redo(ParameterSet &set) {
    if (redoStack.empty())
      return false;
    undoStack.push(redoStack.top());
    set = redoStack.top();
    redoStack.pop();
    return true;
  }

private:
  std::stack<ParameterSet> undoStack, redoStack;
};

// tree node used to organize presets
struct PresetCategoryNode {
  juce::String name;
  std::vector<std::unique_ptr<PresetCategoryNode>> children;
  std::vector<juce::String> presets;
};

// manages preset storage and retrieval
class PresetManager {
public:
  void addPreset(const juce::String &name, const ParameterSet &set,
                 const std::vector<juce::String> &categoryPath = {}) {
    lookup[name] = set;
    PresetCategoryNode *node = &root;
    for (const auto &cat : categoryPath) {
      auto it = std::find_if(node->children.begin(), node->children.end(),
                             [&](const auto &c) { return c->name == cat; });
      if (it == node->children.end()) {
        node->children.push_back(std::make_unique<PresetCategoryNode>());
        node->children.back()->name = cat;
        node = node->children.back().get();
      } else {
        node = it->get();
      }
    }
    node->presets.push_back(name);
  }

  bool getPreset(const juce::String &name, ParameterSet &set) const {
    auto it = lookup.find(name);
    if (it == lookup.end())
      return false;
    set = it->second;
    return true;
  }

  void getLeafCategories(std::vector<juce::String> &out) const {
    getLeafCategoriesRecursive(root, juce::String(), out);
  }

  void getPresetsForCategory(const juce::String &path,
                             std::vector<juce::String> &out) const {
    if (auto *node = findCategory(path))
      out = node->presets;
  }

  PresetCategoryNode root{"ROOT"};

private:
  PresetCategoryNode *findCategory(const juce::String &path) const {
    if (path.isEmpty())
      return const_cast<PresetCategoryNode *>(&root);
    juce::StringArray tokens;
    tokens.addTokens(path, "/", "");
    auto *node = const_cast<PresetCategoryNode *>(&root);
    for (auto &token : tokens) {
      auto it = std::find_if(node->children.begin(), node->children.end(),
                             [&](const auto &c) { return c->name == token; });
      if (it == node->children.end())
        return nullptr;
      node = it->get();
    }
    return node;
  }

  void getLeafCategoriesRecursive(const PresetCategoryNode &node,
                                  const juce::String &path,
                                  std::vector<juce::String> &out) const {
    juce::String newPath =
        (node.name == "ROOT")
            ? path
            : (path.isEmpty() ? node.name : path + "/" + node.name);
    if (node.children.empty()) {
      if (!newPath.isEmpty())
        out.push_back(newPath);
      return;
    }
    for (const auto &child : node.children)
      getLeafCategoriesRecursive(*child, newPath, out);
  }

  std::unordered_map<juce::String, ParameterSet> lookup;
};

struct GraphEdge {
  int to;
  float gain;
  float feedback;
};

// simple graph to detect feedback loops
class ReverbNetworkGraph {
public:
  void addEdge(int from, int to, float gain = 1.0f, float feedback = 0.0f) {
    adj[from].push_back({to, gain, feedback});
  }
  bool hasCycle() const {
    std::unordered_set<int> visited, stack;
    for (auto &kv : adj) {
      if (dfs(kv.first, visited, stack))
        return true;
    }
    return false;
  }

private:
  bool dfs(int v, std::unordered_set<int> &visited,
           std::unordered_set<int> &stack) const {
    if (!visited.insert(v).second)
      return stack.count(v) != 0;
    stack.insert(v);
    auto it = adj.find(v);
    if (it != adj.end()) {
      for (const auto &e : it->second) {
        if (dfs(e.to, visited, stack))
          return true;
      }
    }
    stack.erase(v);
    return false;
  }

  std::unordered_map<int, std::vector<GraphEdge>> adj;
};
