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
// Copyright (C) 2015-2019 by Thomas Dreibholz
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

#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/shared_ptr.hpp>


boost::shared_ptr<boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>>
   MySink(new boost::log::sinks::synchronous_sink<boost::log::sinks::text_ostream_backend>);

BOOST_LOG_GLOBAL_LOGGER_INIT(MyLogger, boost::log::sources::severity_logger_mt) {
   boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> MyLogger;

   // ====== Additional attributes ==========================================
   // MyLogger.add_attribute("LineID",    boost::log::attributes::counter<unsigned int>(1));     // lines are sequentially numbered
   // MyLogger.add_attribute("ThreadID",  boost::log::attributes::current_thread_id());
   MyLogger.add_attribute("TimeStamp", boost::log::attributes::local_clock());             // each log line gets a timestamp

   // ====== Create text sink ===============================================
   MySink->locked_backend()->add_stream(boost::shared_ptr<std::ostream>(&std::clog, boost::null_deleter()));

   // ====== Coloring expression ============================================
   const auto colorisationExpression =
      boost::log::expressions::stream << boost::log::expressions::if_(boost::log::trivial::severity <= boost::log::trivial::severity_level::trace)[ boost::log::expressions::stream << "\x1b[37m" ].else_[
         boost::log::expressions::stream << boost::log::expressions::if_(boost::log::trivial::severity == boost::log::trivial::severity_level::debug)[ boost::log::expressions::stream << "\x1b[36m" ].else_[
            boost::log::expressions::stream << boost::log::expressions::if_(boost::log::trivial::severity == boost::log::trivial::severity_level::info)[ boost::log::expressions::stream << "\x1b[34m" ].else_[
               boost::log::expressions::stream << boost::log::expressions::if_(boost::log::trivial::severity == boost::log::trivial::severity_level::warning)[ boost::log::expressions::stream << "\x1b[33m" ].else_[
                  boost::log::expressions::stream << boost::log::expressions::if_(boost::log::trivial::severity == boost::log::trivial::severity_level::error)[ boost::log::expressions::stream << "\x1b[31;1m" ].else_[
                     boost::log::expressions::stream << "\x1b[37;41;1m"   // Fatal
      ]]]]];

   // ====== Formatting expression ==========================================
    const boost::log::formatter formatter =
       colorisationExpression
        /*
        << std::setw(7) << std::setfill('0') << boost::log::expressions::attr< unsigned int >("LineID") << std::setfill(' ') << " "
        << std::setw(7) << std::setfill(' ') << boost::log::expressions::attr< boost::log::attributes::current_thread_id::value_type >("ThreadID") << std::setfill(' ') << " | "
        */
        << boost::log::expressions::format_date_time(boost::log::expressions::attr< boost::posix_time::ptime >("TimeStamp"),
                                  "[%Y-%m-%d %H:%M:%S.%f]")
        << "[" << boost::log::trivial::severity << "]"
        << ": " << boost::log::expressions::smessage
        << "\x1b[0m";
    MySink->set_formatter(formatter);

    boost::log::core::get()->add_sink(MySink);
    return MyLogger;
}


// ###### Initialise logger #################################################
void initialiseLogger(const unsigned int logLevel)
{
   // ====== Set filter ====================================================
   MySink->set_filter(boost::log::trivial::severity >= logLevel);

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
