/**
*	Saintly Hive
* ------------------------
**/

#include "stdafx.h"

using std::string;
using std::cout;
using std::queue;
using std::fstream;

MYSQL _mysqlR;
bool _connected  = false;
bool _working = false;
Config config;
queue <string> _stmtQueue;
boost::mutex _stmtQueueMutex;
boost::condition_variable _stmtQueueCond;
boost::thread worker;
fstream logFile;

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
	if (!logFile.is_open())
	{
		logFile.open("logfile.log", std::ios_base::app);
	}

	outputSize -= 1;
	string input(function);
	size_t cmdIndex = input.find_first_of(',');
	string command(input.begin(), input.begin() + cmdIndex);
	string query(input.begin() + (cmdIndex + 1), input.end());
	logFile << "COMMAND: " << command << " PAYLOAD: \"" << query << "\"" << std::endl;

	if (!_connected)
	{
		//Current directory is ArmA 2 server directory
		GetCurrentDirectoryA(250, config.currentDir);

		//Read config files from bliss.ini
		sprintf(config.currentDir, "%s\\bliss.ini", config.currentDir);
		GetPrivateProfileStringA("database", "username", "dayz", config.dbuser, 100, config.currentDir);
		GetPrivateProfileStringA("database", "password", "dayzpass", config.dbpass, 100, config.currentDir);
		GetPrivateProfileStringA("database", "database", "dayz", config.dbname, 100, config.currentDir);
		GetPrivateProfileStringA("database", "hostname", "127.0.0.1", config.dbhost, 100, config.currentDir);
		config.dbport = GetPrivateProfileIntA("database", "port", 3306, config.currentDir);

		if (mysql_init(&_mysqlR) == NULL)
		{
			logFile << "MAIN: mysql_init() error" << std::endl;
			return;
		}
		else
		{
			if (!mysql_real_connect(&_mysqlR, config.dbhost, config.dbuser, config.dbpass, config.dbname, config.dbport, nullptr, CLIENT_MULTI_RESULTS))
			{
				logFile << "MAIN: mysql_real_connect() error - " << mysql_errno(&_mysqlR) + " - " << mysql_error(&_mysqlR) << std::endl;
				return;
			}
			else
			{
				logFile << "MAIN: mysql connected" << std::endl;
				_connected = true;
			}
		}
	}

	if (_connected && !_working)
	{
		worker = boost::thread(boost::bind(&mysqlCUDThread));
		_working = true;
		logFile << "MAIN: worker thread created" << std::endl;
	}

	if (command == "execute")
	{
		boost::unique_lock<boost::mutex> lock(_stmtQueueMutex);
		_stmtQueue.push(query);
		logFile << "MAIN: waking up worker" << std::endl;
		_stmtQueueCond.notify_one();
	}
	else if (command == "query")
	{
		MYSQL_ROW row;
		int status = mysql_real_query(&_mysqlR, query.c_str(), query.length());
		logFile << "MAIN: mysql_real_query()" << std::endl;
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
					logFile << "MAIN: mysql_store_result() error - " << mysql_errno(&_mysqlR) + " - " << mysql_error(&_mysqlR) << std::endl;
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
	MYSQL _mysqlCUD;
	if (mysql_init(&_mysqlCUD) == NULL)
	{
		logFile << "WORKER: mysql_init() error" << std::endl;
		return;
	}
	else
	{
		if (!mysql_real_connect(&_mysqlCUD, config.dbhost, config.dbuser, config.dbpass, config.dbname, config.dbport, nullptr, CLIENT_MULTI_RESULTS))
		{
			logFile << "WORKER: mysql_real_connect() error - " << mysql_errno(&_mysqlCUD) + " - " << mysql_error(&_mysqlCUD) << std::endl;
			return;
		}
		else
		{
			logFile << "WORKER: mysql connected" << std::endl;
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
		logFile << "WORKER: woken up" << std::endl;

		//Empty the queue
		while (!_stmtQueue.empty()) {
			string issue = _stmtQueue.front();

			mysql_real_query(&_mysqlCUD, issue.c_str(), issue.length());
			logFile << "WORKER: mysql_real_query()" << std::endl;

			//Discard any results
			MYSQL_RES *res = mysql_store_result(&_mysqlCUD);
			mysql_free_result(res);
			mysql_next_result(&_mysqlCUD);

			_stmtQueue.pop();
		}

		logFile << "WORKER: sleeping" << std::endl;
	}
}