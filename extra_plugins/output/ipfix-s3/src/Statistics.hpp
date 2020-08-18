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
            ss << "    Total: " << format_rate(total_bytes + bytes_this_measure, total_duration + duration);
        } else {
            ss << "    Total: " << format_rate(total_bytes, total_duration);
        }
        ss << "\n    Last: " << format_rate(last_bytes, last_duration);
        if (measure_in_progress) {
            ss << "\n    Current: " << format_rate(bytes_this_measure, duration);
        }

        duration_t duration_since_first = measure_end - first_measure_start;
        if (measure_in_progress) {
            ss << "\n    Total (since first measure): " << format_rate(total_bytes + bytes_this_measure, duration_since_first);
        } else {
            ss << "\n    Total (since first measure): " << format_rate(total_bytes, duration_since_first);
        }
        return ss.str();
    }
};

#endif // STATISTICS_HPP