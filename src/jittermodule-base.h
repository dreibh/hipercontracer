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

#ifndef JITTERMODULE_BASE_H
#define JITTERMODULE_BASE_H

#include <list>

#include "resultentry.h"


class JitterModuleBase;

struct RegisteredJitterModule {
   std::string  Name;
   unsigned int Type;
   JitterModuleBase* (*CreateJitterModuleFunction)();
};


class JitterModuleBase
{
   public:
   JitterModuleBase();
   virtual ~JitterModuleBase();

   virtual const JitterType   getJitterType() const = 0;
   virtual const std::string& getJitterName() const = 0;

   virtual unsigned int       packets()       const = 0;
   virtual unsigned long long jitter()        const = 0;
   virtual unsigned long long meanLatency()   const = 0;
   virtual void process(const uint8_t            timeSource,
                        const unsigned long long sendTimeStamp,
                        const unsigned long long receiveTimeStamp) = 0;

   static bool registerJitterModule(const unsigned int moduleType,
                                    const std::string& moduleName,
                                    JitterModuleBase* (*createJitterModuleFunction)());
   static JitterModuleBase* createJitterModule(const JitterType moduleType);
   static const RegisteredJitterModule* checkJitterModule(const std::string& moduleName);

   private:
   static std::list<RegisteredJitterModule*>* JitterModuleList;
};

#define REGISTER_JITTERMODULE(moduleType, moduleName, jitterModule) \
   static JitterModuleBase* createJitterModule_##jitterModule() { \
      return new jitterModule(); \
   } \
   static bool Registered_##jitterModule = JitterModuleBase::registerJitterModule(moduleType, moduleName, createJitterModule_##jitterModule);

#endif
