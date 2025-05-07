#pragma once
#include "Util.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

std::map<std::string, std::string> parseQuery(const std::string& query);
void handleRequest(http::request<http::string_body> req, http::response<http::string_body>& res);
static int sqLiteCallback(void *NotUsed, int argc, char **argv, char **azColName);
bool sqLiteExecute(sqlite3 *db, const std::string &sql);
int sqLiteInitialize();
static int makeDefaultAdmin(sqlite3 *db);

std::string createAcc(std::map<std::string, std::string> params); // to be changed
std::string deleteAcc(std::map<std::string, std::string> params);
std::string buyTicket(std::map<std::string, std::string> params);
std::string getTicket(std::map<std::string, std::string> params);
std::string adminAdd(std::map<std::string, std::string> params);
std::string adminDel(std::map<std::string, std::string> params);
std::string addMovie(std::map<std::string, std::string> params);
std::string delMovie(std::map<std::string, std::string> params);
std::string accDetails(std::map<std::string, std::string> params);
std::string listMovies(std::map<std::string, std::string> params);
std::string verifyAcc(std::map<std::string, std::string> params);
std::string getMovie(std::map<std::string, std::string> params);
std::string adminVerify(std::map<std::string, std::string> params);
std::string reviewAdd(std::map<std::string, std::string> params);
std::string reviewList(std::map<std::string, std::string> params);
std::string addTheater(std::map<std::string, std::string> params);
std::string delTheater(std::map<std::string, std::string> params);
std::string getTheater(std::map<std::string, std::string> params);
std::string genReport(std::map<std::string, std::string> params);
std::string updatePayment(std::map<std::string, std::string> params);
