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

#include "logger.h"

#include <boost/shared_ptr.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>


BOOST_LOG_GLOBAL_LOGGER_INIT(MyLogger, boost::log::sources::severity_logger_mt) {
   boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> MyLogger;

   // ====== Additional attributes ==========================================
   // MyLogger.add_attribute("LineID",    boost::log::attributes::counter<unsigned int>(1));   // Lines are sequentially numbered
   // MyLogger.add_attribute("ThreadID",  boost::log::attributes::current_thread_id());        // Thread ID
   MyLogger.add_attribute("TimeStamp", boost::log::attributes::utc_clock());                   // Each log line gets a UTC timestamp
   return MyLogger;
}


// ###### Initialise logger #################################################
void initialiseLogger(const unsigned int logLevel,
                      const bool         logColor,
                      const char*        logFile
                      // const char*        logTarget
                     )
{
   boost::shared_ptr<boost::log::core> core = boost::log::core::get();

   // ====== Formatting expressions =========================================
   const static boost::log::formatter coloredFormatter =
      boost::log::expressions::stream << boost::log::expressions::if_(boost::log::trivial::severity <= boost::log::trivial::severity_level::trace)[ boost::log::expressions::stream << "\x1b[37m" ].else_[
         boost::log::expressions::stream << boost::log::expressions::if_(boost::log::trivial::severity == boost::log::trivial::severity_level::debug)[ boost::log::expressions::stream << "\x1b[36m" ].else_[
            boost::log::expressions::stream << boost::log::expressions::if_(boost::log::trivial::severity == boost::log::trivial::severity_level::info)[ boost::log::expressions::stream << "\x1b[34m" ].else_[
               boost::log::expressions::stream << boost::log::expressions::if_(boost::log::trivial::severity == boost::log::trivial::severity_level::warning)[ boost::log::expressions::stream << "\x1b[33m" ].else_[
                  boost::log::expressions::stream << boost::log::expressions::if_(boost::log::trivial::severity == boost::log::trivial::severity_level::error)[ boost::log::expressions::stream << "\x1b[31;1m" ].else_[
                     boost::log::expressions::stream << "\x1b[37;41;1m"   // Fatal
       ]]]]]
       /*
       << std::setw(7) << std::setfill('0') << boost::log::expressions::attr< unsigned int >("LineID") << std::setfill(' ') << " "
       << std::setw(7) << std::setfill(' ') << boost::log::expressions::attr< boost::log::attributes::current_thread_id::value_type >("ThreadID") << std::setfill(' ') << " | "
       */
       << boost::log::expressions::format_date_time(boost::log::expressions::attr< boost::posix_time::ptime >("TimeStamp"),
                                 "[%Y-%m-%d %H:%M:%S.%f]")
       << "[" << boost::log::trivial::severity << "]"
       << ": " << boost::log::expressions::smessage
       << "\x1b[0m";
   const static boost::log::formatter plainFormatter =
      boost::log::expressions::stream
       << boost::log::expressions::format_date_time(boost::log::expressions::attr< boost::posix_time::ptime >("TimeStamp"),
                                 "[%Y-%m-%d %H:%M:%S.%f]")
       << "[" << boost::log::trivial::severity << "]"
       << ": " << boost::log::expressions::smessage;

   // ====== Log file output ================================================
   if(logFile != nullptr) {
      boost::shared_ptr<boost::log::sinks::text_file_backend> backend =
         boost::make_shared<boost::log::sinks::text_file_backend>(
            boost::log::keywords::file_name             = logFile,
            boost::log::keywords::open_mode             = std::ios_base::out | std::ios_base::app
            // boost::log::keywords::target_file_name      = logTarget,
            // boost::log::keywords::enable_final_rotation = false,
            // boost::log::keywords::scan_method           = boost::log::sinks::file::scan_matching,
            // boost::log::keywords::max_files             = 10,
            // boost::log::keywords::rotation_size         = 2048,
            // boost::log::keywords::time_based_rotation   = boost::log::sinks::file::rotation_at_time_point(0, 0, 0)
         );
      boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>> fileSink(new boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend>(backend));

      fileSink->set_formatter((logColor == true) ? coloredFormatter : plainFormatter);
      fileSink->set_filter(boost::log::trivial::severity >= logLevel);
      core->add_sink(fileSink);
   }

   // ====== Console output =================================================
   else {
      boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>>
         consoleSink(new boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>());
      consoleSink->locked_backend()->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));
      consoleSink->set_formatter((logColor == true) ? coloredFormatter : plainFormatter);
      consoleSink->set_filter(boost::log::trivial::severity >= logLevel);
      core->add_sink(consoleSink);
   }

   HPCT_LOG(trace) << "Initialised logger";

#if 0
   HPCT_LOG(fatal)   << "FATAL";
   HPCT_LOG(error)   << "Error";
   HPCT_LOG(warning) << "Warning";
   HPCT_LOG(info)    << "Info";
   HPCT_LOG(debug)   << "Debug";
   HPCT_LOG(trace)   << "Trace";
#endif
}
