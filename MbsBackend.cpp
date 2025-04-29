#include "Util.cpp"
#include "MbsBackend.h"

//################################################
// handling requests

std::map<std::string, std::string> parseQuery(const std::string& query) {
	std::map<std::string, std::string> params;
	std::stringstream ss(query);
	std::string item;

	while (std::getline(ss, item, '&')) {
		size_t pos = item.find('=');
		if (pos != std::string::npos) {
			std::string key = item.substr(0, pos);
			std::string value = item.substr(pos + 1);
			params[key] = value;
		}
	}

	return params;
}


void handleRequest(http::request<http::string_body> req, http::response<http::string_body>& res) {
	// response version
	res.version(req.version());
	// keep-alive settings
	res.keep_alive(req.keep_alive());

	std::string body;

	if (req.method() == http::verb::get) {
		std::string target(req.target().begin(), req.target().end());
		size_t pos = target.find('?');

		if (pos != std::string::npos) {
			std::string query = target.substr(pos + 1);
			auto params = parseQuery(query);

			if(params.find("action") != params.end()) {
				if(params["action"] == "createacc") {
					body = createAcc(params);
				} else if(params["action"] == "delacc") {
					body = deleteAcc(params);
				} else if(params["action"] == "buyticket") {
					body = buyTicket(params);
				} else if(params["action"] == "getticket") {
					body = getTicket(params);
				} else if(params["action"] == "adminadd") {
					body = adminAdd(params);
				} else if(params["action"] == "admindel") {
					body = adminDel(params);
				} else if(params["action"] == "addmovie") {
					body = addMovie(params);
				} else if(params["action"] == "delmovie") {
					body = delMovie(params);
				} else if(params["action"] == "accdetails") {
					body = accDetails(params);
				} else if(params["action"] == "listmovies") {
					body = listMovies(params);
				} else {
					body = "{\"message\": \"INVALID ACTION: " + params["action"] + "\"}";
				}
			} else {
				body = "{\"message\": \"err: NO ACTION\"}";
			}
		}
		std::string userAgent(req[http::field::user_agent].begin(), req[http::field::user_agent].end());
		if (userAgent.find("Mozilla") != std::string::npos ||
			userAgent.find("Chrome") != std::string::npos ||
			userAgent.find("Safari") != std::string::npos) {
			body = "{\"request\": \"1\", \"message\": \"Get the hell out of the browser loser.\"}";
		}
		res.result(http::status::ok);
		res.set(http::field::content_type, "application/json");
		res.body() = body;		
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

int sqLiteInitialize() {
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

/*
 * Requirements:
 * - Email
 * - Hashed Password
 * - Username
 *
 * Returns:
 * - Fail/Success
 * - Account ID (?)
 */
std::string createAcc(std::map<std::string, std::string> params) {
	sqlite3 *db;

	if (sqlite3_open("movie_ticket_system.db", &db)) { //try to open the database
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Unable to open database.\"}"; // if fails, returns error
	}

	// check required fields. look above, for example
	if (params.count("email") == 0 || params.count("password") == 0 || params.count("name") == 0) { // if ANY of these dont exist, returns error
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\',\'password\', and \'name\'\"}";
	}

	// had to learn sqlite for this.
	std::string sql = "INSERT INTO Users (name, email, password, payment_details) VALUES (?, ?, ?, ?);";
	sqlite3_stmt *stmt = nullptr; // generate empty pointer in case no payment details

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["name"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, params["email"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, params["password"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, params.count("payment_details") ? params["payment_details"].c_str() : nullptr, -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_DONE) {
		int user_id = sqlite3_last_insert_rowid(db);
		result = "{\"request\": \"0\", \"message\": \"Success!\", \"user_id\": \"" + std::to_string(user_id) + "\"}";
	} else {
		result = "{\"request\": \"1\", \"message\": \"Failed to create account.\"}";
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return result;
}


/*
 * Requirements:
 * - Email
 * - Hashed Password
 *
 * Returns:
 * - Fail/Success
 */
std::string deleteAcc(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	sqlite3_close(db);
	return("{\"request\": \"0\",\"message\":\"Success!\"}");
}

/*
 * Requirements:
 * - Email
 * - Hashed Password
 * - Payment info (?)
 * - Ticket amount
 * - Movie ID
 *
 * Returns:
 * - Fail/Success
 * - Ticket ID(s)
 */
std::string buyTicket(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	sqlite3_close(db);
	return("{\"request\": \"0\",\"message\":\"Success!\"}");
}

/*
 * Requirements:
 * - Email
 * - Hashed Password
 *
 * Returns:
 * - Fail/Success
 * - Ticket information
 */
std::string getTicket(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	sqlite3_close(db);
	return("{\"request\": \"0\",\"message\":\"Success!\"}");
}

/*
 * Requirements:
 * - Email (w. admin status)
 * - Target email
 * - Hashed Password
 *
 * Returns:
 * - Fail/Success
 */
std::string adminAdd(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	sqlite3_close(db);
	return("{\"request\": \"0\",\"message\":\"Success!\"}");
}

/*
 * Requirements:
 * - Email (w. admin status)
 * - Target email
 * - Hashed Password
 *
 * Returns:
 * - Fail/Success
 */
std::string adminDel(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	sqlite3_close(db);
	return("{\"request\": \"0\",\"message\":\"Success!\"}");
}

/*
 * Requirements:
 * - Email (w. admin status)
 * - Hashed Password
 * - Movie name
 * - Showtime
 * - Price
 * - Rating
 *
 * Returns:
 * - Fail/Success
 * - Movie ID
 */
std::string addMovie(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	sqlite3_close(db);
	return("{\"request\": \"0\",\"message\":\"Success!\"}");
}

/*
 * Requirements:
 * - Email (w. admin status)
 * - Hashed Password
 * - Movie ID
 *
 * Returns:
 * - Fail/Success
 */
std::string delMovie(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	sqlite3_close(db);
	return("{\"request\": \"0\",\"message\":\"Success!\"}");
}

/*
 * Requirements:
 * - Email (w. admin status)
 * - Hashed Password
 *
 * Returns:
 * - Fail/Success
 * - Account ID
 * - Payment information (?)
 * - Admin status
 */
std::string accDetails(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	sqlite3_close(db);
	return("{\"request\": \"0\",\"message\":\"Success!\"}");
}

/*
 * Returns:
 * - Movie name
 * - Movie ID
 * - Showtime
 * - Rating
 * - Price
 */
std::string listMovies(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	sqlite3_close(db);
	return("{\"request\": \"0\",\"message\":\"Success!\"}");
}

//################################################
int main(int argc, char* argv[]) {
	try {
		asio::io_context ioc;

		int sqlData = sqLiteInitialize();
		if(sqlData == 1) {
			return(1);
		}

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
