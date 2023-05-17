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

#ifndef IMPORTER_EXCEPTION_H
#define IMPORTER_EXCEPTION_H

#include <exception>
#include <string>


//  Base class for all importer problems (logic, reader, database)
class ImporterException : public std::runtime_error
{
   public:
   ImporterException(const std::string& error) : std::runtime_error(error) { }
};


// Program logic exception
class ImporterLogicException : public ImporterException
{
   public:
   ImporterLogicException(const std::string& error) : ImporterException(error) { }
};


// Generic reader problem
class ImporterReaderException : public ImporterException
{
   public:
   ImporterReaderException(const std::string& error) : ImporterException(error) { }
};


// Problem with input data (syntax error, etc.) => invalid data
class ImporterReaderDataErrorException : public ImporterReaderException
{
   public:
   ImporterReaderDataErrorException(const std::string& error) : ImporterReaderException(error) { }
};


// Generic database problem
class ImporterDatabaseException : public ImporterException
{
   public:
   ImporterDatabaseException(const std::string& error) : ImporterException(error) { }
};


// Problem with database transaction (syntax error, etc.) => invalid data
class ImporterDatabaseDataErrorException : public ImporterDatabaseException
{
   public:
   ImporterDatabaseDataErrorException(const std::string& error) : ImporterDatabaseException(error) { }
};

#endif
