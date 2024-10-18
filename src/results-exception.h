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

#ifndef RESULTS_EXCEPTION_H
#define RESULTS_EXCEPTION_H

#include <stdexcept>
#include <string>


//  Base class for all importer problems (logic, reader, database)
class ResultsException : public std::runtime_error
{
   public:
   ResultsException(const std::string& error) : std::runtime_error(error) { }
};


// Program logic exception
class ResultsLogicException : public ResultsException
{
   public:
   ResultsLogicException(const std::string& error) : ResultsException(error) { }
};


// Generic reader problem
class ResultsReaderException : public ResultsException
{
   public:
   ResultsReaderException(const std::string& error) : ResultsException(error) { }
};


// Problem with input data (syntax error, etc.) => invalid data
class ResultsReaderDataErrorException : public ResultsReaderException
{
   public:
   ResultsReaderDataErrorException(const std::string& error) : ResultsReaderException(error) { }
};


// Generic database problem
class ResultsDatabaseException : public ResultsException
{
   public:
   ResultsDatabaseException(const std::string& error) : ResultsException(error) { }
};


// Problem with database transaction (syntax error, etc.) => invalid data
class ResultsDatabaseDataErrorException : public ResultsDatabaseException
{
   public:
   ResultsDatabaseDataErrorException(const std::string& error) : ResultsDatabaseException(error) { }
};

#endif
