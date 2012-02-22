/* Copyright (c) 2009 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "boost/thread/locks.hpp"

#include "maidsafe/routing/routing.pb.h"
#include "maidsafe/routing/routing_table.h"
#include "maidsafe/routing/maidsafe_routing.h"
#include "maidsafe/routing/log.h"

#include "maidsafe/transport/rudp_transport.h"
#include "maidsafe/transport/utils.h"

#include "maidsafe/common/utils.h"


namespace maidsafe {
namespace routing {
  typedef protobuf::Contact Contact;
class RoutingImpl {
 public:
   RoutingImpl();
   RoutingTable routing_table_;
   transport::RudpTransport transport_;
   Contact my_contact_;
};

  
Routing::Routing() :  pimpl_(new RoutingImpl()) {
  pimpl_->routing_table_ = RoutingTable(pimpl_->my_contact_);
}
          

void Routing::Start(boost::asio::io_service& service) { // NOLINT
  pimpl_->transport_ = transport::RudpTransport(service);
  // TODO Frasers first question !! why oh why !!! cant I get my head around this without pointers !!
//   std::vector<IP> local_ips(transport::GetLocalAddresses());
  
}

void Routing::Send(const Message &message) { // NOLINT, cause I dont want pointers
//   Impl->routing_table_ 
}




// TODO get messages from transport




}  // namespace routing
}  // namespace maidsafe