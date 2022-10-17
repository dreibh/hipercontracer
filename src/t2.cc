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

class DNSLookup;

struct TimeRecord {
   std::chrono::system_clock::time_point FirstSeen;
   std::chrono::system_clock::time_point LastUpdate;
   std::chrono::system_clock::time_point ValidUntil;   // LastUpdate + TTL
//       sendTime = std::chrono::system_clock::now();
};

struct AddressInfo {
   DNSLookup*        Owner;
   int               Status;
   std::string       Name;
   TimeRecord        Validity;
};

struct NameInfo {
   DNSLookup*                                       Owner;
   int                                              Status;
   std::string                                      Location;
   std::map<boost::asio::ip::address, AddressInfo*> Addresses;
};

class DNSLookup
{
   public:
   DNSLookup();
   ~DNSLookup();

   void queryName(const std::string& name,
                  const unsigned int dnsclass = ns_c_in,
                  const unsigned int type     = ns_t_a);
   void queryAddress(const boost::asio::ip::address& address);

   void run();

   private:
   AddressInfo* getOrCreateAddressInfo(const boost::asio::ip::address& address,
                                       const bool                      mustBeNew = false);
   NameInfo* getOrCreateNameInfo(const std::string& name,
                                 const bool         mustBeNew = false);
   void updateNameToAddressMapping(NameInfo*                       nameInfo,
                                   const std::string&              name,
                                   const boost::asio::ip::address& address);
   void updateAddressToNameMapping(AddressInfo*                    addressInfo,
                                   const boost::asio::ip::address& address,
                                   const std::string&              name);

   static void handlePtrResult(void* arg, int status, int timeouts, struct hostent* host);
   static void handleGenericResult(void* arg, int status, int timeouts, unsigned char* abuf, int alen);

   static double rfc1867_size(const uint8_t* aptr);
   static double rfc1867_angle(const uint8_t* aptr);


   std::map<boost::asio::ip::address, AddressInfo*> AddressInfoMap;
   std::map<std::string, NameInfo*>                 NameInfoMap;

   ares_channel Channel;
};



DNSLookup::DNSLookup()
{
   ares_options options;
   options.flags = ARES_FLAG_USEVC;   // DNS over TCP
   int optmask = ARES_OPT_FLAGS;

   int result = ares_init_options(&Channel, &options, optmask);
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


AddressInfo* DNSLookup::getOrCreateAddressInfo(const boost::asio::ip::address& address,
                                               const bool                      mustBeNew)
{
   std::map<boost::asio::ip::address, AddressInfo*>::iterator found = AddressInfoMap.find(address);
   if(found == AddressInfoMap.end()) {
      // ====== Create AddressInfo ==========================================
      AddressInfo* addressInfo = new AddressInfo;
      assert(addressInfo != nullptr);
      addressInfo->Owner  = this;
      addressInfo->Status = -1;

      AddressInfoMap.insert(std::pair<boost::asio::ip::address, AddressInfo*>(address, addressInfo));
      return addressInfo;
   }
   else {
      if(!mustBeNew) {
         return found->second;
      }
   }
   return nullptr;
}


NameInfo* DNSLookup::getOrCreateNameInfo(const std::string& name,
                                         const bool         mustBeNew)
{
   std::map<std::string, NameInfo*>::iterator found = NameInfoMap.find(name);
   if(found == NameInfoMap.end()) {
      // ====== Create NameInfo ==========================================
      NameInfo* nameInfo = new NameInfo;
      assert(nameInfo != nullptr);
      nameInfo->Owner  = this;
      nameInfo->Status = -1;

      NameInfoMap.insert(std::pair<std::string, NameInfo*>(name, nameInfo));
      return nameInfo;
   }
   else {
      if(!mustBeNew) {
         return found->second;
      }
   }
   return nullptr;
}


void DNSLookup::updateNameToAddressMapping(NameInfo*                       nameInfo,
                                           const std::string&              name,
                                           const boost::asio::ip::address& address)
{
   if(nameInfo == nullptr) {
    abort(); //??? FIXME!
      nameInfo = getOrCreateNameInfo(name);
   }
   AddressInfo* addressInfo = getOrCreateAddressInfo(address);
}


void DNSLookup::updateAddressToNameMapping(AddressInfo*                    addressInfo,
                                           const boost::asio::ip::address& address,
                                           const std::string&              name)
{
   if(addressInfo == nullptr) {
    abort(); //??? FIXME!
      addressInfo = getOrCreateAddressInfo(address);
   }
   NameInfo* nameInfo = getOrCreateNameInfo(name);

}


void DNSLookup::queryName(const std::string& name, const unsigned int dnsclass, const unsigned int type)
{
   NameInfo* nameInfo = getOrCreateNameInfo(name, true);
   if(nameInfo) {
      // ====== Query DNS ===================================================
      ares_query(Channel, name.c_str(), dnsclass, type,
                 &DNSLookup::handleGenericResult, nameInfo);
   }
}


void DNSLookup::queryAddress(const boost::asio::ip::address& address)
{
   AddressInfo* addressInfo = getOrCreateAddressInfo(address, true);
   if(addressInfo) {
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
   }
}


void DNSLookup::handlePtrResult(void* arg, int status, int timeouts, struct hostent* host)
{
   AddressInfo* addressInfo = (AddressInfo*)arg;
   DNSLookup*   dnsLookup   = addressInfo->Owner;

   addressInfo->Status = status;
   if(host != nullptr) {
      addressInfo->Name = host->h_name;

      NameInfo* nameInfo = dnsLookup->getOrCreateNameInfo(addressInfo->Name);
      assert(nameInfo != nullptr);
   }
}


void DNSLookup::handleGenericResult(void* arg, int status, int timeouts, unsigned char* abuf, int alen)
{
   NameInfo*  nameInfo  = (NameInfo*)arg;
   DNSLookup* dnsLookup = nameInfo->Owner;

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
                  dnsLookup->updateNameToAddressMapping(nameInfo, name, a4);
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
                   dnsLookup->updateNameToAddressMapping(nameInfo, name, a6);
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


void DNSLookup::run()
{
   int nfds;
   fd_set readers, writers;
   timeval tv, *tvp;
   while(true) {
      FD_ZERO(&readers);
      FD_ZERO(&writers);
      nfds = ares_fds(Channel, &readers, &writers);
      if(nfds == 0) {
         break;
      }
      tvp = ares_timeout(Channel, nullptr, &tv);
      select(nfds, &readers, &writers, nullptr, tvp);
      ares_process(Channel, &readers, &writers);
    }
}



int main() {
   DNSLookup drl;

   drl.queryAddress(boost::asio::ip::address_v4::from_string("224.244.244.224"));
   drl.queryAddress(boost::asio::ip::address_v4::from_string("8.8.4.4"));
   drl.queryAddress(boost::asio::ip::address_v4::from_string("8.8.8.8"));
   drl.queryAddress(boost::asio::ip::address_v4::from_string("9.9.9.9"));
   drl.queryAddress(boost::asio::ip::address_v4::from_string("99.99.99.99"));
   drl.queryAddress(boost::asio::ip::address_v4::from_string("193.99.144.80"));
   drl.queryAddress(boost::asio::ip::address_v4::from_string("1.1.1.1"));
   drl.queryAddress(boost::asio::ip::address_v4::from_string("2.2.2.2"));
   drl.queryAddress(boost::asio::ip::address_v4::from_string("158.37.0.1"));
   drl.queryAddress(boost::asio::ip::address_v4::from_string("128.39.0.1"));
   drl.queryAddress(boost::asio::ip::address_v6::from_string("2606:4700::6810:2c63"));
   drl.queryAddress(boost::asio::ip::address_v6::from_string("2a02:2e0:3fe:1001:7777:772e:2:85"));
   drl.queryAddress(boost::asio::ip::address_v6::from_string("2a02:26f0:5200::b81f:f78"));

   drl.queryName("ringnes.fire.smil.",   ns_c_in, ns_t_loc);
   drl.queryName("oslo-gw1.uninett.no.", ns_c_in, ns_t_loc);

   drl.queryName("ringnes.fire.smil.",   ns_c_in, ns_t_any);
   drl.queryName("oslo-gw1.uninett.no.", ns_c_in, ns_t_a);
   drl.queryName("www.nntb.no.",         ns_c_in, ns_t_any);

   drl.run();
   return 0;
}
