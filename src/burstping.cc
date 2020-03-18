
#include "burstping.h"
#include "icmpheader.h"
#include "ipv4header.h"
#include "ipv6header.h"
#include "traceserviceheader.h"
#include "logger.h"

#include <iostream>

// ###### Constructor #######################################################
Burstping::Burstping(ResultsWriter*         resultsWriter,
           const unsigned int               iterations,
           const bool                       removeDestinationAfterRun,
           const boost::asio::ip::address&  sourceAddress,
           const std::set<DestinationInfo>& destinationArray,
           const unsigned long long         interval,
           const unsigned int               expiration,
           const unsigned int               ttl,
           const unsigned int               payload,
           const unsigned int               burst)
   :  payload(payload), 
      burst(burst),
      Ping(resultsWriter, iterations, removeDestinationAfterRun,
                sourceAddress, destinationArray,
                interval, expiration, ttl),
      BurstpingInstanceName(std::string("Burstping(") + sourceAddress.to_string() + std::string(")"))
   

{
}

// ###### Destructor ########################################################
Burstping::~Burstping()
{
}


// ###### Start thread ######################################################
const std::string& Burstping::getName() const
{
   std::cout << "Burstping name" << std::endl;
   return BurstpingInstanceName;
}

// ###### Send one ICMP request to given destination ########################
void Burstping::sendBurstICMPRequest(const DestinationInfo& destination,
                                 const unsigned int     ttl,
                                 const unsigned int     round,
                                 uint32_t&              targetChecksum,
                                 const unsigned int     payload)
{
   
   // ====== Set TTL ========================================
   const boost::asio::ip::unicast::hops option(ttl);
   ICMPSocket.set_option(option);

   // ====== Create an ICMP header for an echo request ======
   SeqNumber++;
   ICMPHeader echoRequest;
   echoRequest.type((isIPv6() == true) ? ICMPHeader::IPv6EchoRequest : ICMPHeader::IPv4EchoRequest);
   echoRequest.code(0);
   echoRequest.identifier(Identifier);
   echoRequest.seqNumber(SeqNumber);
   
   const std::chrono::system_clock::time_point sendTime = std::chrono::system_clock::now();

   // ======== Payload ========================
   std::vector<unsigned char> contents(payload, ~0);
   
   // ====== Tweak checksum ===============================
   computeInternet16(echoRequest, contents.begin(), contents.end());
   targetChecksum = echoRequest.checksum();
   
   // ====== Encode the request packet ======================
   boost::asio::streambuf request_buffer;
   std::ostream os(&request_buffer);
   os << echoRequest;
   os.write(reinterpret_cast<char const*>(contents.data()), contents.size());
   HPCT_LOG(info) << "Request size: " << request_buffer.size() << std::endl;
   
   // ====== Send the request ===============================
   std::size_t sent;
   try {
      int level;
      int option;
      int trafficClass = destination.trafficClass();
      if(destination.address().is_v6()) {
         level  = IPPROTO_IPV6;
         option = IPV6_TCLASS;
      }
      else {
         level  = IPPROTO_IP;
         option = IP_TOS;
      }

      if(setsockopt(ICMPSocket.native_handle(), level, option,
                    &trafficClass, sizeof(trafficClass)) < 0) {
         HPCT_LOG(warning) << "Unable to set Traffic Class!";
         sent = -1;
      }
      else {
         sent = ICMPSocket.send_to(request_buffer.data(), boost::asio::ip::icmp::endpoint(destination.address(), 0));
      }
   }
   catch (boost::system::system_error const& e) {
      sent = -1;
   }
   if(sent < 1) {
      HPCT_LOG(warning) << getName() << ": Burstping::sendBurstICMPRequest() - ICMP send_to("
                        << SourceAddress << "->" << destination << ") failed!";
   }
   else {
      // ====== Record the request ==========================
      OutstandingRequests++;

      assert((targetChecksum & ~0xffff) == 0);
      ResultEntry resultEntry(round, SeqNumber, ttl, (uint16_t)targetChecksum, sendTime,
                              destination, Unknown);
      std::pair<std::map<unsigned short, ResultEntry>::iterator, bool> result = ResultsMap.insert(std::pair<unsigned short, ResultEntry>(SeqNumber,resultEntry));
      assert(result.second == true);
   }
}

// ###### Send requests to all destinations #################################
void Burstping::sendRequests()
{

   std::lock_guard<std::recursive_mutex> lock(DestinationMutex);
   // ====== Send requests, if there are destination addresses ==============
   if(Destinations.begin() != Destinations.end()) {
      // All packets of this request block (for each destination) use the same checksum.
      // The next block of requests may then use another checksum.
      uint32_t targetChecksum = ~0U;
      for(std::set<DestinationInfo>::const_iterator destinationIterator = Destinations.begin();
          destinationIterator != Destinations.end(); destinationIterator++) {
         const DestinationInfo& destination = *destinationIterator;
         for(int i=1; i<=Burstping::burst; i++)
         {
            HPCT_LOG(info) << "Burst No. " << i << " of payload " << Burstping::payload << std::endl;
            sendBurstICMPRequest(destination, FinalMaxTTL, 0, targetChecksum, Burstping::payload);
         }
         
      }

      scheduleTimeoutEvent();
   }

   // ====== No destination addresses -> wait ===============================
   else {
      scheduleIntervalEvent();
   }
}