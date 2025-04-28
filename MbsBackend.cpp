#include "MbsBackend.h"
#include "Util.cpp"

//################################################
// we will be using boost::beast for the webapi (including websockets)

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

//################################################
// handling requests

void handleRequest(http::request<http::string_body> req, http::response<http::string_body>& res) {
	res.version(req.version());
	res.keep_alive(req.keep_alive());

	if (req.method() == http::verb::get && req.target() == "/") {
		res.result(http::status::ok);
		res.set(http::field::content_type, "text/plain");
		res.body() = "Welcome to MbsBackend! If you are seeing this, you shouldn't be here. Go back to the app! Read the documentation for more information.";
	} else {
		res.result(http::status::not_found);
		res.set(http::field::content_type, "text/plain");
		res.body() = "Not found";
	}

	res.prepare_payload();
}

//################################################

int main(int argc, char* argv[]) {
	//if(argc < 2 || argc > 2) { // only one argument possible
	//	std::cout << "Please include only one argument.";
	//	return(1);
	//}
	
	try {
		asio::io_context ioc;

		tcp::acceptor acceptor(ioc, {tcp::v4(), 4444});
		std::cout << "Server listening on http://localhost:4444/\n";
		
		for (;;) {
			tcp::socket socket(ioc);
			acceptor.accept(socket);

			beast::flat_buffer buffer;
			http::request<http::string_body> req;
			http::read(socket, buffer, req);

			http::response<http::string_body> res;
			handleRequest(req, res);

			http::write(socket, res);
		}
	} catch (const std::exception& exception) {
		std::cerr << "Error: " << exception.what() << "\n";
		return(1);
	}
	return(0);
}
