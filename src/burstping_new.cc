
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

void Burstping::handler(
      const boost::system::error_code& error, // Result of operation.
      std::size_t bytes_transferred           // Number of bytes sent.
)
{
   if(error) {
      std::cerr << "Error writing data to socket: " << error.message() << std::endl;
      HPCT_LOG(warning) << ": Burstping::sendBurstICMPRequest() - ICMP async_send_to failed!";
   }
   std::cout<< "Bytes send: " << bytes_transferred << std::endl;
}

// ###### Send one ICMP request to given destination ########################
// void Burstping::sendBurstICMPRequest(const DestinationInfo& destination,
//                                  const unsigned int     ttl,
//                                  const unsigned int     round,
//                                  uint32_t&              targetChecksum,
//                                  const unsigned int     payload)
// {
void Burstping::sendBurstICMPRequest(const DestinationInfo& destination,
                     const unsigned int     ttl,
                     const unsigned int     round,
                     std::vector<boost::asio::streambuf::const_buffers_type> buffers,
                     std::vector<ICMPHeader> ICMPHeaderBuffers,
                     std::vector<TraceServiceHeader> TraceServiceHeaderBuffers,
                     std::vector<std::chrono::system_clock::time_point> sendTimeBuffers)
{
   // ====== Set TTL ========================================
   const boost::asio::ip::unicast::hops option(ttl);
   ICMPSocket.set_option(option);
   /**
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
   HPCT_LOG(info) << "Request size: " << request_buffer.size() << std::endl;
   
   //const std::chrono::system_clock::time_point sendTime = std::chrono::system_clock::now();

   // ======== Payload ========================
   //
   **/
   
   // ====== Tweak checksum ===============================
   //computeInternet16(echoRequest, contents.begin(), contents.end());
   //targetChecksum = echoRequest.checksum();
   
   // ====== Encode the request packet ======================
   //boost::asio::streambuf request_buffer;
   //std::ostream os(&request_buffer);
   //os << echoRequest;
   //os.write(reinterpret_cast<char const*>(contents.data()), contents.size());
   //HPCT_LOG(info) << "Request size: " << request_buffer.size() << std::endl;

   HPCT_LOG(info) << "Request size: " << buffers.size() << std::endl;

   uint32_t targetChecksum = ~0U;
   std::chrono::system_clock::time_point sendTime;
   
   // boost::asio::placeholders::bytes_transferred bytes_transferred;
   // unsigned short SeqNumber;
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
         // sent = ICMPSocket.send_to(request_buffer.data(), boost::asio::ip::icmp::endpoint(destination.address(), 0));
         ICMPSocket.async_send_to(buffers, boost::asio::ip::icmp::endpoint(destination.address(), 0), handler);
      }
   }
   catch (boost::system::system_error const& e) {
      sent = -1;
   }
   if(sent < 1) {
      // HPCT_LOG(warning) << getName() << ": Burstping::sendBurstICMPRequest() - ICMP send_to("
      //                   << sendTimeStamp << "->" << destination << ") failed!";
      HPCT_LOG(warning) << getName() << ": Burstping::sendBurstICMPRequest() - Burst ICMP send_to ->" << destination << ") failed!";
   }
   else {
      // ====== Record the request ==========================
      // for(auto t=ICMPHeaderBuffers.begin(); t!=ICMPHeaderBuffers.end(); ++t){
      //    std::cout<< t.seqNumber() << std::endl;
      // }
      
      for (int i=0; i<ICMPHeaderBuffers.size(); i++){
         OutstandingRequests++;
         targetChecksum = ICMPHeaderBuffers.at(i).checksum();
         sendTime = sendTimeBuffers.at(i);
         SeqNumber = ICMPHeaderBuffers.at(i).seqNumber();

         assert((targetChecksum & ~0xffff) == 0);

         ResultEntry resultEntry(round, SeqNumber, ttl, (uint16_t)targetChecksum,
                                    sendTime, destination, Unknown);
         
         std::pair<std::map<unsigned short, ResultEntry>::iterator, bool> result = ResultsMap.insert(std::pair<unsigned short, ResultEntry>(SeqNumber,resultEntry));

         assert(result.second == true);

         // std::cout<< i << " " << SeqNumber << std::endl;
         // std::cout<< i << " " << TraceServiceHeaderBuffers.at(i).sendTimeStamp() << std::endl;
      }
      

      // assert((targetChecksum & ~0xffff) == 0);
      // ResultEntry resultEntry(round, SeqNumber, ttl, (uint16_t)targetChecksum, sendTime,
      //                         destination, Unknown);
      // std::pair<std::map<unsigned short, ResultEntry>::iterator, bool> result = ResultsMap.insert(std::pair<unsigned short, ResultEntry>(SeqNumber,resultEntry));
      // assert(result.second == true);
   }
}

// ###### Handle timer event ################################################
void Burstping::handleIntervalEvent(const boost::system::error_code& errorCode)
{
   if(StopRequested == false) {
      // ====== Prepare new run =============================================
      if(errorCode != boost::asio::error::operation_aborted) {
         HPCT_LOG(debug) << getName() << ": Starting iteration " << (IterationNumber + 1) << " ...";
         prepareRun(true);
         sendRequests();
      }
   }
}

// ###### Schedule interval timer ###########################################
void Burstping::scheduleIntervalEvent()
{
   if((Iterations == 0) || (IterationNumber < Iterations)) {
      std::lock_guard<std::recursive_mutex> lock(DestinationMutex);

      /***
      // ====== Schedule event ==============================================
      long long millisecondsToWait;
      
      const unsigned long long deviation       = std::max(10ULL, Interval / 5);   // 20% deviation
      const unsigned long long waitingDuration = Interval + (std::rand() % deviation);
      const std::chrono::steady_clock::duration howLongToWait =
         (RunStartTimeStamp + std::chrono::milliseconds(waitingDuration)) - std::chrono::steady_clock::now();
      millisecondsToWait = std::max(0LL, (long long)std::chrono::duration_cast<std::chrono::milliseconds>(howLongToWait).count());
      

      //IntervalTimer.expires_from_now(boost::posix_time::milliseconds(millisecondsToWait));
      IntervalTimer.expires_from_now(boost::posix_time::milliseconds(0));
      IntervalTimer.async_wait(std::bind(&Burstping::handleIntervalEvent, this,
                                         std::placeholders::_1));
      HPCT_LOG(debug) << getName() << ": Waiting " << millisecondsToWait / 1000.0
                      << "s before iteration " << (IterationNumber + 1) << " ...";

      **/

      //const unsigned long long deviation = std::max(10ULL, Interval / 5ULL);   // 20% deviation
      //const unsigned long long duration  = Interval + (std::rand() % deviation);
      TimeoutTimer.expires_from_now(boost::posix_time::microseconds(Interval));
      TimeoutTimer.async_wait(std::bind(&Burstping::handleTimeoutEvent, this,
                                     std::placeholders::_1));
      // ====== Check, whether it is time for starting a new transaction ====
      if(ResultsOutput) {
         ResultsOutput->mayStartNewTransaction();
      }
   }
   else {
       // ====== Done -> exit! ==============================================
       StopRequested.exchange(true);
       cancelIntervalTimer();
       cancelTimeoutTimer();
       cancelSocket();
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
      // uint32_t targetChecksum = ~0U;
      for(std::set<DestinationInfo>::const_iterator destinationIterator = Destinations.begin();
          destinationIterator != Destinations.end(); destinationIterator++) {
         const DestinationInfo& destination = *destinationIterator;
         std::vector<ICMPHeader> ICMPHeaderBuffers;
         std::vector<TraceServiceHeader> TraceServiceHeaderBuffers;
         std::vector<std::chrono::system_clock::time_point> sendTimeBuffers;
         std::vector<boost::asio::streambuf::const_buffers_type> buffers;
         for(int i=1; i<=Burstping::burst; i++)
         {
            // ====== Create an ICMP header for an echo request ======
            SeqNumber++;
            ICMPHeader echoRequest;
            echoRequest.type((isIPv6() == true) ? ICMPHeader::IPv6EchoRequest : ICMPHeader::IPv4EchoRequest);
            echoRequest.code(0);
            echoRequest.identifier(Identifier);
            echoRequest.seqNumber(SeqNumber);
            ICMPHeaderBuffers.push_back(echoRequest);
            // HPCT_LOG(info) << "Seq1: " << echoRequest.seqNumber() << std::endl;

            TraceServiceHeader tsHeader;
            tsHeader.magicNumber(MagicNumber);
            tsHeader.sendTTL(FinalMaxTTL);
            tsHeader.round((unsigned char)0);
            tsHeader.checksumTweak(0);
            const std::chrono::system_clock::time_point sendTime = std::chrono::system_clock::now();
            tsHeader.sendTimeStamp(makePacketTimeStamp(sendTime));
            std::vector<unsigned char> tsHeaderContents = tsHeader.contents();
            TraceServiceHeaderBuffers.push_back(tsHeader);
            sendTimeBuffers.push_back(sendTime);

            // ====== Tweak checksum ===============================
            computeInternet16(echoRequest, tsHeaderContents.begin(), tsHeaderContents.end());
            // targetChecksum = echoRequest.checksum();
            // assert((targetChecksum & ~0xffff) == 0);

            // ====== Encode the request packet ======================
            boost::asio::streambuf request_buffer;
            std::ostream os(&request_buffer);
            os << echoRequest << tsHeader;
            int missing_data = Burstping::payload - request_buffer.size();
            if (missing_data > 0){
               std::vector<unsigned char> contents(missing_data, ~0);
               os.write(reinterpret_cast<char const*>(contents.data()), contents.size());
            }
            //buffers.push_back(boost::asio::buffer(request_buffer.data()));
            buffers.push_back(request_buffer.data());

            // HPCT_LOG(info) << "Burst No. " << i << " of payload " << request_buffer.size() << std::endl;
            
            
            //HPCT_LOG(info) << "Burst No. " << i << " of payload " << Burstping::payload << std::endl;
            //sendBurstICMPRequest(destination, FinalMaxTTL, 0, targetChecksum, Burstping::payload);
            
         }
         sendBurstICMPRequest(destination, FinalMaxTTL, 0, buffers, ICMPHeaderBuffers, TraceServiceHeaderBuffers, sendTimeBuffers);
         HPCT_LOG(info) << "Seq2: " << ICMPHeaderBuffers[1].seqNumber() << std::endl;
         HPCT_LOG(info) << "Time2: " << TraceServiceHeaderBuffers[1].sendTimeStamp() << std::endl;
         
      }
      scheduleIntervalEvent();

   }

}