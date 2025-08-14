#pragma once

#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <audioapi/utils/SpscChannel.hpp>

namespace audioapi {

class AudioNode;

class AudioNodeDestructor {
 public:
  AudioNodeDestructor();
  ~AudioNodeDestructor();

  /// @brief Adds a node to the deconstruction queue.
  /// @param node The audio node to be deconstructed.
  /// @return True if the node was successfully added, false otherwise.
  bool addNodeForDeconstruction(const std::shared_ptr<AudioNode> &node);

 private:
  std::thread thread_;
  channels::spsc::Sender<
    std::shared_ptr<AudioNode>,
    channels::spsc::OverflowStrategy::WAIT_ON_FULL,
    channels::spsc::WaitStrategy::ATOMIC_WAIT> sender_;

  std::atomic<bool> isExiting_;

  /// @brief Processes audio nodes for deconstruction.
  /// @param receiver The receiver channel for incoming audio nodes.
  void process(channels::spsc::Receiver<
    std::shared_ptr<AudioNode>,
    channels::spsc::OverflowStrategy::WAIT_ON_FULL,
    channels::spsc::WaitStrategy::ATOMIC_WAIT> &&receiver);
};

} // namespace audioapi
