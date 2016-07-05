// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//#define DEBUG // Debug supression

#include <os>
#include <net/inet4>

using namespace std;
using namespace net;
auto& timer = hw::PIT::instance();

void Service::start()
{
  static auto inet = 
    net::new_ipv4_stack<>({ 10,0,0,42 },      // IP
                          { 255,255,255,0 },  // Netmask
                          { 10,0,0,1 });      // Gateway
  printf("IP address: %s \n", inet->ip_addr().str().c_str());

  // UDP
  const UDP::port_t port = 4242;
  auto& conn = inet->udp().bind(port);

  conn.on_read(
  [&conn] (auto addr, auto port, const char* data, int len) {
    string received = std::string(data,len);

    if (received == "SUCCESS") {
      INFO("Test 2", "Client says SUCCESS");
      return;
    }

    INFO("Test 2","Starting UDP-test. Got UDP data from %s: %i: %s",
         addr.str().c_str(), port, received.c_str());

    const int PACKETS = 2400;

    string first_reply {"Received '" + received +
                        "'. Expect " + to_string(PACKETS) + 
                        " packets in 1s\n" };

    // Send the first packet, and then wait for ARP
    conn.sendto(addr, port, first_reply.c_str(), first_reply.size());

    timer.on_timeout_ms(1s, 
    [&conn, addr, port, data, len] {
      size_t packetsize = inet->ip_obj().MDDS() - sizeof(UDP::udp_header);
      INFO("Test 2", "Trying to transmit %i UDP packets of size %i at maximum throttle",
           PACKETS, packetsize);

      for (int i = 0; i < PACKETS; i++) {
        char c = 'A' + (i % 26);
        string send (packetsize, c);
        //printf("<TEST> %i nr. of  %c \n", send.size(), c);
        conn.sendto(addr, port, send.c_str() , send.size());
      }

      CHECK(1,"UDP-transmission didn't panic");
      INFO("UDP Transmision tests", "SUCCESS");
    });
  });

  timer.on_timeout_ms(200ms,
  [=] {
    const int PACKETS = 600;
    INFO("Test 1", "Trying to transmit %i ethernet packets at maximum throttle", PACKETS);
    for (int i=0; i < PACKETS; i++){
      auto pckt = inet->createPacket(inet->MTU());
      Ethernet::header* hdr = reinterpret_cast<Ethernet::header*>(pckt->buffer());
      hdr->dest.major = Ethernet::addr::BROADCAST_FRAME.major;
      hdr->dest.minor = Ethernet::addr::BROADCAST_FRAME.minor;
      hdr->type = Ethernet::ETH_ARP;
      inet->link().transmit(pckt);
    }

    CHECK(1,"Transmission didn't panic");
    INFO("Test 1", "Done. Send some UDP-data to %s:%i to continue test",
         inet->ip_addr().str().c_str(), port);
  });
}
