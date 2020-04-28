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
// Copyright (C) 2015-2020 by Thomas Dreibholz
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

#ifndef SERVICE_H
#define SERVICE_H

#include "destinationinfo.h"
#include "resultentry.h"

#include <functional>


class Service;

typedef std::function<void(Service* service, const ResultEntry* resultEntry)> ResultCallbackType;


class Service
{
   public:
   Service();
   virtual ~Service();

   virtual const boost::asio::ip::address& getSource() = 0;
   virtual bool addDestination(const DestinationInfo& destination) = 0;
   virtual void setResultCallback(const ResultCallbackType& resultCallback);

   virtual const std::string& getName() const = 0;
   virtual bool start() = 0;
   virtual void requestStop() = 0;
   virtual bool joinable() = 0;
   virtual void join() = 0;

   protected:
   ResultCallbackType ResultCallback;
};

#endif
