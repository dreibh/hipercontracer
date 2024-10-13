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
// Copyright (C) 2015-2024 by Thomas Dreibholz
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

#include "service.h"


// ###### Constructor #######################################################
Service::Service(ResultsWriter*                resultsWriter,
                 const char*                   outputFormatName,
                 const OutputFormatVersionType outputFormatVersion,
                 const unsigned int            iterations) :
   ResultsOutput(resultsWriter),
   OutputFormatName(outputFormatName),
   OutputFormatVersion(outputFormatVersion),
   Iterations(iterations)
{
   ResultCallback = nullptr;
}


// ###### Destructor ########################################################
Service::~Service()
{
}


// ###### Set result callback ###############################################
void Service::setResultCallback(const ResultCallbackType& resultCallback)
{
   ResultCallback = resultCallback;
}


// ###### Prepare service start #############################################
bool Service::prepare(const bool privileged)
{
   if(privileged == false) {
      // No special privileges needed for preparing files.
      if(ResultsOutput != nullptr) {
         return ResultsOutput->prepare();
      }
   }
   return true;
}
