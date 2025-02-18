#include "log_reader.h"
#include "command_caller.h"
#include "log_record.h"

#include <sstream>

LogReader::LogReader(const std::initializer_list<std::string> &log_sources)
{
  for (const std::string &source : log_sources) {
    log_files.emplace_front(source, std::ifstream::in);
  }
}

std::list<std::shared_ptr<LogRecord>> LogReader::read_logs()
{
  std::list<std::shared_ptr<LogRecord>> logs;

  std::string log_data;
  for (std::ifstream &log_file : log_files) {
    while (log_file.is_open() && std::getline(log_file, log_data)) {
      auto log = std::make_shared<LogRecord>(log_data);

      if (log->valid()) {
        logs.push_back(log);
      }
    }
  }

  append_audit_logs(logs);
  return logs;
}

void LogReader::append_audit_logs(std::list<std::shared_ptr<LogRecord>> &log_list)
{
  std::string output = CommandCaller::get_logs(checkpoint_filepath);
  std::istringstream stream(output);

  if (checkpoint_filepath.empty()) {
    std::getline(stream, checkpoint_filepath);
  }

  std::string line;
  while (std::getline(stream, line)) {
    auto log = std::make_shared<LogRecord>(line);

    if (log->valid()) {
      log_list.push_back(log);
    }
  }
}
