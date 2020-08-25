
#include "burstping.h"
#include "icmpheader.h"
#include "ipv4header.h"
#include "ipv6header.h"
#include "traceserviceheader.h"
#include "logger.h"

#include <iostream>

#include "tools.h"
#include <boost/format.hpp>

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
           const unsigned int               burst,
           const unsigned int               priority)
   :  Payload(payload), 
      Burst(burst),
      Priority(priority),
      Ping(resultsWriter, iterations, removeDestinationAfterRun,
                sourceAddress, destinationArray,
                interval, expiration, ttl, priority),
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

   TraceServiceHeader tsHeader;
   tsHeader.magicNumber(MagicNumber);
   tsHeader.sendTTL(ttl);
   tsHeader.round((unsigned char)round);
   tsHeader.checksumTweak(0);
   const std::chrono::system_clock::time_point sendTime = std::chrono::system_clock::now();
   tsHeader.sendTimeStamp(makePacketTimeStamp(sendTime));
   std::vector<unsigned char> tsHeaderContents = tsHeader.contents();

   // ====== Tweak checksum ===============================
   computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
   targetChecksum = echoRequest.checksum();

   // ====== Encode the request packet ======================
   boost::asio::streambuf request_buffer;
   std::ostream os(&request_buffer);
   os << echoRequest << tsHeader;
   int missing_data = payload - request_buffer.size();
   if (missing_data > 0){
      std::vector<unsigned char> contents(missing_data, ~0);
      os.write(reinterpret_cast<char const*>(contents.data()), contents.size());
   }
   //HPCT_LOG(info) << "Request size: " << request_buffer.size() << std::endl;
   
   //const std::chrono::system_clock::time_point sendTime = std::chrono::system_clock::now();

   // ======== Payload ========================
   //
   
   // ====== Tweak checksum ===============================
   //computeInternet16(echoRequest, contents.begin(), contents.end());
   //targetChecksum = echoRequest.checksum();
   
   // ====== Encode the request packet ======================
   //boost::asio::streambuf request_buffer;
   //std::ostream os(&request_buffer);
   //os << echoRequest;
   //os.write(reinterpret_cast<char const*>(contents.data()), contents.size());
   // HPCT_LOG(info) << "Request size: " << request_buffer.size() << std::endl;


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
      TotalPackets++;

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
         for(int i=1; i<=Burstping::Burst; i++)
         {
            // HPCT_LOG(info) << "Burst No. " << i << " of payload " << Burstping::Payload << std::endl;
            sendBurstICMPRequest(destination, FinalMaxTTL, 0, targetChecksum, Burstping::Payload);
         }
      }

      scheduleTimeoutEvent();
   }

   // ====== No destination addresses -> wait ===============================
   else {
      scheduleIntervalEvent();
   }

}

// ###### Comparison function for results output ############################
int Burstping::comparePingResults(const ResultEntry* a, const ResultEntry* b)
{
   return(a->destination() < b->destination());
}


// ###### Request stop of thread ############################################
void Burstping::requestStop() {
   Traceroute::requestStop();
   std::cout << std::endl;
   HPCT_LOG(info) << "Burstping icmp results:" << std:: endl
                     << "* ICMP ECHO REQUEST = " << TotalPackets   << " packets" << std::endl
                     << "* ICMP ECHO REPLY   = " << TotalResponses << " responses" << std::endl;

   // HPCT_LOG(info) << "Total ICMP ECHO REQUEST: " << totalPackets << std::endl;
   // HPCT_LOG(info) << "Total ICMP ECHO REPLY: " << totalResponses << std::endl;
   
   // std::cout << "Requests " << totalPackets << std::endl;
   // std::cout << "Responses " << totalResponses << std::endl;
 
   //std::cout << Ping::ResultsMap.size() << std::endl;
   //HPCT_LOG(warning) << "resultsVector " << resultEntry.size() ;
}


// ###### Start thread ######################################################
bool Burstping::start()
{
   StopRequested.exchange(false);
   Thread = std::thread(&Burstping::run, this);
   return(Traceroute::prepareSocket());
}

// ###### Run the measurement ###############################################
void Burstping::run()
{
   Identifier = ::getpid();   // Identifier is the process ID
   // NOTE: Assuming 16-bit PID, and one PID per thread!

   // ====== Set  priority =============================================
   
   int policy, res;

   struct sched_param param;

   std::cout << " ORIGINAL: ";
   std::cout << "policy=" << ((policy == SCHED_FIFO)  ? "SCHED_FIFO" :
                              (policy == SCHED_RR)    ? "SCHED_RR" :
                              (policy == SCHED_OTHER) ? "SCHED_OTHER" :
                                                         "???")
            << ", priority=" << param.sched_priority << std::endl;

   // #################################################
   // ##### SCHED_FIFO    a first-in, first-out policy; and
   // ##### SCHED_RR      a round-robin policy
   // #################################################
   // policy = SCHED_RR;
   policy = SCHED_FIFO;
   param.sched_priority = Burstping::Priority;

   // std::cout << sched_get_priority_max(Identifier) << std::endl;

   if ((res = sched_setscheduler(Identifier, policy, &param)) == -1)
   {
      perror("sched_setscheduler");
      return;
   }

   std::cout << " CHANGED: ";
   std::cout << "policy=" << ((policy == SCHED_FIFO)  ? "SCHED_FIFO" :
                                 (policy == SCHED_RR)    ? "SCHED_RR" :
                                 (policy == SCHED_OTHER) ? "SCHED_OTHER" :
                                                            "???")
               << ", RT priority=" << sched_getparam(Identifier, &param) 
               << ", priority=" << param.sched_priority << std::endl;

   prepareRun(true);
   sendRequests();
   expectNextReply();

   IOService.run();
}

// ###### Process results ###################################################
void Burstping::processResults()
{
   // ====== Sort results ===================================================
   std::vector<ResultEntry*> resultsVector;
   for(std::map<unsigned short, ResultEntry>::iterator iterator = ResultsMap.begin(); iterator != ResultsMap.end(); iterator++) {
      resultsVector.push_back(&iterator->second);
   }
   std::sort(resultsVector.begin(), resultsVector.end(), &comparePingResults);

   // ====== Process results ================================================
   const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
   for(std::vector<ResultEntry*>::iterator iterator = resultsVector.begin(); iterator != resultsVector.end(); iterator++) {
      ResultEntry* resultEntry = *iterator;

      // ====== Time-out entries ============================================
      if( (resultEntry->status() == Unknown) &&
          (std::chrono::duration_cast<std::chrono::milliseconds>(now - resultEntry->sendTime()).count() >= Expiration) ) {
         resultEntry->setStatus(Timeout);
         resultEntry->setReceiveTime(resultEntry->sendTime() + std::chrono::milliseconds(Expiration));
      }

      // ====== Print completed entries =====================================
      if(resultEntry->status() != Unknown) {
         HPCT_LOG(trace) << getName() << ": " << *resultEntry;

         TotalResponses++;

         if(ResultCallback) {
            ResultCallback(this, resultEntry);
         }

         if(ResultsOutput) {
            ResultsOutput->insert(
               str(boost::format("#P %s %s %x %x %d %d %x")
                  % SourceAddress.to_string()
                  % resultEntry->destinationAddress().to_string()
                  % usSinceEpoch(resultEntry->sendTime())
                  % resultEntry->checksum()
                  % resultEntry->status()
                  % std::chrono::duration_cast<std::chrono::microseconds>(resultEntry->receiveTime() - resultEntry->sendTime()).count()
                  % (unsigned int)resultEntry->destination().trafficClass()
            ));
         }
      }

      // ====== Remove completed entries ====================================
      if(resultEntry->status() != Unknown) {
         assert(ResultsMap.erase(resultEntry->seqNumber()) == 1);
         if(OutstandingRequests > 0) {
            OutstandingRequests--;
         }
      }
   }

   if(RemoveDestinationAfterRun == true) {
      std::lock_guard<std::recursive_mutex> lock(DestinationMutex);
      DestinationIterator = Destinations.begin();
      while(DestinationIterator != Destinations.end()) {
         Destinations.erase(DestinationIterator);
         DestinationIterator = Destinations.begin();
      }
   }
}
