#include "Util.cpp"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

std::map<std::string, std::string> parseQuery(const std::string& query);
void handleRequest(http::request<http::string_body> req, http::response<http::string_body>& res);
static int sqLiteCallback(void *NotUsed, int argc, char **argv, char **azColName);
bool sqLiteExecute(sqlite3 *db, const std::string &sql);
int sqLiteInitialize();

std::string createAcc(); // to be changed
std::string deleteAcc();
std::string buyTicket();
std::string getTicket();
std::string adminAdd();
std::string adminDel();
std::string addMovie();
std::string delMovie();
std::string accDetails();
