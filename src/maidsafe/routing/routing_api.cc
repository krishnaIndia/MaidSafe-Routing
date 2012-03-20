/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#include "boost/asio/deadline_timer.hpp"
#include "boost/date_time.hpp"
#include "maidsafe/routing/routing_api.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/node_id.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/rpcs.h"

namespace fs = boost::filesystem;
namespace bs2 = boost::signals2;


namespace maidsafe {

namespace routing {

namespace {
const unsigned int kNumChunksToCache(100);
const unsigned int kTimoutInSeconds(5);
}


Message::Message()
    : type(0),
      source_id(),
      destination_id(),
      data(),
      cacheable(false),
      direct(false),
      replication(0) {}

Message::Message(const protobuf::Message &protobuf_message)
    : type(protobuf_message.type()),
      source_id(protobuf_message.source_id()),
      destination_id(protobuf_message.destination_id()),
      data(protobuf_message.data()),
      cacheable(protobuf_message.cacheable()),
      direct(protobuf_message.direct()),
      replication(protobuf_message.replication()) {}

Routing::Routing(NodeType node_type,
                 const asymm::PrivateKey &private_key,
                 const std::string &node_id,
                 bool signatures_required,  // sets all nodes sign data with asymm
                 bool encryption_required)
    : asio_service_(),
      bootstrap_file_(),
      bootstrap_nodes_(),
      private_key_(private_key),
      node_local_endpoint_(),
      node_external_endpoint_(),
      transport_(new transport::ManagedConnection()),
      routing_table_(new RoutingTable(node_id)),
      rpc_ptr_(new Rpcs(routing_table_, transport_)),
      service_(new Service(rpc_ptr_, routing_table_)),
      message_received_signal_(),
      network_status_signal_(),
      cache_size_hint_(kNumChunksToCache),
      cache_chunks_(),
      waiting_for_response_(),
      joined_(false),
      signatures_required_(signatures_required),
      encryption_required_(encryption_required),
      node_type_(node_type) {
  Init();
}

void Routing::Init() {
  asio_service_.Start(5);
  // TODO fill in bootstrap file location and do ReadConfigFile
  transport_->Init(20);
  node_local_endpoint_ = transport_->GetOurEndpoint();
  LOG(INFO) << " Local IP address : " << node_local_endpoint_.ip.to_string();
  LOG(INFO) << " Local Port       : " << node_local_endpoint_.port;
  transport_->on_message_received()->connect(
      std::bind(&Routing::ReceiveMessage, this, args::_1));
  Join();
}

void Routing::Send(const Message &message,
                   const ResponseReceivedFunctor &response_functor) {
  if (message.type < 100) {
    DLOG(ERROR) << "Attempt to use Reserved message type (<100), aborted send";
    return;
  }
  uint32_t message_unique_id = AddToCallbackQueue(response_functor);
  protobuf::Message proto_message;
  proto_message.set_id(message_unique_id);
  proto_message.set_source_id(message.source_id);
  proto_message.set_destination_id(message.destination_id);
  proto_message.set_cacheable(message.cacheable);
  proto_message.set_data(message.data);
  proto_message.set_direct(message.direct);
  proto_message.set_replication(message.replication);
  proto_message.set_type(message.type);
  SendOn(proto_message, NodeId(message.destination_id));
}


uint32_t Routing::AddToCallbackQueue(const ResponseReceivedFunctor &response_functor) {
  auto it = waiting_for_response_.end();
  uint32_t id;
  while(it == waiting_for_response_.end()) {
    id = RandomUint32();
    it = waiting_for_response_.find(id);
  }
  waiting_for_response_.insert(std::pair<uint32_t, ResponseReceivedFunctor>
                                                     (id, response_functor));
  boost::asio::deadline_timer timer(asio_service_.service(),
                                 boost::posix_time::seconds(kTimoutInSeconds));
  timer.async_wait(std::bind(&Routing::FindAndKillJob, this, id));
  return id;
}

void Routing::ExecuteCallback(protobuf::Message &message) {
 uint32_t id = message.id();
 auto it = waiting_for_response_.find(id);
 if (it != waiting_for_response_.end()) {
     Message message_struct;
     message_struct.source_id = message.source_id();
     message_struct.data = message.data();
     (*it).second(message_struct);
 }  // otherwise timed out and deleted 
}

void Routing::FindAndKillJob(uint32_t job_number) {
 auto it = waiting_for_response_.find(job_number);
 if (it != waiting_for_response_.end()) {
    Message failure;
    failure.timeout = true;
   (*it).second(failure); // send failure in callback
   waiting_for_response_.erase(it); // kill job
 }
}

bs2::signal<void(int, Message)> &Routing::RequestReceivedSignal() {
  return message_received_signal_;
}

bs2::signal<void(unsigned int)> &Routing::NetworkStatusSignal() {
  return network_status_signal_;
}

void Routing::Join() {
  if (bootstrap_nodes_.empty()) {
    DLOG(INFO) << "No bootstrap nodes";
    return;
  }

  for (auto it = bootstrap_nodes_.begin();
       it != bootstrap_nodes_.end(); ++it) {
    // TODO)dirvine) send bootstrap requests

  }
}

// drop existing routing table and restart
void Routing::BootStrapFromThisEndpoint(const transport::Endpoint
                                                             &endpoint) {
  LOG(INFO) << " Entered bootstrap IP address : " << endpoint.ip.to_string();
  LOG(INFO) << " Entered bootstrap Port       : " << endpoint.port;
  for (unsigned int i = 0; i < routing_table_->Size(); ++i) {
    NodeInfo remove_node =
                 routing_table_->GetClosestNode(routing_table_->kNodeId(), 0);
    transport_->RemoveConnection(remove_node.endpoint);
    routing_table_->DropNode(remove_node.endpoint);
  }
  network_status_signal_(routing_table_->Size());
  bootstrap_nodes_.clear();
  bootstrap_nodes_.push_back(endpoint);
  asio_service_.service().post(std::bind(&Routing::Join, this));
}

bool Routing::WriteConfigFile() const {
  // TODO(dirvine) implement
return false;
}

bool Routing::ReadConfigFile() {  //TODO(dirvine) FIXME now a dir
  protobuf::ConfigFile protobuf_config;
  protobuf::Bootstrap protobuf_bootstrap;
  // TODO(Fraser#5#): 2012-03-14 - Use try catch / pass error_code for fs funcs.
//  if (!fs::exists(config_file_) || !fs::is_regular_file(config_file_)) {
//    DLOG(ERROR) << "Cannot read config file " << config_file_;
//    return false;
//  }
//  try {
//    fs::ifstream config_file_stream(config_file_);
//    if (!protobuf_config.ParseFromString(config_file_.string()))
//      return false;
//    if (!private_key_is_set_) {
//      if (!protobuf_config.has_private_key()) {
//        DLOG(ERROR) << "No private key in config or set ";
//        return false;
//      } else {
//        asymm::DecodePrivateKey(protobuf_config.private_key(), &private_key_);
//      }
//    }
//    if (!node_is_set_) {
//      if (protobuf_config.has_node_id()) {
//         node_id_ = NodeId(protobuf_config.node_id());
//       } else {
//        DLOG(ERROR) << "Cannot read NodeId ";
//        return false;
//       }
//    }
//    transport::Endpoint endpoint;
//    for (int i = 0; i != protobuf_bootstrap.endpoint_size(); ++i) {
//      endpoint.ip.from_string(protobuf_bootstrap.endpoint(i).ip());
//      endpoint.port= protobuf_bootstrap.endpoint(i).port();
//      bootstrap_nodes_.push_back(endpoint);
//    }
//  }
//  catch(const std::exception &e) {
//    DLOG(ERROR) << "Exception: " << e.what();
//    return false;
//  }
  return true;
}

void Routing::AckReceived(const transport::TransportCondition &return_value,
                          const std::string &message) {
  if (return_value != transport::kSuccess)
    return;  // TODO(dirvine) FIXME we may need to take action here
             // depending on return code
  ReceiveMessage(message);
}

void Routing::SendOn(const protobuf::Message &message,
                         const NodeId &target_node) {
  std::string message_data(message.SerializeAsString());
  transport::Endpoint send_to =
             routing_table_->GetClosestNode(target_node, 0).endpoint;
  transport::ResponseFunctor response_functor =
                std::bind(&Routing::AckReceived, this, args::_1, args::_2);
  transport_->Send(send_to, message_data, response_functor);
}

void Routing::ReceiveMessage(const std::string &message) {
  protobuf::Message protobuf_message;
  protobuf::ConnectRequest connection_request;
  if (protobuf_message.ParseFromString(message))
    ProcessMessage(protobuf_message);
}


void Routing::ProcessMessage(protobuf::Message &message) {
  // TODO(dirvine) if message is from/for a client connected
  // to us replace the source address to me for requests
  // in responses the message will be direct with our address

  // handle cache data
  if (message.has_cacheable() && message.cacheable()) {
    if (message.has_response() && message.response()) {
      AddToCache(message);
     } else  {  // request
       if (GetFromCache(message))
         return;
     }
  }
  // is it for us ??
  if (!routing_table_->AmIClosestNode(NodeId(message.destination_id()))) {
    NodeId next_node =
     routing_table_->GetClosestNode(NodeId(message.destination_id()), 0).node_id;
    SendOn(message, next_node);
    return;
  } else {  // I am closest
    if (message.type() == 0) {  // ping
      if (message.has_response() && message.response()) {
        // TODO(dirvine) FIXME its for me  !! DoPingResponse(message);
        return;  // Job done !!
      } else {
        service_->Ping(message);
      }
    }
    if (message.type() == 1) {  // find_nodes
      if (message.has_response() && message.response()) {
        // TODO(dirvine) FIXME its for me  !!        DoFindNodeResponse(message);
       return;
      } else {
         service_->FindNodes(message);
         return;
      }
    }
    if (message.type() == 2) {  // bootstrap
      if (message.has_response() && message.response()) {
         // TODO(dirvine) FIXME its for me  !!        DoConnectResponse(message);
       return;
      } else {
         service_->Connect(message);
         return;
      }
    }

    // if this is set not direct and ID == ME do NOT respond.
    if (!message.direct() &&
      (message.destination_id() != routing_table_->kNodeId().String())) {
      try {
        Message msg(message);
        message_received_signal_(static_cast<int>(message.type()), msg);
      }
      catch(const std::exception &e) {
        DLOG(ERROR) << e.what();
      }
    }
    // I am closest so will send to all my replicant nodes
    message.set_direct(true);
    message.set_source_id(routing_table_->kNodeId().String());
    auto close =
          routing_table_->GetClosestNodes(NodeId(message.destination_id()),
                                 static_cast<uint16_t>(message.replication()));
    for (auto it = close.begin(); it != close.end(); ++it) {
      message.set_destination_id((*it).String());
      NodeId send_to = routing_table_->GetClosestNode((*it), 0).node_id;
      SendOn(message, send_to);
    }
  }
}




bool Routing::GetFromCache(protobuf::Message &message) {
  bool result(false);
  for (auto it = cache_chunks_.begin(); it != cache_chunks_.end(); ++it) {
      if ((*it).first == message.source_id()) {
        result = true;
        message.set_destination_id(message.source_id());
        message.set_cacheable(true);
        message.set_data((*it).second);
        message.set_source_id(routing_table_->kNodeId().String());
        message.set_direct(true);
        message.set_response(false);
        NodeId next_node =
           routing_table_->GetClosestNode(NodeId(message.destination_id()),
                                         0).node_id;
        SendOn(message, next_node);
      }
  }
  return result;
}

void Routing::AddToCache(const protobuf::Message &message) {
  std::pair<std::string, std::string> data;
  try {
    // check data is valid TODO FIXME - ask CAA
    if (crypto::Hash<crypto::SHA512>(message.data()) != message.source_id())
      return;
    data = std::make_pair(message.source_id(), message.data());
    cache_chunks_.push_back(data);
    while (cache_chunks_.size() > cache_size_hint_)
      cache_chunks_.erase(cache_chunks_.begin());
  }
  catch(const std::exception &/*e*/) {
    // oohps reduce cache size quickly
    cache_size_hint_ = cache_size_hint_ / 2;
    while (cache_chunks_.size() > cache_size_hint_)
      cache_chunks_.erase(cache_chunks_.begin()+1);
  }
}


}  // namespace routing

}  // namespace maidsafe
