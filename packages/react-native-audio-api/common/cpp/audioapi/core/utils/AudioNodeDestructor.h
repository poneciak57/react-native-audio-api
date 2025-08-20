#pragma once

#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <audioapi/utils/SpscChannel.hpp>

namespace audioapi {

class AudioNode;

#define AUDIO_NODE_DESTRUCTOR_SPSC_OPTIONS \
  std::shared_ptr<AudioNode>, \
  channels::spsc::OverflowStrategy::WAIT_ON_FULL, \
  channels::spsc::WaitStrategy::ATOMIC_WAIT

class AudioNodeDestructor {
 public:
  AudioNodeDestructor();
  ~AudioNodeDestructor();

  /// @brief Adds a node to the deconstruction queue.
  /// @param node The audio node to be deconstructed.
  /// @return True if the node was successfully added, false otherwise.
  /// @note node does NOT get moved out if it is not successfully added.
  bool tryAddNodeForDeconstruction(std::shared_ptr<AudioNode> &&node);

 private:
  std::thread thread_;
  std::atomic<bool> isExiting_;

  channels::spsc::Sender<
    AUDIO_NODE_DESTRUCTOR_SPSC_OPTIONS> sender_;

  /// @brief Processes audio nodes for deconstruction.
  /// @param receiver The receiver channel for incoming audio nodes.
  void process(channels::spsc::Receiver<
    AUDIO_NODE_DESTRUCTOR_SPSC_OPTIONS> &&receiver);
};

#undef AUDIO_NODE_DESTRUCTOR_SPSC_OPTIONS

} // namespace audioapi
