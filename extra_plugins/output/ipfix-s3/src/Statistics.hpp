/**
 * \file extra_plugins/output/ipfix-s3/src/Statistics.hpp
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Statistics counter
 * \date 2020
 */

/* Copyright (C) 2020 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */


#ifndef STATISTICS_HPP
#define STATISTICS_HPP

#include <chrono>
#include <sstream>
#include <iomanip>

class Statistics {
    using duration_t = std::chrono::steady_clock::duration;

private:
    bool first_measure = false;
    std::chrono::time_point<std::chrono::steady_clock> first_measure_start;

    duration_t total_duration = duration_t::zero();
    std::size_t total_bytes = 0;
    
    duration_t last_duration;
    std::size_t last_bytes = 0;
    
    std::chrono::time_point<std::chrono::steady_clock> measure_start;
    std::size_t bytes_this_measure;

    bool measure_in_progress = false;

public:
    void start_measure()
    {
        if (measure_in_progress) {
            stop_measure();
        }
        measure_in_progress = true;
        measure_start = std::chrono::steady_clock::now();
        if (!first_measure) {
            first_measure_start = measure_start;
            first_measure = true;
        }
        bytes_this_measure = 0;
    }

    void add_bytes(std::size_t count)
    {
        if (!measure_in_progress) {
            return;
        }
        bytes_this_measure += count;
    }

    void stop_measure()
    {
        if (!measure_in_progress) {
            return;
        }
        auto measure_end = std::chrono::steady_clock::now();
        auto duration = measure_end - measure_start;
        last_duration = duration;
        last_bytes = bytes_this_measure;
        total_duration += duration;
        total_bytes += bytes_this_measure;
        measure_in_progress = false;
    }

    std::string to_human_bytes(double bytes)
    {
        int unit = 0;
        const std::vector<std::string> units = {
            "B", "kiB", "MiB", "GiB", "TiB", "PiB" 
        };
        while (bytes >= 1024 && unit < units.size()) {
            unit++;
            bytes /= 1024;
        }
        std::stringstream ss;
        ss << bytes << " " << units[unit];
        return ss.str();
    }

    std::string to_human_time(duration_t duration)
    {
        std::stringstream ss;
        auto hours = std::chrono::duration_cast<std::chrono::hours>(duration).count();
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration).count() % 60;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count() % 60;
        auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() % 1000;

        ss << hours << ":" << minutes << ":" << seconds << "." << milliseconds << std::fixed << std::setprecision(4) << milliseconds;

        return ss.str();
    }

    std::string format_rate(std::size_t bytes, duration_t duration)
    {
        std::stringstream ss;
        auto msecs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        double secs = msecs / 1000.0;
        ss << to_human_bytes(bytes) << " in " << secs << "s";
        if (secs == 0) {
            ss << " (N/A)";
        } else {
            ss << " (" << to_human_bytes(bytes / secs) << "/s)";
        }
        return ss.str();
    }

    std::string to_string()
    {
        auto measure_end = std::chrono::steady_clock::now();        
        duration_t duration = measure_end - measure_start;

        std::stringstream ss;
        if (measure_in_progress) {
            ss << "    Total (cumulative): " << format_rate(total_bytes + bytes_this_measure, total_duration + duration);
        } else {
            ss << "    Total (cumulative): " << format_rate(total_bytes, total_duration);
        }
        ss << "\n    Last: " << format_rate(last_bytes, last_duration);
        if (measure_in_progress) {
            ss << "\n    Current: " << format_rate(bytes_this_measure, duration);
        }

        duration_t duration_since_first = measure_end - first_measure_start;
        if (measure_in_progress) {
            ss << "\n    Total (real): " << format_rate(total_bytes + bytes_this_measure, duration_since_first);
        } else {
            ss << "\n    Total (real): " << format_rate(total_bytes, duration_since_first);
        }
        return ss.str();
    }
};

#endif // STATISTICS_HPP