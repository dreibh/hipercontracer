#ifndef BURSTPING_H
#define BURSTPING_H

#include "ping.h"


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
   virtual void requestStop();

   protected:
   virtual void sendRequests();
   virtual void scheduleIntervalEvent();
   virtual void processResults();
   virtual void handleIntervalEvent(const boost::system::error_code& errorCode);
   virtual void sendBurstICMPRequest(const DestinationInfo& destination,
                        const unsigned int             ttl,
                        const unsigned int             round,
                        uint32_t&                      targetChecksum,
                        const unsigned int             payload);

   private:
   const std::string BurstpingInstanceName;
   const unsigned int Payload;
   const unsigned int Burst;
   unsigned int TotalPackets;
   unsigned int TotalResponses;
   static int comparePingResults(const ResultEntry* a, const ResultEntry* b);
};

#endif
