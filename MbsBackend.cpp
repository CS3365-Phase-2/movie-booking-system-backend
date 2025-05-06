#include "Util.cpp"
#include "MbsBackend.h"

//################################################
// handling requests

std::unordered_map<std::string,std::function<std::string(std::map<std::string, std::string> params)>> requestTable = {
	{"createacc",	createAcc},
	{"delacc",	deleteAcc},
	{"buyticket",	buyTicket},
	{"getticket",	getTicket},
	{"adminadd",	adminAdd},
	{"admindel",	adminDel},
	{"addmovie",	addMovie},
	{"delmovie",	delMovie},
	{"accdetails",	accDetails},
	{"listmovies",	listMovies},
	{"verifyacc",	verifyAcc},
	{"getmovie",	getMovie},
	{"checkadmin",	adminVerify}
};

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
				try {
					body = 	requestTable[params["action"]](params);
				} catch (...) {
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

	std::cout << makeDefaultAdmin(db);
	std::cout << "Tables created successfully!\n";

	sqlite3_close(db);
	return(0);
}

static int makeDefaultAdmin(sqlite3 *db) {
	sqlite3_stmt *stmt = nullptr;
	std::string adminuser = "admin";

	std::string selectAdminSQL = R"(
		SELECT id FROM Users WHERE email = ?;
	)";
	if (sqlite3_prepare_v2(db, selectAdminSQL.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		std::cerr << "Error preparing select: " << sqlite3_errmsg(db) << "\n";
		return 1;
	}
	sqlite3_bind_text(stmt, 1, adminuser.c_str(), -1, SQLITE_TRANSIENT);
	bool adminExists = (sqlite3_step(stmt) == SQLITE_ROW);
	sqlite3_finalize(stmt);

	if (!adminExists) {
		std::string insertUserSQL = R"(
		INSERT INTO Users (name, email, password, payment_details)
		VALUES (?, ?, ?, ?);
	)";
	if (sqlite3_prepare_v2(db, insertUserSQL.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		std::cerr << "Error preparing insert: " << sqlite3_errmsg(db) << "\n";
		return 1;
	}
	sqlite3_bind_text(stmt, 1, adminuser.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, adminuser.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, adminuser.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, adminuser.c_str(), -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) != SQLITE_DONE) {
			std::cerr << "Error inserting admin user: " << sqlite3_errmsg(db) << "\n";
			sqlite3_finalize(stmt);
			return 1;
		}
		sqlite3_finalize(stmt);
	}

	std::string insertAdminSQL = R"(
		INSERT OR IGNORE INTO Admins (user_id)
		VALUES ((SELECT id FROM Users WHERE email = ?));
	)";
	if (sqlite3_prepare_v2(db, insertAdminSQL.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		std::cerr << "Error preparing admin insert: " << sqlite3_errmsg(db) << "\n";
		return 1;
	}
	sqlite3_bind_text(stmt, 1, adminuser.c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		std::cerr << "Error inserting admin record: " << sqlite3_errmsg(db) << "\n";
		sqlite3_finalize(stmt);
		return 1;
	}
	sqlite3_finalize(stmt);

	return 0;
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
// This should work but should be tested again 
// Also expand to have optional payment_details
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
// TODO: FINISH THIS
std::string deleteAcc(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	// check required fields. look above, for example
	if (params.count("email") == 0 || params.count("password") == 0) { // if ANY of these dont exist, returns error
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\' and \'password\'\"}";
	}

	// had to learn sqlite for this.
	std::string sql = "DELETE FROM Users WHERE ? = email AND ? = password";
	sqlite3_stmt *stmt = nullptr; // generate empty pointer in case no payment details

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_DONE) {
		//int user_id = sqlite3_last_insert_rowid(db); // This prolly needs to be changed
        
		result = "{\"request\": \"0\", \"message\": \"Success!\" }";
	} else {
		result = "{\"request\": \"1\", \"message\": \"Failed to delete account.\"}";
	}

	sqlite3_finalize(stmt);

	sqlite3_close(db);
	return result;
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
// TODO: FINISH THIS
std::string buyTicket(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);
	// check required fields. look above, for example
	if (params.count("email") == 0 || params.count("password") == 0 || params.count("payment_details") == 0 || params.count("ticket_amount") == 0 || params.count("movie_id") == 0) { // if ANY of these dont exist, returns error
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\', \'password\', \'payment_details\', \'ticket_amount\', \'movie_id\'\"}";
	}

    // Please nested query... Please just work
	std::string sql = "INSERT INTO Tickets(user_id, movie_id, quantity, purchase_time) VALUES ( (SELECT id FROM Users WHERE ? = email AND ? = password),?,?,?) ";
	sqlite3_stmt *stmt = nullptr; // generate empty pointer in case no payment details

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, params["ticket_amount"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, std::to_string(time(NULL)).c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_DONE) {
		int ticket_id = sqlite3_last_insert_rowid(db); // This prolly needs to be changed
        
		result = "{\"request\": \"0\", \"message\": \"Success!\", \"ticket_id\": \"" + std::to_string(ticket_id) + "\"}";
	} else {
		result = "{\"request\": \"1\", \"message\": \"Failed to purchase ticket.\"}";
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
 * - Ticket information
 */
// TODO: Needs to be expanded to take either movie_id, name+showtime, or noparams
//  movie_id shows tickets only for "movie_id"
//  name+showtime does the same as above
//  noparams shows all tickets belonging to the user
std::string getTicket(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	// check required fields. look above, for example
	if (params.count("email") == 0 || params.count("password") == 0) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\' and \'password\'\"}";
	}

    // If wanting a specific movie send w/ a movie_id, otherwise just send all tickets attached to a user
    std::string sql = (params.count("movie_id")) ? 
        "SELECT * FROM Tickets WHERE user_id = (SELECT id FROM Users WHERE password = ? and email = ?) AND movie_id = ?" :
        "SELECT * FROM Tickets WHERE user_id = (SELECT id FROM Users WHERE password = ? and email = ?)" ;


    // Please nested query... Please just work
	//std::string sql = "SELECT * FROM Tickets WHERE user_id = (SELECT id from Users WHERE password = ? AND email = ?)";
	sqlite3_stmt *stmt = nullptr; // generate empty pointer in case no payment details

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
	//sqlite3_bind_text(stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
    rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
        std::string entries = "";

        do {
            const char* column = (const char*)sqlite3_column_text(stmt,0);
            entries += "\t" +  std::string(column);
        } while((rc = sqlite3_step(stmt)) == SQLITE_ROW);

		result = "{\"request\": \"0\", \"message\": \"Success!\", \"tickets\": \"" + entries + "\"}";
	} else {
		result = "{\"request\": \"1\", \"message\": \"Failed to find tickets.\"}";
	}

	sqlite3_finalize(stmt);

	sqlite3_close(db);
    return result;
    
}

/*
 * Requirements:
 * - Email (w. admin status)
 * - Hashed Password
 * - Target email
 *
 * Returns:
 * - Fail/Success
 */
// TODO: TEST THIS
std::string adminAdd(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);
    std::string result;

    if(!params.count("email") || !params.count("target") || !params.count("password")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\' and \'password\' \'target\'\"}";
    }

    // Check if the user is admin
    std::string admin_query = "SELECT CASE WHEN user_id = id THEN 1 WHEN user_id <> id THEN 0 END AS is_admin FROM Admins FULL JOIN Users WHERE email = ? AND password = ?";

	sqlite3_stmt *admin_stmt = nullptr; // generate empty pointer in case no payment details

    if(sqlite3_prepare_v2(db, admin_query.c_str(), -1, &admin_stmt, nullptr)) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
    }

    sqlite3_bind_text(admin_stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(admin_stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(admin_stmt) == SQLITE_ROW) {
        if(sqlite3_column_text(admin_stmt, 0)[0] == '0') 
		    return "{\"request\": \"1\", \"message\": \"Permission denied.\"}";
    }else
		return "{\"request\": \"1\", \"message\": \"Failed to check if user is admin.\"}";

    // Insert target
    std::string sql = "INSERT INTO Admins (user_id) VALUES ((SELECT id FROM Users WHERE email = ?))";
    sqlite3_stmt *stmt = nullptr;
    
    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr)) {
        sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
    }

    if(sqlite3_step(stmt) == SQLITE_DONE) {
        int admin_id = sqlite3_last_insert_rowid(db);
		result = "{\"request\": \"0\", \"message\": \"Success!\", \"admin_id\": \"" + std::to_string(admin_id) + "\"}";
    } else
		result = "{\"request\": \"1\", \"message\": \"Failed to delete account.\"}";

    sqlite3_finalize(stmt);
    sqlite3_finalize(admin_stmt);

	sqlite3_close(db);
	return result;
}

/*
 * Requirements:
 * - Email (w. admin status)
 * - Hashed Password
 * - Target email (to be removed)
 *
 * Returns:
 * - Fail/Success
 */
// TODO: TEST THIS
std::string adminDel(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);
    std::string result;

    if(!params.count("email") || !params.count("target") || !params.count("password")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\' and \'password\' \'target\'\"}";
    }

    // Check if the user is admin
    std::string admin_query = "SELECT CASE WHEN user_id = id THEN 1 WHEN user_id <> id THEN 0 END AS is_admin FROM Admins FULL JOIN Users WHERE email = ? AND password = ?";

	sqlite3_stmt *admin_stmt = nullptr; // generate empty pointer in case no payment details

    if(sqlite3_prepare_v2(db, admin_query.c_str(), -1, &admin_stmt, nullptr)) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
    }

    sqlite3_bind_text(admin_stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(admin_stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(admin_stmt) == SQLITE_ROW) {
        if(sqlite3_column_text(admin_stmt, 0)[0] == '0') 
		    return "{\"request\": \"1\", \"message\": \"Permission denied.\"}";
    }else
		return "{\"request\": \"1\", \"message\": \"Failed to check if user is admin.\"}";

    // Remove target
    std::string sql = "DELETE FROM Admins WHERE user_id = (SELECT id FROM Users WHERE email = ?)";
    sqlite3_stmt *stmt = nullptr;
    
    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr)) {
        sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
    }

    if(sqlite3_step(stmt) == SQLITE_DONE) 
		result = "{\"request\": \"1\", \"message\": \"Successfully deleted account.\"}";
    else
		result = "{\"request\": \"1\", \"message\": \"Failed to delete account.\"}";

    sqlite3_finalize(stmt);
    sqlite3_finalize(admin_stmt);
	sqlite3_close(db);

	return result;
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

	// check required fields. look above, for example
	if (!params.count("email") || !params.count("password") || !params.count("movie_name") || !params.count("showtime") || !params.count("price") || !params.count("rating")) { // if ANY of these dont exist, returns error
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\' and \'password\'\"}";
	}


    // Check if the user is an admin
    std::string admin_query = "SELECT CASE WHEN user_id = id THEN 1 WHEN user_id <> id THEN 0 END AS is_admin FROM Admins FULL JOIN Users WHERE email = ? AND password = ?";

	sqlite3_stmt *admin_stmt = nullptr; // generate empty pointer in case no payment details

    if(sqlite3_prepare_v2(db, admin_query.c_str(), -1, &admin_stmt, nullptr)) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
    }

    sqlite3_bind_text(admin_stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(admin_stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(admin_stmt) == SQLITE_ROW) {
        if(sqlite3_column_text(admin_stmt, 0)[0] == '0') 
		    return "{\"request\": \"1\", \"message\": \"Permission denied.\"}";
    }else
		return "{\"request\": \"1\", \"message\": \"Failed to check if user is admin.\"}";


	std::string sql = "INSERT INTO Movies(name,showtime,price_per_ticket,rating) VALUES (?,?,?,?)";
	sqlite3_stmt *stmt = nullptr; // generate empty pointer in case no payment details
	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["movie_name"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, params["showtime"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, params["price"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, params["rating"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_DONE) {
		int movie_id = sqlite3_last_insert_rowid(db); // This prolly needs to be changed
		result = "{\"request\": \"0\", \"message\": \"Success!\", \"movie_id\": \"" + std::to_string(movie_id) + "\"}";
	} else result = "{\"request\": \"1\", \"message\": \"Failed to add movie.\"}";

	sqlite3_finalize(stmt);
	sqlite3_finalize(admin_stmt);

	sqlite3_close(db);
	return result;
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

	if (rc) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Unable to open database.\"}";
	}

	if (!params.count("email") || !params.count("password") || !params.count("movie_id")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs 'email', 'password', and 'movie_id'\"}";
	}

	std::string admin_query = "SELECT CASE WHEN user_id = id THEN 1 WHEN user_id <> id THEN 0 END AS is_admin FROM Admins FULL JOIN Users WHERE email = ? AND password = ?";

	sqlite3_stmt *admin_stmt = nullptr;

	if (sqlite3_prepare_v2(db, admin_query.c_str(), -1, &admin_stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(admin_stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(admin_stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(admin_stmt) == SQLITE_ROW) {
		if (sqlite3_column_text(admin_stmt, 0)[0] == '0') {
			sqlite3_finalize(admin_stmt);
			sqlite3_close(db);
			return "{\"request\": \"1\", \"message\": \"Permission denied.\"}";
		}
	} else {
		sqlite3_finalize(admin_stmt);
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to check if user is admin.\"}";
	}

	std::string delete_sql = "DELETE FROM Movies WHERE id = ?;";
	sqlite3_stmt *stmt = nullptr;

	if (sqlite3_prepare_v2(db, delete_sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_finalize(admin_stmt);
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare delete statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["movie_id"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_DONE) {
		result = "{\"request\": \"0\", \"message\": \"Movie deleted successfully.\"}";
	} else {
		result = "{\"request\": \"1\", \"message\": \"Failed to delete movie.\"}";
	}

	sqlite3_finalize(stmt);
	sqlite3_finalize(admin_stmt);
	sqlite3_close(db);
	return result;
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
    std::string sql, result;

	// check required fields. look above, for example
	//if (params.count("movie_id") == 0) listall = 1;
    if (!params.count("email") || !params.count("password")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\' and \'password\'\"}";
    }

	sqlite3_stmt *stmt = nullptr; // generate empty pointer in case no payment details

    sql = "SELECT U.*, CASE WHEN U.id = A.user_id THEN 1 WHEN U.id <> A.user_id THEN 0 END AS is_admin FROM Users U FULL JOIN Admins A WHERE email = ? AND password = ?";

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

    sqlite3_bind_text(stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
        std::string entries = "";

        do {
            std::string row = "";
            for(char i = 0; i < 6; i++)
                row += "\t" + std::string((const char*)sqlite3_column_text(stmt,i));
            entries += "\n" + row;
        } while((rc = sqlite3_step(stmt)) == SQLITE_ROW);

		result = "{\"request\": \"0\", \"message\": \"Success!\", \"user\": \"" + entries + "\"}";
	} else result = "{\"request\": \"1\", \"message\": \"Failed to get account info.\"}";

	sqlite3_finalize(stmt);

	sqlite3_close(db);
	return result;
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
	std::string sql,result;
	int rc = sqlite3_open("movie_ticket_system.db", &db);
    char list_mode = 0x0; // Bitflag for options

    sql = "SELECT * FROM Movies";

    if (params.count("movie_name")) {
        sql += " WHERE name = ?";
        list_mode |= 0x1;
    }
    if (params.count("showtime")) { 
        sql += (list_mode) 
            ? " AND showtime = ?"
            : " WHERE showtime = ?";
        list_mode |= 0x2;
    } 

	sqlite3_stmt *stmt = nullptr;

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

    int field = 1;

    if(list_mode & 0x1) 
        sqlite3_bind_text(stmt, field++, params["movie_name"].c_str(), -1, SQLITE_TRANSIENT);
    if(list_mode & 0x2) 
        sqlite3_bind_text(stmt, field++, params["showtime"].c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
        std::string entries = "";

        do {
            std::string row = "";
            for(char i = 0; i < 5; i++)
                row += "\t" + std::string((const char*)sqlite3_column_text(stmt,i));
            entries += "\n" + row;
        } while((rc = sqlite3_step(stmt)) == SQLITE_ROW);

		result = "{\"request\": \"0\", \"message\": \"Success!\", \"movies\": \"" + entries + "\"}";
	}

	sqlite3_finalize(stmt);


	sqlite3_close(db);
	return result;
}

/*
 * Returns:
 * - User ID
 */
std::string verifyAcc(std::map<std::string, std::string> params) {
	sqlite3 *db;
	if (sqlite3_open("movie_ticket_system.db", &db)) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Unable to open database.\"}";
	}
	if (params.count("email") == 0 || params.count("password") == 0) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs 'email' and 'password'\"}";
	}

	std::string sql = "SELECT id FROM Users WHERE email = ? AND password = ?;";
	sqlite3_stmt *stmt = nullptr;

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		int user_id = sqlite3_column_int(stmt, 0);
		result = "{\"request\": \"0\", \"message\": \"Login successful.\", \"user_id\": \"" + std::to_string(user_id) + "\"}";
	} else {
		result = "{\"request\": \"1\", \"message\": \"Invalid email or password.\"}";
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return result;
}

/*
 * Returns:
 * - id
 * - name
 * - showtime
 * - price
 * - rating
 */
std::string getMovie(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);

	if (rc) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Unable to open database.\"}";
	}

	if (!params.count("movie_id")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing field: Needs 'movie_id'\"}";
	}

	std::string sql = "SELECT id, name, showtime, price_per_ticket, rating FROM Movies WHERE id = ?;";
	sqlite3_stmt *stmt = nullptr;

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["movie_id"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		int id = sqlite3_column_int(stmt, 0);
		const char *name = (const char*)sqlite3_column_text(stmt, 1);
		const char *showtime = (const char*)sqlite3_column_text(stmt, 2);
		double price_per_ticket = sqlite3_column_double(stmt, 3);
		const char *rating = (const char*)sqlite3_column_text(stmt, 4);

		result = "{\"request\": \"0\", \"message\": \"Success!\", \"movie\": {\"id\": \"" + std::to_string(id) + "\", \"name\": \"" + name + "\", \"showtime\": \"" + showtime + "\", \"price_per_ticket\": " + std::to_string(price_per_ticket) + ", \"rating\": \"" + rating + "\"}}";
	} else {
		result = "{\"request\": \"1\", \"message\": \"Movie not found.\"}";
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return result;
}

/*
 * Returns:
 * - 0 or 1 based on the user
 * 
 * Input:
 * - Email and Password
 */
std::string adminVerify(std::map<std::string, std::string> params) {
	sqlite3 *db;
	int rc = sqlite3_open("movie_ticket_system.db", &db);
	if (rc) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Unable to open database.\"}";
	}

	if (!params.count("email") || !params.count("password")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs 'email' and 'password'\"}";
	}

	std::string admin_query = "SELECT CASE WHEN user_id = id THEN 1 WHEN user_id <> id THEN 0 END AS is_admin FROM Admins FULL JOIN Users WHERE email = ? AND password = ?";
	sqlite3_stmt *stmt = nullptr;

	if (sqlite3_prepare_v2(db, admin_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		if (sqlite3_column_text(stmt, 0)[0] == '1') {
			result = "{\"request\": \"0\", \"message\": \"User is admin.\"}";
		} else {
			result = "{\"request\": \"1\", \"message\": \"User is not an admin.\"}";
		}
	} else {
		result = "{\"request\": \"1\", \"message\": \"User not found or invalid credentials.\"}";
	}

	sqlite3_finalize(stmt);
	sqlite3_close(db);
	return result;
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
