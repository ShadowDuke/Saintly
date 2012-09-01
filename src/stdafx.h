#pragma once

#define WIN32_LEAN_AND_MEAN

#include <boost/bind.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <my_global.h>
#include <windows.h>

#include <mysql_version.h>
#include <mysql_com.h>
#include <mysql_time.h>
#include <my_list.h>
#include <mysql.h>
#pragma comment(lib, "libmysql.lib")

#include <vector>
#include <queue>
#include <string>

#include "config.h"
