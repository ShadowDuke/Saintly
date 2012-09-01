/**
*	Saintly Hive
* ------------------------
**/

#include "stdafx.h"

using std::string;
using std::cout;
using std::queue;
using std::fstream;

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/configurator.h>
#include <iomanip>

using namespace log4cplus;

#define LOG4CPLUS_BUILD_DLL

MYSQL _mysqlR;
bool _connected  = false;
bool _working = false;
Config config;
queue <string> _stmtQueue;
boost::mutex _stmtQueueMutex;
boost::condition_variable _stmtQueueCond;
boost::thread worker;

void mysqlCUDThread();

BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

extern "C"
{
	__declspec(dllexport) void __stdcall RVExtension(char *output, int outputSize, const char *function);
};

void __stdcall RVExtension(char *output, int outputSize, const char *function)
{
	PropertyConfigurator logConfig("log4cplus.properties");
	logConfig.configure();
    Logger logger = Logger::getInstance("main");

	outputSize -= 1;
	string input(function);
	size_t cmdIndex = input.find_first_of(':');
	size_t instanceIndex = input.find_last_of(':');
	string command(input.begin(), input.begin() + cmdIndex);
	string instance(input.begin() + cmdIndex + 1, input.begin() + instanceIndex);	
	string query(input.begin() + instanceIndex + 1, input.end());
	LOG4CPLUS_INFO(logger, "" + instance + " command " + command + ": " + query);

	if (!_connected)
	{
		//Current directory is ArmA 2 server directory
		GetCurrentDirectoryA(250, config.currentDir);

		//Read config files from saintly.ini
		sprintf_s(config.currentDir, "%s\\saintly.ini", config.currentDir);
		GetPrivateProfileStringA(instance.c_str(), "username", "dayz", config.dbuser, 100, config.currentDir);
		GetPrivateProfileStringA(instance.c_str(), "password", "dayzpass", config.dbpass, 100, config.currentDir);
		GetPrivateProfileStringA(instance.c_str(), "database", "dayz", config.dbname, 100, config.currentDir);
		GetPrivateProfileStringA(instance.c_str(), "hostname", "127.0.0.1", config.dbhost, 100, config.currentDir);
		config.dbport = GetPrivateProfileIntA(instance.c_str(), "port", 3306, config.currentDir);

		if (mysql_init(&_mysqlR) == NULL)
		{
			LOG4CPLUS_INFO(logger, "mysql_init() error");
			return;
		}
		else
		{
			my_bool reconnect = 1;
			mysql_options(&_mysqlR, MYSQL_OPT_RECONNECT, &reconnect);
			if (!mysql_real_connect(&_mysqlR, config.dbhost, config.dbuser, config.dbpass, config.dbname, config.dbport, nullptr, CLIENT_MULTI_RESULTS))
			{
				LOG4CPLUS_ERROR(logger, "mysql_real_connect() error - " << mysql_errno(&_mysqlR) << " - " << mysql_error(&_mysqlR));
				return;
			}
			else
			{
				LOG4CPLUS_INFO(logger, "mysql connected");
				_connected = true;
			}
		}
	} else {
		int status = mysql_ping(&_mysqlR);
		if (status > 0) {
			LOG4CPLUS_ERROR(logger, "mysql_ping() error - " << mysql_errno(&_mysqlR) << " - " << mysql_error(&_mysqlR));
		}
	}

	if (_connected && !_working)
	{
		worker = boost::thread(boost::bind(&mysqlCUDThread));
		_working = true;
		LOG4CPLUS_DEBUG(logger, "worker thread created");
	}

	if (command == "E")
	{
		boost::unique_lock<boost::mutex> lock(_stmtQueueMutex);
		_stmtQueue.push(query);
		LOG4CPLUS_TRACE(logger, "waking up worker");
		_stmtQueueCond.notify_one();
	}
	else if (command == "Q")
	{
		MYSQL_ROW row;
		int status = mysql_real_query(&_mysqlR, query.c_str(), query.length());
		LOG4CPLUS_DEBUG(logger, "mysql_real_query()");
		do
		{
			MYSQL_RES *res = mysql_store_result(&_mysqlR);
			if (res)
			{
				string outStr("[");
				if (mysql_num_rows(res) > 0) {
					bool firstResult = true;
					while ((row = mysql_fetch_row(res)))
					{
						if (firstResult)
						{
							firstResult = false;
						}
						else
						{
							outStr.append(",");
						}
						outStr.append("[");
						for (uint i = 0; i < mysql_num_fields(res); i++)
						{
							if (i != 0)
							{
								outStr.append(",");
							}
							string data (row[i]);
							boost::replace_all(data, "\"", "\"\"");
							outStr.append("\"").append(data.c_str()).append("\"");
						}
						outStr.append("]");
					}
				} else {
					outStr.append("[]");
				}
				mysql_free_result(res);
				outStr.append("]");
				strncpy(output, outStr.c_str(), outputSize);
			}
			else
			{
				if (mysql_field_count(&_mysqlR) != 0)
				{
					LOG4CPLUS_ERROR(logger, "mysql_store_result() error - " << mysql_errno(&_mysqlR) << " - " << mysql_error(&_mysqlR));
				}
				else
				{
					break;
				}
			}

			status = mysql_next_result(&_mysqlR);
		} while (status == 0);
	}
};

void mysqlCUDThread()
{
	PropertyConfigurator logConfig("log4cplus.properties");
	logConfig.configure();
	Logger logger = Logger::getInstance("work");

	MYSQL _mysqlCUD;
	if (mysql_init(&_mysqlCUD) == NULL)
	{
		LOG4CPLUS_ERROR(logger, "mysql_init() error");
		return;
	}
	else
	{
		my_bool reconnect = 1;
		mysql_options(&_mysqlCUD, MYSQL_OPT_RECONNECT, &reconnect);
		if (!mysql_real_connect(&_mysqlCUD, config.dbhost, config.dbuser, config.dbpass, config.dbname, config.dbport, nullptr, CLIENT_MULTI_RESULTS))
		{
			LOG4CPLUS_ERROR(logger, "mysql_real_connect() error - " << mysql_errno(&_mysqlCUD) << " - " << mysql_error(&_mysqlCUD));
			return;
		}
		else
		{
			LOG4CPLUS_INFO(logger, "mysql connected");
		}
	}

	while (1)
	{
		//Wait until the main thread calls notify_one()
		boost::unique_lock<boost::mutex> lock(_stmtQueueMutex);
		while (_stmtQueue.empty())
		{
			_stmtQueueCond.wait(lock);
		}
		LOG4CPLUS_TRACE(logger, "woken up");

		//Empty the queue
		while (!_stmtQueue.empty()) {
			string issue = _stmtQueue.front();

			int status = mysql_ping(&_mysqlCUD);
			if (status > 0) {
				LOG4CPLUS_ERROR(logger, "mysql_ping() error - " << mysql_errno(&_mysqlCUD) << " - " << mysql_error(&_mysqlCUD));
			}

			mysql_real_query(&_mysqlCUD, issue.c_str(), issue.length());
			LOG4CPLUS_DEBUG(logger, "mysql_real_query()");

			//Discard any results
			MYSQL_RES *res = mysql_store_result(&_mysqlCUD);
			mysql_free_result(res);
			mysql_next_result(&_mysqlCUD);

			_stmtQueue.pop();
		}

		LOG4CPLUS_TRACE(logger, "sleeping");
	}
}
