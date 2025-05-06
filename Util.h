#pragma once 
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>

#include <bits/stdc++.h>

#include <map>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <ctime>

#include <sqlite3.h>

#ifdef DEBUG 
#define DBG_PRINT(x) fprintf(stderr, "\x1b[33mDEBUG\x1b[0m: %s", x)
#else
#define DBG_PRINT(x)
#endif

