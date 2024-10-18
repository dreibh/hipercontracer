// ==========================================================================
//     _   _ _ ____            ____          _____
//    | | | (_)  _ \ ___ _ __ / ___|___  _ _|_   _| __ __ _  ___ ___ _ __
//    | |_| | | |_) / _ \ '__| |   / _ \| '_ \| || '__/ _` |/ __/ _ \ '__|
//    |  _  | |  __/  __/ |  | |__| (_) | | | | || | | (_| | (_|  __/ |
//    |_| |_|_|_|   \___|_|   \____\___/|_| |_|_||_|  \__,_|\___\___|_|
//
//       ---  High-Performance Connectivity Tracer (HiPerConTracer)  ---
//                 https://www.nntb.no/~dreibh/hipercontracer/
// ==========================================================================
//
// High-Performance Connectivity Tracer (HiPerConTracer)
// Copyright (C) 2015-2025 by Thomas Dreibholz
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


class UDPEchoInstance
{
   public:
   UDPEchoInstance(boost::asio::io_service&        ioService,
                   const boost::asio::ip::address& localAddress,
                   const uint16_t                  localPort);
   ~UDPEchoInstance();

   void handleMessage(const boost::system::error_code& errorCode,
                      std::size_t                      bytesReceived);

   boost::asio::io_service&             IOService;
   const boost::asio::ip::udp::endpoint LocalEndpoint;
   boost::asio::ip::udp::socket         UDPSocket;
   boost::asio::ip::udp::endpoint       RemoteEndpoint;
   char                                 Buffer[65536];
};


// ###### Constructor #######################################################
UDPEchoInstance::UDPEchoInstance(boost::asio::io_service&        ioService,
                                 const boost::asio::ip::address& localAddress,
                                 const uint16_t                  localPort)
   : IOService(ioService),
     LocalEndpoint(localAddress, localPort),
     UDPSocket(IOService, (localAddress.is_v6() == true) ?
                              boost::asio::ip::udp::v6() :
                              boost::asio::ip::udp::v4())
{
   UDPSocket.bind(LocalEndpoint);
   UDPSocket.async_receive_from(boost::asio::buffer(Buffer, sizeof(Buffer)),
                                RemoteEndpoint,
                                std::bind(&UDPEchoInstance::handleMessage,
                                          this,
                                          std::placeholders::_1,
                                          std::placeholders::_2));
}


// ###### Destructor ########################################################
UDPEchoInstance::~UDPEchoInstance()
{
}


// ###### Handle incoming echo request ######################################
void UDPEchoInstance::handleMessage(const boost::system::error_code& errorCode,
                                    std::size_t                      bytesReceived)
{
   if(errorCode != boost::asio::error::operation_aborted) {
      // ====== Send response ===============================================
      if(bytesReceived > 0) {
         // Security:
         // * For security reasons, only respond if remote port >= 1024.
         //   Otherwise, an attacker could inject a message to cause
         //   an echo loop between two echo servers (port 7)!
         // * Also ignore requests from same port as local port!
         if( (RemoteEndpoint.port() >= 1024) &&
             (RemoteEndpoint.port() != LocalEndpoint.port()) ) {
            boost::system::error_code sendErrorCode;
            UDPSocket.send_to(boost::asio::buffer(Buffer, bytesReceived),
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
      UDPSocket.async_receive_from(boost::asio::buffer(Buffer, sizeof(Buffer)),
                                   RemoteEndpoint,
                                   std::bind(&UDPEchoInstance::handleMessage,
                                             this,
                                             std::placeholders::_1,
                                             std::placeholders::_2));
   }
}


// ###### Handle SIGINT #####################################################
static void signalHandler(boost::asio::io_service*         ioService,
                          const boost::system::error_code& errorCode,
                          int                              signalNumber)
{
   std::cout << "\nGot signal " << signalNumber << "\n";
   ioService->stop();
}



// ###### Main program ######################################################
int main(int argc, char *argv[])
{
   // ====== Initialize =====================================================
   unsigned int             logLevel;
   bool                     logColor;
   std::filesystem::path    logFile;
   std::string              user((getlogin() != nullptr) ? getlogin() : "0");
   std::string              localAddressString;
   boost::asio::ip::address localAddress;
   uint16_t                 localPortFrom;
   uint16_t                 localPortTo;

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
      ( "from-port,port,f,p",
           boost::program_options::value<uint16_t>(&localPortFrom)->default_value(7),
           "Port" )
      ( "to-port,t",
           boost::program_options::value<uint16_t>(&localPortTo)->default_value(0),
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
      std::cerr << "ERROR: Bad parameter: " << e.what() << "\n";
      return 1;
   }

   if(localPortTo == 0) {
      localPortTo = localPortFrom;
   }
   if( (localPortTo < 1) || (localPortFrom > localPortTo) || (localPortTo > 65535) ) {
      std::cerr << "ERROR: Invalid port range " << localPortFrom << " - " << localPortTo << "\n";
      return 1;
   }
   const unsigned int ports = 1U + localPortTo - localPortFrom;

   try {
      localAddress = boost::asio::ip::address::from_string(localAddressString);
   }
   catch(std::exception& e) {
      std::cerr << "ERROR: Invalid address: " << e.what() << "\n";
      return 1;
   }

   if(vm.count("help")) {
       std::cerr << "Usage: " << argv[0] << " parameters" << "\n"
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

   // ------ Handle SIGINT --------------------------------------------------
   boost::asio::io_service ioService;
   boost::asio::signal_set signals(ioService, SIGINT);
   signals.async_wait(std::bind(&signalHandler, &ioService,
                                std::placeholders::_1,
                                std::placeholders::_2));

   // ------ Create UDP instances -------------------------------------------
   UDPEchoInstance*        udpEchoInstance[ports];
   for(unsigned int p = 0; p  < ports; p++) {
      const uint16_t localPort = p + localPortFrom;
      try {
         udpEchoInstance[p] = new UDPEchoInstance(ioService, localAddress, localPort);
      }
      catch(std::exception& e) {
         HPCT_LOG(fatal) << "Unable to bind UDP socket to source address "
                        << localAddress << ":" << localPort << ": " << e.what();
         return 1;
      }
   }
   if(ports > 1) {
      HPCT_LOG(info) << "Listening on UDP ports " << localPortFrom << " to " << localPortTo;
   }
   else {
      HPCT_LOG(info) << "Listening on UDP port " << localPortFrom;
   }

   // ====== Reduce privileges ==============================================
   if(reducePrivileges(pw) == false) {
      HPCT_LOG(fatal) << "Failed to reduce privileges!";
      return 1;
   }

   // ====== Main loop ======================================================
   ioService.run();

   // ====== Clean up =======================================================
   for(unsigned int p = 0; p  < ports; p++) {
      delete udpEchoInstance[p];
      udpEchoInstance[p] = nullptr;
   }
   return 0;
}
