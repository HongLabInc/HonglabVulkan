#pragma once

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <cassert>
#include <string>
#include <format>

namespace hlab {

using namespace std;

class Logger
{
  private:
    static unique_ptr<Logger> instance;

    ofstream logFile;
    size_t messagesProcessed;

    // Private constructor for singleton
    Logger() : messagesProcessed(0)
    {
        logFile.open("log.txt", ios::out | ios::trunc);

        if (!logFile.is_open()) {
            cerr << "ERROR: Could not open log.txt for writing!" << endl;
        }
    }

    // Delete copy constructor and assignment operator
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    template <typename T>
    void formatToStream(ostringstream& oss, T&& arg)
    {
        oss << forward<T>(arg);
    }

    template <typename T, typename... Args>
    void formatToStream(ostringstream& oss, T&& first, Args&&... args)
    {
        oss << forward<T>(first);
        if (sizeof...(args) > 0) {
            oss << " ";
            formatToStream(oss, forward<Args>(args)...);
        }
    }

  public:
    ~Logger()
    {
        if (logFile.is_open()) {
            logFile.flush();
            logFile.close();
        }
    }

    static Logger& getInstance()
    {
        if (!instance) {
            instance = unique_ptr<Logger>(new Logger());
        }
        return *instance;
    }

    static void printLog(string message)
    {
        auto& logger = getInstance();

        cout << message << endl;

        if (logger.logFile.is_open()) {
            logger.logFile << message << endl;
            logger.logFile.flush(); // Ensure immediate write
            logger.messagesProcessed++;
        } else {
            cerr << "WARNING: Log file is not open, message lost: " << message << endl;
        }
    }
};

template <typename... Args>
void printLog(std::format_string<Args...> fmt, Args&&... args)
{
    string message = std::format(fmt, std::forward<Args>(args)...);
    Logger::printLog(message);
}

template <typename... Args>
void exitWithMessage(std::format_string<Args...> fmt, Args&&... args)
{
    string message = std::format(fmt, std::forward<Args>(args)...);
    Logger::printLog(message);
    assert(false);
    exit(EXIT_FAILURE);
}

} // namespace hlab