#include "MbsBackend.h"
#include "Util.cpp"

//################################################
// we will be using boost::beast for the webapi (including websockets)

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


//################################################








//################################################

int main(int argc, char* argv[]) {
	if(argc < 2 || argc > 2) { // only one argument possible
		std::cout << "Please include only one argument.";
		return(1);
	}
}

