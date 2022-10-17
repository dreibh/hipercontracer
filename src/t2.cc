#include <string>
#include <iostream>
#include <set>
#include <map>

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <ares.h>
#include <ares_dns.h>

#include <arpa/nameser.h>


// Examples: http://blog.gerryyang.com/tcp/ip/2022/05/12/c-ares-in-action.html


class DNSLookup
{
   public:
   DNSLookup();
   ~DNSLookup();

   void addAddress(const boost::asio::ip::address& address);

   void queryRR(const char* name, int dnsclass, int type);

   void run();

   private:
   static void handlePtrResult(void* arg, int status, int timeouts, struct hostent* host);
   static void handleGenericResult(void* arg, int status, int timeouts, unsigned char* abuf, int alen);
//    void updateAddressInfo(const boost::asio::ip::address& address,
//                           const int                       status,
//                           const hostent*                  host);

   static double rfc1867_size(const uint8_t* aptr);
   static double rfc1867_angle(const uint8_t* aptr);

   struct AddressInfo {
      // DNSLookup* Owner;
      int               Status;
      std::string       Name;
   };
   std::map<boost::asio::ip::address, AddressInfo*> AddressInfoMap;

   struct TTLRecord {
      unsigned int TTL;
   };

   struct NameInfo {
      // DNSLookup* Owner;
      int                                            Status;
      std::string                                    Location;
      std::map<boost::asio::ip::address, TTLRecord*> Addresses;
   };
   std::map<std::string, NameInfo*> NameInfoMap;

   ares_channel Channel;
};



DNSLookup::DNSLookup()
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


DNSLookup::~DNSLookup()
{
   unsigned int i = 0;

   std::cout << "AddressInfoMap:" << std::endl;
   for(std::map<boost::asio::ip::address, AddressInfo*>::iterator addressInfoIterator = AddressInfoMap.begin();
       addressInfoIterator != AddressInfoMap.end(); addressInfoIterator++) {
      const AddressInfo* addressInfo = addressInfoIterator->second;
      std::cout << ++i << "\t" << addressInfoIterator->first << " -> "
                << addressInfo->Name
                << " (status " << addressInfo->Status << ")"
                << std::endl;
   }

   std::cout << "NameInfoMap:" << std::endl;
   i = 0;
   for(std::map<std::string, NameInfo*>::iterator nameInfoIterator = NameInfoMap.begin();
       nameInfoIterator != NameInfoMap.end(); nameInfoIterator++) {
      const NameInfo* nameInfo = nameInfoIterator->second;
      std::cout << ++i << "\t" << nameInfoIterator->first << " -> "
                << nameInfo->Location
                << " (status " << nameInfo->Status << ")"
                << std::endl;
   }

   if(Channel) {
      ares_destroy(Channel);
   }
}


void DNSLookup::addAddress(const boost::asio::ip::address& address)
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
                            &DNSLookup::handlePtrResult, addressInfo);
      }
      else {
         assert(address.is_v6());
         const boost::asio::ip::address_v6::bytes_type& v6 = address.to_v6().to_bytes();
         ares_gethostbyaddr(Channel, v6.data(), 16, AF_INET6,
                            &DNSLookup::handlePtrResult, addressInfo);
      }

      AddressInfoMap.insert(std::pair<boost::asio::ip::address, AddressInfo*>(address, addressInfo));
   }
   else {
      std::cout << "Already there: " << address << std::endl;
   }
}


void DNSLookup::handlePtrResult(void* arg, int status, int timeouts, struct hostent* host)
{
   AddressInfo*  addressInfo = (AddressInfo*)arg;
   // DNSLookup* drl         = addressInfo->Owner;

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


// void DNSLookup::updateAddressInfo(const boost::asio::ip::address& address,
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


double DNSLookup::rfc1867_size(const uint8_t* aptr)
{
  const uint8_t value    = *aptr;
  double        size     = (value & 0xF0) >> 4;
  unsigned int  exponent = (value & 0x0F);
  while(exponent != 0) {
    size *= 10;
    exponent--;
  }
  return size / 100;  /* return size in meters, not cm */
}


double DNSLookup::rfc1867_angle(const uint8_t* aptr)
{
   const uint32_t angle = DNS__32BIT(aptr);
   if(angle < 0x80000000U) {   // West
      return -(double)(0x80000000U - angle) /  (1000 * 60 * 60);
   }
   else {   // East
      return (double)(angle - 0x80000000U) /  (1000 * 60 * 60);
   }
}


void DNSLookup::handleGenericResult(void* arg, int status, int timeouts, unsigned char* abuf, int alen)
{
   NameInfo* nameInfo = (NameInfo*)arg;

   nameInfo->Status = status;
   if(status == ARES_SUCCESS) {
      if(alen >= NS_HFIXEDSZ) {
         const unsigned int questions = DNS_HEADER_QDCOUNT(abuf);
         const unsigned int answers   = DNS_HEADER_ANCOUNT(abuf);
#if 0
         for(size_t i = 0; i < (size_t)alen; i++) {
            printf("%02x ", abuf[i]);
         }
         puts("");
#endif
         const unsigned char* aptr = abuf + NS_HFIXEDSZ;
         char*                name = nullptr;

         // ====== Iterate questions ========================================
         // printf("questions=%d\n", questions);
         for(unsigned int i = 0; i < questions; i++) {
            // ====== Parse RR header =======================================
            long len;
            int success = ares_expand_name(aptr, abuf, alen, &name, &len);
            if(success != ARES_SUCCESS) {
               goto done;
            }
            aptr += len;
            if (aptr + QFIXEDSZ > abuf + alen) {
               goto done;
            }
#if 0
            const unsigned int type     = DNS_QUESTION_TYPE(aptr);
            const unsigned int dnsclass = DNS_QUESTION_CLASS(aptr);
            printf("Question %u/%u for %s: class=%u, type=%u\n",
                   i + 1, questions, name, dnsclass, type);
#endif
            aptr += QFIXEDSZ;
         }


         // ====== Iterate answers ==========================================
         // printf("answers=%d\n", answers);
         for(unsigned int i = 0; i < answers; i++) {
            // ====== Parse RR header =======================================
            long len;
            int success = ares_expand_name(aptr, abuf, alen, &name, &len);
            if(success != ARES_SUCCESS) {
               goto done;
            }
            aptr += len;
            if (aptr + RRFIXEDSZ > abuf + alen) {
               goto done;
            }
            const unsigned int type     = DNS_RR_TYPE(aptr);
            const unsigned int dnsclass = DNS_RR_CLASS(aptr);
            const unsigned int ttl      = DNS_RR_TTL(aptr);
            const unsigned int dlen     = DNS_RR_LEN(aptr);
            aptr += RRFIXEDSZ;
            printf("Answer %u/%u for %s: class=%u, type=%u, dlen=%u, ttl=%u\n", i + 1,
                   answers, name, dnsclass, type, dlen, ttl);
            if(aptr + dlen > abuf + alen) {
               goto done;
            }

            // ====== Parse RR contents =====================================
            switch (type) {
               // ------ A RR -----------------------------------------------
               case ns_t_a: {
                  if(dlen < 4) {
                     goto done;
                  }
                  boost::asio::ip::address_v4::bytes_type v4;
                  std::copy((uint8_t*)&aptr[0], (uint8_t*)&aptr[4], v4.begin());
                  const boost::asio::ip::address_v4 a4(v4);
                  printf("A for %s: %s\n", name, a4.to_string().c_str());
                }
                break;
               // ------ AAAA RR --------------------------------------------
               case ns_t_aaaa: {
                  if(dlen < 16) {
                     goto done;
                   }
                   boost::asio::ip::address_v6::bytes_type v6;
                   std::copy((uint8_t*)&aptr[0], (uint8_t*)&aptr[16], v6.begin());
                   const boost::asio::ip::address_v6 a6(v6);
                   printf("AAAA for %s: %s\n", name, a6.to_string().c_str());
                }
                break;
               // ------ LOC RR ---------------------------------------------
               case ns_t_loc: {
                  if(dlen < 16) {
                     goto done;
                  }
                  const uint8_t version = *(aptr + 0x00);
                   if(version == 0) {
                      const double size       = rfc1867_size(aptr + 0x01);
                      const double hprecision = rfc1867_size(aptr + 0x02);
                      const double vprecision = rfc1867_size(aptr + 0x03);
                      const double latitude   = rfc1867_angle(aptr + 0x04);
                      const double longitude  = rfc1867_angle(aptr + 0x08);
                      printf("LOC for %s: lat=%f, lon=%f, size=%f, hp=%f, vp=%f\n",
                             name, latitude, longitude, size, hprecision, vprecision);
                  }
                }
                break;
               // ------ CNAME RR -------------------------------------------
               case ns_t_cname: {
                  char* cname;
                  long  cnamelen;
                  status = ares_expand_name(aptr, abuf, alen, &cname, &cnamelen);
                  if(status != ARES_SUCCESS) {
                     goto done;
                  }
                  printf("CNAME for %s: %s\n", name, cname);
                  ares_free_string(cname);
                }
                break;
            }

            if(name) {
               ares_free_string(name);
               name = nullptr;
            }
            aptr += dlen;
         }

         // ====== Clean up =================================================
done:
         if(name) {
            ares_free_string(name);
            name = nullptr;
         }
      }
   }
}

void DNSLookup::queryRR(const char* name, int dnsclass, int type)
{
   NameInfo*                                            nameInfo;
   std::map<std::string, NameInfo*>::iterator found = NameInfoMap.find(name);
   if(found == NameInfoMap.end()) {
      std::cout << "add: " << name << std::endl;

      // ====== Create NameInfo =============================================
      nameInfo = new NameInfo;
      assert(nameInfo != nullptr);
      // nameInfo->Owner  = this;
      nameInfo->Status = -1;
   }
   else {
      nameInfo = found->second;
   }

   ares_query(Channel, name, dnsclass, type, &DNSLookup::handleGenericResult, nameInfo);

   NameInfoMap.insert(std::pair<std::string, NameInfo*>(name, nameInfo));
}


void DNSLookup::run()
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
   DNSLookup drl;

   drl.addAddress(boost::asio::ip::address_v4::from_string("224.244.244.224"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("8.8.4.4"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("8.8.8.8"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("9.9.9.9"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("99.99.99.99"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("193.99.144.80"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("1.1.1.1"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("2.2.2.2"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("158.37.0.1"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("128.39.0.1"));
   drl.addAddress(boost::asio::ip::address_v6::from_string("2606:4700::6810:2c63"));
   drl.addAddress(boost::asio::ip::address_v6::from_string("2a02:2e0:3fe:1001:7777:772e:2:85"));
   drl.addAddress(boost::asio::ip::address_v6::from_string("2a02:26f0:5200::b81f:f78"));

   drl.queryRR("ringnes.fire.smil.",   ns_c_in, ns_t_loc);
   drl.queryRR("oslo-gw1.uninett.no.", ns_c_in, ns_t_loc);

   drl.queryRR("ringnes.fire.smil.",   ns_c_in, ns_t_any);
   drl.queryRR("oslo-gw1.uninett.no.", ns_c_in, ns_t_a);
   drl.queryRR("www.nntb.no.",         ns_c_in, ns_t_any);

   drl.run();
   return 0;
}
