#include "logs_controller.h"
#include "../model/database.h"
#include "../model/status_column_record.h"
#include "../view/logs.h"
#include <glibmm/main.h>
#include <glibmm/priorities.h>
#include <memory>
#include <sigc++/functors/mem_fun.h>
#include <sigc++/functors/ptr_fun.h>
#include <sstream>

#include <iostream>

template<class LogsTab, class Database, class Adapter>
std::string LogsController<LogsTab, Database, Adapter>::format_log_data(const std::string &data)
{
  const std::regex remove_quotes = std::regex("\\\"(\\S*)\\\"");
  std::smatch m;
  std::regex_search(data, m, remove_quotes);
  return m[1];
}

template<class LogsTab, class Database, class Adapter>
void
LogsController<LogsTab, Database, Adapter>::add_row_from_json(const Json::Value &entry)
{
  // getting timestamp from json argument, retrieving important fields from json
  const time_t timestamp      = std::stol(entry["_SOURCE_REALTIME_TIMESTAMP"].asString()) / 1000000;
  const std::string type      = format_log_data(entry["_AUDIT_FIELD_APPARMOR"].asString());
  const std::string pid       = entry["_PID"].asString();
  
  std::string operation;
  std::string name;
  std::string status;

  // Adapted from: https://gitlab.com/apparmor/apparmor/-/blob/master/libraries/libapparmor/include/aalogparse.h
  if(type == "STATUS") {
    name      = entry["_AUDIT_FIELD_NAME"].asString();
    operation = format_log_data(entry["_AUDIT_FIELD_OPERATION"].asString());
    status    = format_log_data(entry["_AUDIT_FIELD_PROFILE"].asString());
  }
  /* Denied access event */
  else if(type == "DENIED") {
    name      = format_log_data(entry["_AUDIT_FIELD_PROFILE"].asString());
    operation = format_log_data(entry["_AUDIT_FIELD_OPERATION"].asString());

    if(operation == "capable") {
      status = "capname: " + entry["_AUDIT_FIELD_CAPNAME"].asString() + " capability: " + entry["_AUDIT_FIELD_CAPABILITY"].asString();
    }
    else {
      status = "requested mask: " + format_log_data(entry["_AUDIT_FIELD_REQUESTED_MASK"].asString()) + " denied mask: " + format_log_data(entry["_AUDIT_FIELD_DENIED_MASK"].asString());
    }
  }
  // /* Default event type */
  // else if(type == "INVALID") {

  // }
  // /* Internal AA error */
  // else if(type == "ERROR") {

  // }
  // /* Audited event */
  // else if(type == "AUDIT") {

  // }
  // /* Complain mode event */
  // else if(type == "ALLOWED") {

  // }
  // /* Process tracking info */
  // else if(type == "HINT") {

  // }
  else {
    std::cerr << "Error - Unknown log type: " << type << std::endl;
    name      = entry["_AUDIT_FIELD_NAME"].asString();
    operation = format_log_data(entry["_AUDIT_FIELD_OPERATION"].asString());
    status    = format_log_data(entry["_AUDIT_FIELD_PROFILE"].asString());
  }
  
  adapter->put_data(timestamp,
                    type,
                    operation,
                    name,
                    stoul(pid),
                    status);
}

template<class LogsTab, class Database, class Adapter>
void
LogsController<LogsTab, Database, Adapter>::add_data_to_record(const std::string &data)
{
  auto json_data = std::make_shared<std::istringstream>(data);

  auto lambda = [&, json_data]() -> bool { return add_data_to_record_helper(json_data); };

  Glib::signal_idle().connect(lambda, Glib::PRIORITY_LOW);
}

template<class LogsTab, class Database, class Adapter>
bool
LogsController<LogsTab, Database, Adapter>::add_data_to_record_helper(std::shared_ptr<std::istringstream> json_data)
{
  // Declare some variables that will be written to
  Json::Value value;
  Json::CharReaderBuilder builder;
  JSONCPP_STRING errs;

  // gets each log entry (in json format, separated by \n), parses the json, and calls add_row_from_json to add each individual entry
  for (uint i = 0; i < 127; i++) {
    // Get the next line (if it exists)
    std::string line;
    if (!std::getline(*json_data, line)) {
      // We exhausted the input stream, which means no more logs exist
      // Return false to disconnect the signal handler
      refresh();
      return false;
    }

    // A log exists, so lets parse it using JsonCpp
    std::stringstream log_stream(line);

    if (!parseFromStream(builder, log_stream, &value, &errs)) {
      throw std::invalid_argument(errs + "\nArgument of add_data_to_record contains line with invalid JSON format.");
    }

    // Create a row from the json value
    add_row_from_json(value);
  }

  // Refresh the display to show an accurate count of the number of logs
  refresh();
  return true;
}

template<class LogsTab, class Database, class Adapter>
void
LogsController<LogsTab, Database, Adapter>::refresh()
{
  uint num_visible = adapter->get_col_record()->filter_rows();
  logs->set_status_label_text(" " + std::to_string(num_visible) + " logs");
}

// For unit testing
template<class LogsTab, class Database, class Adapter>
LogsController<LogsTab, Database, Adapter>::LogsController(std::shared_ptr<Adapter> adapter, std::shared_ptr<LogsTab> logs)
  : logs{ std::move(logs) },
    adapter{ std::move(adapter) }
{
}

// For production
template<class LogsTab, class Database, class Adapter>
LogsController<LogsTab, Database, Adapter>::LogsController(std::shared_ptr<Database> database)
  : logs{ StatusController<LogsTab>::get_tab() },
    adapter{ new Adapter(database, logs->get_view(), logs->get_window()) }
{
  auto func = sigc::mem_fun(*this, &LogsController<LogsTab, Database, Adapter>::refresh);
  logs->set_refresh_signal_handler(func);

  auto filter_fun = sigc::mem_fun(*this, &LogsController::filter);
  adapter->get_col_record()->set_visible_func(filter_fun);
}

// Used to avoid linker errors
// For more information, see: https://isocpp.org/wiki/faq/templates#class-templates
template class LogsController<Logs, Database, LogAdapter<Database, StatusColumnRecord>>;
