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

#ifndef SERVICE_H
#define SERVICE_H

#include "destinationinfo.h"
#include "resultentry.h"
#include "resultswriter.h"

#include <functional>


enum OutputFormatVersionType
{
   OFT_HiPerConTracer_Version1 = 1,
   OFT_HiPerConTracer_Version2 = 2,

   OFT_Min = OFT_HiPerConTracer_Version1,
   OFT_Max = OFT_HiPerConTracer_Version2
};


class Service;

typedef std::function<void(Service* service, const ResultEntry* resultEntry)> ResultCallbackType;


class Service
{
   public:
   Service(ResultsWriter*                resultsWriter,
           const char*                   outputFormatName,
           const OutputFormatVersionType outputFormatVersion,
           const unsigned int            iterations);
   virtual ~Service();

   virtual const boost::asio::ip::address& getSource() = 0;
   virtual bool addDestination(const DestinationInfo& destination) = 0;
   virtual void setResultCallback(const ResultCallbackType& resultCallback);

   virtual const std::string& getName() const = 0;
   virtual bool prepare(const bool privileged);
   virtual bool start() = 0;
   virtual void requestStop() = 0;
   virtual bool joinable() = 0;
   virtual void join() = 0;

   protected:
   ResultCallbackType            ResultCallback;
   ResultsWriter*                ResultsOutput;
   const std::string             OutputFormatName;
   const OutputFormatVersionType OutputFormatVersion;
   const unsigned int            Iterations;
};

#endif
