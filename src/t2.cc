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

struct AddressInfo;
struct NameInfo;
class DNSLookup;

enum QueryFlags
{
   IQF_NONE              = 0,
   IQF_QUERY_SENT        = (1 << 0),
   IQF_RESPONSE_RECEIVED = (1 << 1)
};

struct TimeRecord {
   std::chrono::system_clock::time_point FirstSeen;
   std::chrono::system_clock::time_point LastUpdate;
   std::chrono::system_clock::time_point ValidUntil;   // LastUpdate + TTL
//       sendTime = std::chrono::system_clock::now();
};

struct AddressInfo {
   DNSLookup*                       Owner;
   boost::asio::ip::address         Address;
   unsigned int                     Flags;
   int                              Status;
   std::string                      Name;
   TimeRecord                       Validity;
   std::map<std::string, NameInfo*> NameInfoMap;
};

struct NameInfo {
   DNSLookup*                                       Owner;
   unsigned int                                     Flags;
   int                                              Status;
   std::string                                      Location;
   std::map<boost::asio::ip::address, AddressInfo*> AddressInfoMap;
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
   void dumpNameInfoMap(std::ostream& os) const;
   void dumpAddressInfoMap(std::ostream& os) const;

   static void handlePtrResult(void* arg, int status, int timeouts, struct hostent* host);
   static void handleGenericResult(void* arg, int status, int timeouts, unsigned char* abuf, int alen);

   static std::string makeFQDN(const std::string& name);
   static double rfc1867_size(const uint8_t* aptr);
   static double rfc1867_angle(const uint8_t* aptr);


   std::map<boost::asio::ip::address, AddressInfo*> AddressInfoMap;
   std::map<std::string, NameInfo*>                 NameInfoMap;
   ares_channel                                     Channel;
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
   dumpAddressInfoMap(std::cout);
   dumpNameInfoMap(std::cout);
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
      addressInfo->Owner   = this;
      addressInfo->Address = address;
      addressInfo->Status  = -1;
      addressInfo->Flags   = IQF_NONE;

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
      nameInfo->Flags  = IQF_NONE;

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
   assert(nameInfo != nullptr);

   AddressInfo* addressInfo = getOrCreateAddressInfo(address);
   assert(addressInfo != nullptr);
   nameInfo->AddressInfoMap.insert(std::pair<boost::asio::ip::address, AddressInfo*>(address, addressInfo));
}


void DNSLookup::updateAddressToNameMapping(AddressInfo*                    addressInfo,
                                           const boost::asio::ip::address& address,
                                           const std::string&              name)
{
   assert(addressInfo != nullptr);
   addressInfo->Name = makeFQDN(name);

   NameInfo* nameInfo = getOrCreateNameInfo(addressInfo->Name);
   assert(nameInfo != nullptr);
   addressInfo->NameInfoMap.insert(std::pair<std::string, NameInfo*>(name, nameInfo));
}


void DNSLookup::dumpNameInfoMap(std::ostream& os) const
{
   os << "NameInfoMap:" << std::endl;
   unsigned int n = 1;
   for(std::map<std::string, NameInfo*>::const_iterator nameInfoIterator = NameInfoMap.begin();
       nameInfoIterator != NameInfoMap.end(); nameInfoIterator++) {
      const NameInfo* nameInfo = nameInfoIterator->second;
      os << n++ << ":\t" << nameInfoIterator->first << " -> "
         << nameInfo->Location;

      for(std::map<boost::asio::ip::address, AddressInfo*>::const_iterator addressInfoIterator = nameInfo->AddressInfoMap.begin();
          addressInfoIterator != nameInfo->AddressInfoMap.end(); addressInfoIterator++) {
         // const AddressInfo* addressInfo = addressInfoIterator->second;
         os << addressInfoIterator->first << " ";
         // os << " (ttl=" << addressInfo->Validity
      }

      os << " (status " << nameInfo->Status << ")"
         << std::endl;
   }
}


void DNSLookup::dumpAddressInfoMap(std::ostream& os) const
{
   std::cout << "AddressInfoMap:" << std::endl;
   unsigned int n = 1;
   for(std::map<boost::asio::ip::address, AddressInfo*>::const_iterator addressInfoIterator = AddressInfoMap.begin();
       addressInfoIterator != AddressInfoMap.end(); addressInfoIterator++) {
      const AddressInfo* addressInfo = addressInfoIterator->second;
      std::cout << n++ << ":\t" << addressInfoIterator->first << " -> "
                << addressInfo->Name
                << " (status " << addressInfo->Status << ")"
                << std::endl;
   }
}


void DNSLookup::queryName(const std::string& name, const unsigned int dnsclass, const unsigned int type)
{
   const std::string fqdn     = makeFQDN(name);
   NameInfo*         nameInfo = getOrCreateNameInfo(fqdn);
   if( (nameInfo) && (!(nameInfo->Flags & IQF_QUERY_SENT)) ) {
      // ====== Query DNS ===================================================
      ares_query(Channel, fqdn.c_str(), dnsclass, type,
                 &DNSLookup::handleGenericResult, nameInfo);
      nameInfo->Flags |= IQF_QUERY_SENT;
   }
   else {
      std::cout << "Already queried " << fqdn << std::endl;
   }
}


void DNSLookup::queryAddress(const boost::asio::ip::address& address)
{
   AddressInfo* addressInfo = getOrCreateAddressInfo(address);
   if( (addressInfo) && (!(addressInfo->Flags & IQF_QUERY_SENT)) ) {
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
      addressInfo->Flags |= IQF_QUERY_SENT;
   }
}


void DNSLookup::handlePtrResult(void* arg, int status, int timeouts, struct hostent* host)
{
   AddressInfo* addressInfo = (AddressInfo*)arg;
   DNSLookup*   dnsLookup   = addressInfo->Owner;

   addressInfo->Status = status;
   addressInfo->Flags |= IQF_RESPONSE_RECEIVED;
   if(host != nullptr) {
      dnsLookup->updateAddressToNameMapping(addressInfo, addressInfo->Address, host->h_name);
      dnsLookup->queryName(addressInfo->Name);
   }
}


void DNSLookup::handleGenericResult(void* arg, int status, int timeouts, unsigned char* abuf, int alen)
{
   NameInfo*  nameInfo  = (NameInfo*)arg;
   DNSLookup* dnsLookup = nameInfo->Owner;

   nameInfo->Status = status;
   nameInfo->Flags |= IQF_RESPONSE_RECEIVED;
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


std::string DNSLookup::makeFQDN(const std::string& name)
{
   if(name[name.size() - 1] != '.') {
      return name + '.';
   }
   return name;
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

   drl.queryName("ringnes.fire.smil.",   ns_c_in, ns_t_aaaa);
   drl.queryName("ringnes.fire.smil.",   ns_c_in, ns_t_a);
   drl.queryName("ringnes.fire.smil.",   ns_c_in, ns_t_loc);

   drl.queryName("oslo-gw1.uninett.no.", ns_c_in, ns_t_a);
   drl.queryName("oslo-gw1.uninett.no.", ns_c_in, ns_t_loc);

   drl.queryName("mack.fire.smil.",      ns_c_in, ns_t_any);
   drl.queryName("hansa.fire.smil.",     ns_c_in, ns_t_any);
   drl.queryName("oslo-gw1.uninett.no.", ns_c_in, ns_t_aaaa);
   drl.queryName("www.nntb.no.",         ns_c_in, ns_t_any);

   drl.run();
   return 0;
}
