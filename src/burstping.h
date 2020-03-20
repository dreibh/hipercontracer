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

   protected:
   virtual void sendRequests();
   virtual void sendBurstICMPRequest(const DestinationInfo& destination,
                        const unsigned int             ttl,
                        const unsigned int             round,
                        uint32_t&                      targetChecksum,
                        const unsigned int             payload);

   private:
   const std::string BurstpingInstanceName;
   const unsigned int payload;
   const unsigned int burst;
};

#endif
