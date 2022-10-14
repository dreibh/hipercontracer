#include <string>
#include <iostream>
#include <functional> //std::bind

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#include <ares.h>
// Examples: http://blog.gerryyang.com/tcp/ip/2022/05/12/c-ares-in-action.html


class DNSReverseLookup
{
   public:
   DNSReverseLookup();
   ~DNSReverseLookup();

   void addAddress(boost::asio::ip::address address);

   void run();

   private:
   static void handleResult(void* arg, int status, int timeouts, struct hostent* host);

   ares_channel Channel;
};


DNSReverseLookup::DNSReverseLookup()
{
   int result = ares_init(&Channel);
   if(result != ARES_SUCCESS) {
      std::cerr << "ERROR: Unable to initialise C-ARES: " << ares_strerror(result) << std::endl;
      ::exit(1);
   }

//    result = ares_set_servers_ports_csv(Channel, "10.193.4.20,10.193.4.21");
//    if(result != ARES_SUCCESS) {
//       std::cerr << "ERROR: Unable to set DNS server addresses: " << ares_strerror(result) << std::endl;
//    }
}


DNSReverseLookup::~DNSReverseLookup()
{
   if(Channel) {
      ares_destroy(Channel);
   }
}


void DNSReverseLookup::addAddress(boost::asio::ip::address address)
{
   boost::asio::ip::tcp::endpoint endpoint;
   endpoint.address(address);
   std::cout << "add: " << endpoint << "\n";

   const uint8_t* ip;
   size_t   ip_size;
   int      ip_family;
   if(address.is_v4()) {
      const boost::asio::ip::address_v4::bytes_type& v4 = address.to_v4().to_bytes();
      ip        = (const uint8_t*)&v4;
      ip_size   = 4;
      ip_family = AF_INET;
      // printf("=> %u.%u.%u.%u\n", ip[0], ip[1], ip[2], ip[3]);
   }
   else {
      assert(address.is_v6());
      const boost::asio::ip::address_v6::bytes_type& v6 = address.to_v6().to_bytes();
      ip        = (const uint8_t*)&v6;
      ip_size   = 16;
      ip_family = AF_INET6;
   }

   ares_gethostbyaddr(Channel, ip, ip_size, ip_family,
                      &DNSReverseLookup::handleResult, this);
}


void DNSReverseLookup::handleResult(void* arg, int status, int timeouts, struct hostent* host)
{
   // DNSReverseLookup* drl = (DNSReverseLookup*)arg;

   if(status == ARES_SUCCESS) {
      boost::asio::ip::address address;
      if(host->h_length == 4) {
         boost::asio::ip::address_v4::bytes_type v4;
         v4.fill(*((unsigned char*)&host->h_addr));
         address = boost::asio::ip::address_v4(v4);
      }
      else {
         assert(host->h_length == 16);
         boost::asio::ip::address_v6::bytes_type v6;
         v6.fill(*((unsigned char*)&host->h_addr));
         address = boost::asio::ip::address_v6(v6);
      }

      std::cout << address << ": " << host->h_name << "\n";
   }
   else {
      std::cout << "Error: " << status << "\n";
   }
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
        tvp = ares_timeout(Channel, NULL, &tv);
        select(nfds, &readers, &writers, NULL, tvp);
        ares_process(Channel, &readers, &writers);
     }
}



int main() {
   DNSReverseLookup drl;

   drl.addAddress(boost::asio::ip::address_v4::from_string("224.244.244.224"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("8.8.8.8"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("9.9.9.9"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("99.99.99.99"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("193.99.144.80"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("1.1.1.1"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("2.2.2.2"));
   drl.addAddress(boost::asio::ip::address_v6::from_string("2a02:2e0:3fe:1001:7777:772e:2:85"));
   drl.addAddress(boost::asio::ip::address_v6::from_string("2606:4700::6810:2c63"));

   drl.run();
   return 0;
}
