#include <string>
#include <iostream>
#include <map>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <ares.h>
// Examples: http://blog.gerryyang.com/tcp/ip/2022/05/12/c-ares-in-action.html


class DNSReverseLookup
{
   public:
   DNSReverseLookup();
   ~DNSReverseLookup();

   void addAddress(const boost::asio::ip::address& address);

   void queryRR(const char* name, int dnsclass, int type);

   void run();

   private:
   static void handlePtrResult(void* arg, int status, int timeouts, struct hostent* host);
   static void handleGenericResult(void* arg, int status, int timeouts, unsigned char* abuf, int alen);
//    void updateAddressInfo(const boost::asio::ip::address& address,
//                           const int                       status,
//                           const hostent*                  host);

   struct AddressInfo {
      // DNSReverseLookup* Owner;
      int               Status;
      std::string       Name;
   };
   std::map<boost::asio::ip::address, AddressInfo*> AddressInfoMap;

   ares_channel Channel;
};



DNSReverseLookup::DNSReverseLookup()
{
   int result = ares_init(&Channel);
   if(result != ARES_SUCCESS) {
      std::cerr << "ERROR: Unable to initialise C-ARES: " << ares_strerror(result) << std::endl;
      ::exit(1);
   }

   result = ares_set_servers_ports_csv(Channel, "10.193.4.20,10.193.4.21");
   if(result != ARES_SUCCESS) {
      std::cerr << "ERROR: Unable to set DNS server addresses: " << ares_strerror(result) << std::endl;
   }
}


DNSReverseLookup::~DNSReverseLookup()
{
   unsigned int i = 0;
   for(std::map<boost::asio::ip::address, AddressInfo*>::iterator addressInfoIterator = AddressInfoMap.begin();
       addressInfoIterator != AddressInfoMap.end(); addressInfoIterator++) {
      const AddressInfo* adressInfo = addressInfoIterator->second;
      std::cout << ++i << "\t" << addressInfoIterator->first << " -> "
                << adressInfo->Name
                << " (status " << adressInfo->Status << ")"
                << std::endl;
   }

   if(Channel) {
      ares_destroy(Channel);
   }
}


void DNSReverseLookup::addAddress(const boost::asio::ip::address& address)
{
   std::map<boost::asio::ip::address, AddressInfo*>::iterator found = AddressInfoMap.find(address);
   if(found == AddressInfoMap.end()) {
      std::cout << "add: " << address << std::endl;

      // ====== Create AddressInfo ==========================================
      AddressInfo* addressInfo = new AddressInfo;
      assert(addressInfo != nullptr);
      // addressInfo->Owner  = this;
      addressInfo->Status = -1;

      // ====== Query DNS ===================================================
      if(address.is_v4()) {
         const boost::asio::ip::address_v4::bytes_type& v4 = address.to_v4().to_bytes();
         ares_gethostbyaddr(Channel, v4.data(), 4, AF_INET,
                            &DNSReverseLookup::handlePtrResult, addressInfo);
      }
      else {
         assert(address.is_v6());
         const boost::asio::ip::address_v6::bytes_type& v6 = address.to_v6().to_bytes();
         ares_gethostbyaddr(Channel, v6.data(), 16, AF_INET6,
                            &DNSReverseLookup::handlePtrResult, addressInfo);
      }

      AddressInfoMap.insert(std::pair<boost::asio::ip::address, AddressInfo*>(address, addressInfo));
   }
   else {
      std::cout << "Already there: " << address << std::endl;
   }
}


void DNSReverseLookup::handlePtrResult(void* arg, int status, int timeouts, struct hostent* host)
{
   AddressInfo*  addressInfo = (AddressInfo*)arg;
   // DNSReverseLookup* drl         = addressInfo->Owner;

   addressInfo->Status = status;
   if(host != nullptr) {
      addressInfo->Name = host->h_name;
/*
      if(host->h_length == 4) {
         boost::asio::ip::address_v4::bytes_type v4;
         std::copy((uint8_t*)&host->h_addr_list[0][0], (uint8_t*)&host->h_addr_list[0][4], v4.begin());
         drl->updateAddressInfo(boost::asio::ip::address_v4(v4), status, host);
      }
      else {
         assert(host->h_length == 16);
         boost::asio::ip::address_v6::bytes_type v6;
         std::copy((uint8_t*)&host->h_addr_list[0][0], (uint8_t*)&host->h_addr_list[0][16], v6.begin());
         drl->updateAddressInfo(boost::asio::ip::address_v6(v6), status, host);
      }
*/
   }
}


// void DNSReverseLookup::updateAddressInfo(const boost::asio::ip::address& address,
//                                          const int                       status,
//                                          const hostent*                  host)
// {
//    std::map<boost::asio::ip::address, AddressInfo*>::iterator found = AddressInfoMap.find(address);
//    if(found != AddressInfoMap.end()) {
//       AddressInfo* addressInfo = found->second;
//       addressInfo->Status = status;
//       addressInfo->Name   = host->h_name;
//
//       std::cout << address << ": " << addressInfo->Name << std::endl;
//    }
//    else {
//       std::cout << "Update for unknwon address " << address << "!" << std::endl;
//    }
// }


void DNSReverseLookup::handleGenericResult(void* arg, int status, int timeouts, unsigned char* abuf, int alen)
{
   printf("S=%d\n", status);
   if(status == ARES_SUCCESS) {
      // ...
   }
}

void DNSReverseLookup::queryRR(const char* name, int dnsclass, int type)
{
   ares_query(Channel, name, dnsclass, type, &DNSReverseLookup::handleGenericResult, NULL);
}


void DNSReverseLookup::run()
{
    int nfds;
    fd_set readers, writers;
    timeval tv, *tvp;
    while (1) {
        FD_ZERO(&readers);
        FD_ZERO(&writers);
        nfds = ares_fds(Channel, &readers, &writers);
        if (nfds == 0)
          break;
        tvp = ares_timeout(Channel, nullptr, &tv);
        select(nfds, &readers, &writers, nullptr, tvp);
        ares_process(Channel, &readers, &writers);
     }
}



int main() {
   DNSReverseLookup drl;

   drl.addAddress(boost::asio::ip::address_v4::from_string("224.244.244.224"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("8.8.4.4"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("8.8.8.8"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("9.9.9.9"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("99.99.99.99"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("193.99.144.80"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("1.1.1.1"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("2.2.2.2"));
   drl.addAddress(boost::asio::ip::address_v6::from_string("2606:4700::6810:2c63"));
   drl.addAddress(boost::asio::ip::address_v6::from_string("2a02:2e0:3fe:1001:7777:772e:2:85"));
   drl.addAddress(boost::asio::ip::address_v6::from_string("2a02:26f0:5200::b81f:f78"));

   drl.queryRR("ringnes.fire.smil", 1, 29);   // IN LOC

   drl.run();
   return 0;
}
