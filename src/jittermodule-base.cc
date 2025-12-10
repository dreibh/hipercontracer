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
// Copyright (C) 2015-2026 by Thomas Dreibholz
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

#include "assure.h"
#include "jittermodule-base.h"


//  ###### Jitter Module Registry ###############################################

#include "jittermodule-rfc3550.h"
#include "jittermodule-iqr.h"

REGISTER_JITTERMODULE(0, "RFC3550", JitterModuleRFC3550);
REGISTER_JITTERMODULE(0, "IQR",     JitterModuleIQR);

//  #########################################################################


std::list<RegisteredJitterModule*>* JitterModuleBase::JitterModuleList = nullptr;


// ###### Constructor #######################################################
JitterModuleBase::JitterModuleBase()
{
}


// ###### Destructor #######################################################
JitterModuleBase::~JitterModuleBase()
{
}


// ###### Register Jitter module ################################################
bool JitterModuleBase::registerJitterModule(
   const unsigned int  moduleType,
   const std::string&  moduleName,
   JitterModuleBase*   (*createJitterModuleFunction)(const unsigned int elements))
{
   if(JitterModuleList == nullptr) {
      JitterModuleList = new std::list<RegisteredJitterModule*>;
      assure(JitterModuleList != nullptr);
   }
   RegisteredJitterModule* registeredJitterModule = new RegisteredJitterModule;
   registeredJitterModule->Type                   = moduleType;
   registeredJitterModule->Name                   = moduleName;
   registeredJitterModule->CreateJitterModuleFunction = createJitterModuleFunction;
   JitterModuleList->push_back(registeredJitterModule);
   return true;
}


// ###### Create new Jitter module ##########################################
JitterModuleBase* JitterModuleBase::createJitterModule(const JitterType   moduleType,
                                                       const unsigned int elements)
{
   for(RegisteredJitterModule* registeredJitterModule : *JitterModuleList) {
      if(registeredJitterModule->Type == moduleType) {
         return registeredJitterModule->CreateJitterModuleFunction(elements);
      }
   }
   return nullptr;
}


// ###### Check existence of Jitter module ##################################
const RegisteredJitterModule* JitterModuleBase::checkJitterModule(const std::string& moduleName)
{
   for(RegisteredJitterModule* registeredJitterModule : *JitterModuleList) {
      if(registeredJitterModule->Name == moduleName) {
         return registeredJitterModule;
      }
   }
   return nullptr;
}
