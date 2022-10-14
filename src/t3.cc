#include <string>
#include <iostream>

#include <boost/asio.hpp>
#include <boost/bind.hpp>


class DNSReverseLookup
{
   public:
   DNSReverseLookup();
   ~DNSReverseLookup();

   void addAddress(boost::asio::ip::address address);

   void run();

   private:
   void handleResult(const boost::system::error_code&             errorCode,
                     boost::asio::ip::tcp::resolver::results_type results,
                     boost::asio::ip::tcp::endpoint               endpoint);

   boost::asio::io_service        IOService;
   boost::asio::ip::tcp::resolver Resolver;
};


DNSReverseLookup::DNSReverseLookup()
   : Resolver(IOService)
{
}


DNSReverseLookup::~DNSReverseLookup()
{
}


void DNSReverseLookup::addAddress(boost::asio::ip::address address)
{
   boost::asio::ip::tcp::endpoint endpoint;
   endpoint.address(address);
   std::cout << "add: " << endpoint << "\n";

   Resolver.async_resolve(endpoint,
                          boost::bind(&DNSReverseLookup::handleResult, this,
                                      boost::asio::placeholders::error,
                                      boost::asio::placeholders::iterator,
                                      endpoint));
}


void DNSReverseLookup::handleResult(const boost::system::error_code&             errorCode,
                                    boost::asio::ip::tcp::resolver::results_type results,
                                    boost::asio::ip::tcp::endpoint               endpoint)
{
   if(!errorCode) {
      boost::asio::ip::tcp::resolver::results_type::const_iterator iterator;
      for(iterator = results.begin(); iterator != results.end(); iterator++) {
         std::cout << endpoint << ": " << iterator->host_name() << "\n";
      }
   }
   else {
      std::cout << "Error: " << errorCode << "\n";
   }
}


void DNSReverseLookup::run()
{
   IOService.run();
}



int main() {
   DNSReverseLookup drl;

   drl.addAddress(boost::asio::ip::address_v4::from_string("224.244.244.224"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("8.8.8.8"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("9.9.9.9"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("99.99.99.99"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("1.1.1.1"));
   drl.addAddress(boost::asio::ip::address_v4::from_string("2.2.2.2"));

   drl.run();
   return 0;
}
