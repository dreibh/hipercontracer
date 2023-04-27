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
// Copyright (C) 2015-2023 by Thomas Dreibholz
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
#include "tools.h"


static boost::asio::ip::udp::endpoint RemoteEndpoint;
static char                           Buffer[65536];


// ###### Handle incoming echo request ######################################
void handleMessage(boost::asio::ip::udp::socket*    udpSocket,
                   const boost::system::error_code& errorCode,
                   std::size_t                      bytesReceived)
{
   if(errorCode != boost::asio::error::operation_aborted) {
      // ====== Send response ===============================================
      if(bytesReceived > 0) {
         // For security reasons, only respond if remote port >= 1024.
         // Otherwise, an attacker could inject a message to cause
         // an echo loop between two echo servers (port 7)!
         if(RemoteEndpoint.port() >= 1024) {
            boost::system::error_code sendErrorCode;
            udpSocket->send_to(boost::asio::buffer(Buffer, bytesReceived),
                              RemoteEndpoint, 0, sendErrorCode);
            if(sendErrorCode != boost::system::errc::success) {
               HPCT_LOG(error) << "Error sending " << bytesReceived << " bytes to "
                               << RemoteEndpoint << ": " << sendErrorCode.message();
            }
#if 0
            else {
               HPCT_LOG(trace) << "Replied " << bytesReceived << " bytes to "
                               << RemoteEndpoint;
            }
#endif
         }
         else {
            HPCT_LOG(warning) << "Ignoring request from " << RemoteEndpoint;
         }
      }

      // ====== Wait for next message =======================================
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
   bool                     logColor;
   std::filesystem::path    logFile;
   std::string              user((getlogin() != nullptr) ? getlogin() : "0");
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
      ( "logfile,O",
           boost::program_options::value<std::filesystem::path>(&logFile)->default_value(std::filesystem::path()),
           "Log file" )
      ( "logcolor,Z",
           boost::program_options::value<bool>(&logColor)->default_value(true),
           "Use ANSI color escape sequences for log output" )
      ( "verbose,v",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::trace),
           "Verbose logging level" )
      ( "quiet,q",
           boost::program_options::value<unsigned int>(&logLevel)->implicit_value(boost::log::trivial::severity_level::warning),
           "Quiet logging level" )
      ( "user,U",
           boost::program_options::value<std::string>(&user),
           "User" )

      ( "address,A",
           boost::program_options::value<std::string>(&localAddressString)->default_value("::"),
           "Address" )
      ( "port,P",
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
   initialiseLogger(logLevel, logColor,
                    (logFile != std::filesystem::path()) ? logFile.string().c_str() : nullptr);
   const passwd* pw = getUser(user.c_str());
   if(pw == nullptr) {
      HPCT_LOG(fatal) << "Cannot find user \"" << user << "\"!";
      return 1;
   }

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


   // ====== Reduce privileges ==============================================
   if(reducePrivileges(pw) == false) {
      HPCT_LOG(fatal) << "Failed to reduce privileges!";
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
