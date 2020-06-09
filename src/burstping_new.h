#ifndef BURSTPING_H
#define BURSTPING_H

#include "ping.h"
#include "traceserviceheader.h"


class Burstping : public Ping
{
   public:
   Burstping(ResultsWriter*              resultsWriter,
        const unsigned int               iterations,
        const bool                       removeDestinationAfterRun,
        const boost::asio::ip::address&  sourceAddress,
        const std::set<DestinationInfo>& destinationArray,
        const unsigned long long         interval   =  1000,
        const unsigned int               expiration = 10000,
        const unsigned int               ttl        =    64,
        const unsigned int               payload    =    56,
        const unsigned int               burst      =    1);
   virtual ~Burstping();

   virtual const std::string& getName() const;

   protected:
   virtual void sendRequests();
   virtual void scheduleIntervalEvent();
   virtual void handleIntervalEvent(const boost::system::error_code& errorCode);
   // virtual void sendBurstICMPRequest(const DestinationInfo& destination,
   //                      const unsigned int             ttl,
   //                      const unsigned int             round,
   //                      uint32_t&                      targetChecksum,
   //                      const unsigned int             payload);
   virtual void sendBurstICMPRequest(const DestinationInfo& destination, 
                  const unsigned int             ttl,
                  const unsigned int             round,
                  std::vector<boost::asio::streambuf::const_buffers_type> buffers,
                  std::vector<ICMPHeader> ICMPHeaderBuffers,
                  std::vector<TraceServiceHeader> TraceServiceHeaderBuffers,
                  std::vector<std::chrono::system_clock::time_point> sendTimeBuffers);
   static void handler(
      const boost::system::error_code& error, // Result of operation.
      std::size_t bytes_transferred           // Number of bytes sent.
   );
   private:
   const std::string BurstpingInstanceName;
   const unsigned int payload;
   const unsigned int burst;
};

#endif
