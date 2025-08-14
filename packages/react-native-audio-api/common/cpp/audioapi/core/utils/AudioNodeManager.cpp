#include <audioapi/core/AudioNode.h>
#include <audioapi/core/AudioParam.h>
#include <audioapi/core/sources/AudioScheduledSourceNode.h>
#include <audioapi/core/utils/AudioNodeManager.h>
#include <audioapi/core/utils/Locker.h>

namespace audioapi {

AudioNodeManager::AudioNodeManager() {
  constexpr size_t CHANNEL_CAPACITY = 1024;
  auto [sender, receiver] = channels::spsc::channel<
      std::unique_ptr<Event>,
      channels::spsc::OverflowStrategy::WAIT_ON_FULL,
      channels::spsc::WaitStrategy::BUSY_LOOP>(CHANNEL_CAPACITY);
  sender_ = std::move(sender);
  receiver_ = std::move(receiver);
}

AudioNodeManager::~AudioNodeManager() {
  cleanup();
}

void AudioNodeManager::addPendingNodeConnection(
    const std::shared_ptr<AudioNode> &from,
    const std::shared_ptr<AudioNode> &to,
    ConnectionType type) {
  auto event = std::make_unique<Event>();
  event->type = type;
  event->payloadType = EventPayloadType::NODES;
  event->payload.nodes.from = from;
  event->payload.nodes.to = to;

  sender_.send(std::move(event));
}

void AudioNodeManager::addPendingParamConnection(
    const std::shared_ptr<AudioNode> &from,
    const std::shared_ptr<AudioParam> &to,
    ConnectionType type) {
  auto event = std::make_unique<Event>();
  event->type = type;
  event->payloadType = EventPayloadType::PARAMS;
  event->payload.params.from = from;
  event->payload.params.to = to;

  sender_.send(std::move(event));
}

void AudioNodeManager::preProcessGraph() {
  settlePendingConnections();
  prepareNodesForDestruction();
}

void AudioNodeManager::addProcessingNode(
    const std::shared_ptr<AudioNode> &node) {
  auto event = std::make_unique<Event>();
  event->type = ConnectionType::ADD;
  event->payloadType = EventPayloadType::NODE;
  event->payload.node = node;

  sender_.send(std::move(event));
}

void AudioNodeManager::addSourceNode(
    const std::shared_ptr<AudioScheduledSourceNode> &node) {
  auto event = std::make_unique<Event>();
  event->type = ConnectionType::ADD;
  event->payloadType = EventPayloadType::SOURCE_NODE;
  event->payload.sourceNode = node;

  sender_.send(std::move(event));
}

void AudioNodeManager::addAudioParam(const std::shared_ptr<AudioParam> &param) {
  auto event = std::make_unique<Event>();
  event->type = ConnectionType::ADD;
  event->payloadType = EventPayloadType::AUDIO_PARAM;
  event->payload.audioParam = param;

  sender_.send(std::move(event));
}

void AudioNodeManager::settlePendingConnections() {
  std::unique_ptr<Event> value;
  while (receiver_.try_receive(value) !=
         channels::spsc::ResponseStatus::CHANNEL_EMPTY) {
    switch (value->type) {
      case ConnectionType::CONNECT:
        handleConnectEvent(std::move(value));
        break;
      case ConnectionType::DISCONNECT:
        handleDisconnectEvent(std::move(value));
        break;
      case ConnectionType::DISCONNECT_ALL:
        handleDisconnectAllEvent(std::move(value));
        break;
      case ConnectionType::ADD:
        handleAddEvent(std::move(value));
        break;
    }
  }
}

void AudioNodeManager::handleConnectEvent(std::unique_ptr<Event> event) {
  assert(event->payloadType == EventPayloadType::NODES);
  event->payload.nodes.from->connectNode(event->payload.nodes.to);
}

void AudioNodeManager::handleDisconnectEvent(std::unique_ptr<Event> event) {
  assert(event->payloadType == EventPayloadType::NODES);
  event->payload.nodes.from->disconnectNode(event->payload.nodes.to);
}

void AudioNodeManager::handleDisconnectAllEvent(std::unique_ptr<Event> event) {
  assert(event->payloadType == EventPayloadType::NODES);
  for (auto it = event->payload.nodes.from->outputNodes_.begin();
       it != event->payload.nodes.from->outputNodes_.end();) {
    auto next = std::next(it);
    event->payload.nodes.from->disconnectNode(*it);
    it = next;
  }
}

void AudioNodeManager::handleAddEvent(std::unique_ptr<Event> event) {
  switch (event->payloadType) {
    case EventPayloadType::NODE:
      processingNodes_.insert(event->payload.node);
      break;
    case EventPayloadType::SOURCE_NODE:
      sourceNodes_.insert(event->payload.sourceNode);
      break;
    case EventPayloadType::AUDIO_PARAM:
      audioParams_.insert(event->payload.audioParam);
      break;
    default:
      assert(false && "Unknown event payload type");
  }
}

void AudioNodeManager::cleanupNode(const std::shared_ptr<AudioNode> &node) {
  node->cleanup();
  nodeDeconstructor_.addNodeForDeconstruction(node);
}

void AudioNodeManager::prepareNodesForDestruction() {
  auto sNodesIt = sourceNodes_.begin();

  while (sNodesIt != sourceNodes_.end()) {
    // we don't want to destroy nodes that are still playing or will be
    // playing
    if (sNodesIt->use_count() == 1 &&
        (sNodesIt->get()->isUnscheduled() || sNodesIt->get()->isFinished())) {
      cleanupNode(*sNodesIt);
      sNodesIt = sourceNodes_.erase(sNodesIt);
    } else {
      ++sNodesIt;
    }
  }

  auto pNodesIt = processingNodes_.begin();

  while (pNodesIt != processingNodes_.end()) {
    if (pNodesIt->use_count() == 1) {
      cleanupNode(*pNodesIt);
      pNodesIt = processingNodes_.erase(pNodesIt);
    } else {
      ++pNodesIt;
    }
  }
}

void AudioNodeManager::cleanup() {
  for (auto it = sourceNodes_.begin(), end = sourceNodes_.end(); it != end;
       ++it) {
    it->get()->cleanup();
  }

  for (auto it = processingNodes_.begin(), end = processingNodes_.end();
       it != end;
       ++it) {
    it->get()->cleanup();
  }

  sourceNodes_.clear();
  processingNodes_.clear();
}

} // namespace audioapi
