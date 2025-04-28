#include "MbsBackend.h"
#include "Util.cpp"

//################################################
// we will be using boost::beast for the webapi (no websockets used)

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
// database preparation and configuration

static int sqLiteCallback(void *NotUsed, int argc, char **argv, char **azColName) {
	for (int i = 0; i < argc; i++) {
		std::cout << azColName[i] << " = " << (argv[i] ? argv[i] : "NULL") << "\n";
	}
	std::cout << "\n";
	return(0);
}

bool sqLiteExecute(sqlite3 *db, const std::string &sql) {
	char *errMsg = nullptr;
	int rc = sqlite3_exec(db, sql.c_str(), sqLiteCallback, 0, &errMsg);
	if (rc != SQLITE_OK) {
		std::cerr << "SQL error: " << errMsg << "\n";
		sqlite3_free(errMsg);
		return(false);
	}
	return(true);
}

int sqLiteConnect() {
	sqlite3 *db;

	int rc = sqlite3_open("movie_ticket_system.db", &db);
	if (rc) {
		std::cerr << "Can't open database: " << sqlite3_errmsg(db) << "\n";
		return(1);
	} else {
		std::cout << "Opened database successfully\n";
	}
	
	/*
	 * Users:
	 * - id (increases per user)
	 * - name (can be whatever name they want.)
	 * - email (can only happen once in the list)
	 * - password (hashed. cannot be empty)
	 * - payment details, although allow paying in person.
	 */
	std::string templateUsersTable = R"(
		CREATE TABLE IF NOT EXISTS Users (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			name TEXT,
			email TEXT NOT NULL UNIQUE,
			password TEXT NOT NULL,
			payment_details TEXT
		);
	)";

	/*
	 * Movies:
	 * - id (must be an int) (increments for each movie)
	 * - name (text of the movie name)
	 * - showtime (unix timestamp possibly?)
	 * - price per ticket (deals, etc)
	 * - rating (g, pg, pg13, etc...)
	 */
	std::string templateMoviesTable = R"(
		CREATE TABLE IF NOT EXISTS Movies (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			name TEXT NOT NULL,
			showtime TEXT NOT NULL,
			price_per_ticket REAL NOT NULL,
			rating TEXT CHECK (rating IN ('G', 'PG', 'PG13', 'R', 'NC17')) NOT NULL
		);
	)";

	/*
	 * Tickets
	 * - id (of ticket)
	 * - userid (of person who bought it) (REFERENCES USER TABLE)
	 * - movieid (REFERENCES MOVIE TABLE)
	 * - quantity (of tickets bought)
	 * - purchase time (time of rows creation)
	 */
	std::string templateTicketsTable = R"(
		CREATE TABLE IF NOT EXISTS Tickets (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
			user_id INTEGER NOT NULL,
			movie_id INTEGER NOT NULL,
			quantity INTEGER NOT NULL DEFAULT 1,
			purchase_time TEXT NOT NULL DEFAULT (datetime('now')),
			FOREIGN KEY (user_id) REFERENCES Users(id),
			FOREIGN KEY (movie_id) REFERENCES Movies(id)
		);
	)";
	
	/*
	 * Admins:
	 * - id (of user that is admin) (REFERENCES USER TABLE)
	 */ 
	std::string templateAdminsTable = R"(
		CREATE TABLE IF NOT EXISTS Admins (
			user_id INTEGER PRIMARY KEY,
			FOREIGN KEY (user_id) REFERENCES Users(id)
		);
	)";

	if (!sqLiteExecute(db, templateUsersTable) ||
		!sqLiteExecute(db, templateMoviesTable) ||
		!sqLiteExecute(db, templateTicketsTable) ||
		!sqLiteExecute(db, templateAdminsTable)) {
		sqlite3_close(db);
		return(1);
	}

	std::cout << "Tables created successfully!\n";

	sqlite3_close(db);
	return(0);
}

//################################################
// database addition





//################################################
int main(int argc, char* argv[]) {
	


	/*try {
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
	*/
}
