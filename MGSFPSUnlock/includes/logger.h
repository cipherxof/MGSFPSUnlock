#ifndef SIZE_LIMITED_SINK_H
#define SIZE_LIMITED_SINK_H

#define FMT_UNICODE 0
#define SPDLOG_EOL "\n"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/base_sink.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Spdlog sink (truncate on startup, single file)
template<typename Mutex>
class size_limited_sink : public spdlog::sinks::base_sink<Mutex>
{
public:
    explicit size_limited_sink(const std::string& filename, size_t max_size)
        : _filename(filename), _max_size(max_size)
    {
        truncate_log_file();

        _file.open(_filename, std::ios::app);
        if (!_file.is_open())
        {
            throw spdlog::spdlog_ex("Failed to open log file " + filename);
        }
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        if (fs::exists(_filename) && fs::file_size(_filename) >= _max_size)
        {
            return;
        }

        spdlog::memory_buf_t formatted;
        this->formatter_->format(msg, formatted);

        _file.write(formatted.data(), formatted.size());
        _file.flush();
    }

    void flush_() override
    {
        _file.flush();
    }

private:
    std::ofstream _file;
    std::string _filename;
    size_t _max_size;

    void truncate_log_file()
    {
        if (fs::exists(_filename))
        {
            std::ofstream ofs(_filename, std::ofstream::out | std::ofstream::trunc);
            ofs.close();
        }
    }
};

#endif // SIZE_LIMITED_SINK_H
