// =================================================================
//          #     #                 #     #
//          ##    #   ####   #####  ##    #  ######   #####
//          # #   #  #    #  #    # # #   #  #          #
//          #  #  #  #    #  #    # #  #  #  #####      #
//          #   # #  #    #  #####  #   # #  #          #
//          #    ##  #    #  #   #  #    ##  #          #
//          #     #   ####   #    # #     #  ######     #
//
//       ---   The NorNet Testbed for Multi-Homed Systems  ---
//                       https://www.nntb.no
// =================================================================
//
// High-Performance Connectivity Tracer (HiPerConTracer)
// Copyright (C) 2015-2022 by Thomas Dreibholz
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Contact: dreibh@simula.no

#include <iostream>
#include <boost/asio.hpp>
#include <boost/program_options.hpp>

#include "logger.h"


static boost::asio::ip::udp::endpoint RemoteEndpoint;
static char                           Buffer[65536];


void handleMessage(boost::asio::ip::udp::socket*    udpSocket,
                   const boost::system::error_code& errorCode,
                   std::size_t                      bytesReceived)
{
   if(errorCode != boost::asio::error::operation_aborted) {
      // For security reasons, only respond if remote port != 7.
      // Otherwise, an attacker could inject a message to cause
      // an echo loop between two echo servers!
      if(RemoteEndpoint.port() != 7) {
         udpSocket->send_to(boost::asio::buffer(Buffer, bytesReceived),
                            RemoteEndpoint);
      }
      else {
         HPCT_LOG(warning) << "Ignoring request from " << RemoteEndpoint;
      }
      udpSocket->async_receive_from(boost::asio::buffer(Buffer, sizeof(Buffer)),
                                    RemoteEndpoint,
                                    std::bind(&handleMessage,
                                              udpSocket,
                                              std::placeholders::_1,
                                              std::placeholders::_2));
   }
}



int main(int argc, char *argv[])
{
   // ====== Initialize =====================================================
   unsigned int             logLevel;
   std::string              localAddressString;
   boost::asio::ip::address localAddress;
   uint16_t                 localPort;

   boost::program_options::options_description commandLineOptions;
   commandLineOptions.add_options()
      ( "help,h",
           "Print help message" )

      ( "loglevel,L",
           boost::program_options::value<unsigned int>(&logLevel)->default_value(boost::log::trivial::severity_level::info),
           "Set logging level" )
      ( "verbose,v",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::trace),
           "Verbose logging level" )
      ( "quiet,q",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::warning),
           "Quiet logging level" )

      ( "address,A",
           boost::program_options::value<std::string>(&localAddressString)->default_value("::"),
           "Address" )
      ( "port",
           boost::program_options::value<uint16_t>(&localPort)->default_value(7),
           "Port" )
    ;

   // ====== Handle command-line arguments ==================================
   boost::program_options::variables_map vm;
   try {
      boost::program_options::store(boost::program_options::command_line_parser(argc, argv).
                                       style(
                                          boost::program_options::command_line_style::style_t::default_style|
                                          boost::program_options::command_line_style::style_t::allow_long_disguise
                                       ).
                                       options(commandLineOptions).
                                       run(), vm);
      boost::program_options::notify(vm);
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: Bad parameter: " << e.what() << std::endl;
      return 1;
   }

   try {
      localAddress = boost::asio::ip::address::from_string(localAddressString);
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: Invalid address: " << e.what() << std::endl;
      return 1;
   }

   if(vm.count("help")) {
       std::cerr << "Usage: " << argv[0] << " parameters" << std::endl
                 << commandLineOptions;
       return 1;
   }


   // ====== Initialize =====================================================
   initialiseLogger(logLevel);

   boost::asio::io_service        ioService;
   boost::asio::ip::udp::endpoint localEndpoint(localAddress, localPort);
   boost::asio::ip::udp::socket   udpSocket(ioService, (localAddress.is_v6() == true) ?
                                                          boost::asio::ip::udp::v6() :
                                                          boost::asio::ip::udp::v4());
   boost::system::error_code      errorCode;

   udpSocket.bind(localEndpoint, errorCode);
   if(errorCode !=  boost::system::errc::success) {
      HPCT_LOG(fatal) << "Unable to bind UDP socket to source address "
                      << localEndpoint << ": " << errorCode.message();
      return 1;
   }


   // ====== Main loop ======================================================
   udpSocket.async_receive_from(boost::asio::buffer(Buffer, sizeof(Buffer)),
                                RemoteEndpoint,
                                std::bind(&handleMessage,
                                          &udpSocket,
                                          std::placeholders::_1,
                                          std::placeholders::_2));
   ioService.run();

   return 0;
}
