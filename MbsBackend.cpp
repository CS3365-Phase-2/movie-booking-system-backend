#include "MbsBackend.h"
#include "Util.h"
#include <argp.h>
#include <sqlite3.h>
#include <string>

// Just to make things easier to change in the future
#define DATABASE_FILE "movie_ticket_system.db"
#define PORT 4444

//################################################
// handling requests

std::unordered_map<std::string,std::function<std::string(std::map<std::string, std::string> params)>> requestTable = {
	{"createacc",	createAcc},
	{"delacc",	    deleteAcc},
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
	{"checkadmin",	adminVerify},
	{"reviewadd",	reviewAdd},
	{"reviewlist",  reviewList}
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
	int rc = sqlite3_open(DATABASE_FILE, &db);
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
			rating TEXT CHECK (rating IN ('G', 'PG', 'PG13', 'R', 'NC17')) NOT NULL,
            theater_id INTEGER NOT NULL,
            FOREIGN KEY (theater_id) REFERENCES Theaters(id)
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

	/*
	 * Reviews
	 * - userid (of person who left the review) (REFERENCES USER TABLE)
	 * - movieid (the movie the review is for) (REFERENCES MOVIE TABLE)
	 * - review (the users review)
	 */
	std::string templateReviewTable = R"(
		CREATE TABLE IF NOT EXISTS Reviews (
			user_id INTEGER NOT NULL,
			movie_id INTEGER NOT NULL,
			review TEXT NOT NULL,
            PRIMARY KEY (user_id, movie_id),
			FOREIGN KEY (user_id) REFERENCES Users(id),
			FOREIGN KEY (movie_id) REFERENCES Movies(id)
		);
	)";

	/*
	 * Theater:
	 * - id (id of the theater)
     * - name (the name of the theater)
	 */ 
	std::string templateTheaterTable = R"(
		CREATE TABLE IF NOT EXISTS Theaters (
			id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL
		);
	)";

	if (!sqLiteExecute(db, templateUsersTable) ||
		!sqLiteExecute(db, templateMoviesTable) ||
		!sqLiteExecute(db, templateTicketsTable) ||
		!sqLiteExecute(db, templateReviewTable) ||
		!sqLiteExecute(db, templateTheaterTable) ||
		!sqLiteExecute(db, templateAdminsTable)) {
		sqlite3_close(db);
		return(1);
	}

	std::cout << makeDefaultAdmin(db) << "\n";
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
 * - Password
 * - db (MUST BE OPENED BEFOREHAND!)
 *
 * Returns:
 * - bool
 */
// Small internal function to cut down on rewriting things
static inline bool isAdmin(std::string &email, std::string &password, sqlite3* db) {
    if(!db) return false; // Check if the db exists
    bool result = false;
	sqlite3_stmt *admin_stmt = nullptr;

    std::string admin_query = "SELECT CASE WHEN user_id = id THEN 1 WHEN user_id <> id THEN 0 END AS is_admin FROM Admins FULL JOIN Users WHERE email = ? AND password = ?";

    if(sqlite3_prepare_v2(db, admin_query.c_str(), -1, &admin_stmt, nullptr)) {
		sqlite3_close(db);
        sqlite3_finalize(admin_stmt);
		return false;
    }

    sqlite3_bind_text(admin_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(admin_stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(admin_stmt) == SQLITE_ROW)
        if(sqlite3_column_text(admin_stmt, 0)[0] == '1') result = true;
    sqlite3_finalize(admin_stmt);
    return result;
}

/*
 * Requirements:
 * - Movie Id
 * - db (MUST BE OPENED BEFOREHAND!)
 *
 * Returns:
 * - bool
 */
// Small internal function to cut down on rewriting things
static inline bool verifyMovie(std::string& id, sqlite3* db) {
    if(!db) return false; // Check if the db exists
    bool result = false;
	sqlite3_stmt *stmt = nullptr;

    std::string query = "SELECT id FROM Movies WHERE id = ?";

    if(sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr)) {
		sqlite3_close(db);
        sqlite3_finalize(stmt);
		return false;
    }

    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(stmt) == SQLITE_ROW) result = true;
    sqlite3_finalize(stmt);
    return result;
}

/*
 * Requirements:
 * - Email
 * - Password
 * - db (MUST BE OPENED BEFOREHAND!)
 *
 * Returns:
 * - bool
 */
// Small internal function to cut down on rewriting things
static inline bool verifyPayment(std::string& email, std::string& password, sqlite3* db) {
    if(!db) return false; // Check if the db exists
    bool result = false;
	sqlite3_stmt *stmt = nullptr;

    std::string query = "SELECT payment_details FROM Users WHERE email = ? AND password = ?";

    if(sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr)) {
		sqlite3_close(db);
        sqlite3_finalize(stmt);
		return false;
    }

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

    // If a row is returned then we have a valid result
    if(sqlite3_step(stmt) == SQLITE_ROW) 
        if(sqlite3_column_text(stmt, 0))
            result = true;

    sqlite3_finalize(stmt);
    return result;
}


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
    DBG_PRINT("User called createacc\n");
	sqlite3 *db;

	if (sqlite3_open(DATABASE_FILE, &db)) { //try to open the database
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Unable to open database.\"}"; // if fails, returns error
	}

	// check required fields. look above, for example
	if (!params.count("email") || !params.count("password") || !params.count("name")) { // if ANY of these dont exist, returns error
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
    DBG_PRINT("User called deleteacc\n");
	sqlite3 *db;
	int rc = sqlite3_open(DATABASE_FILE, &db);

	// check required fields. look above, for example
	if (!params.count("email") || !params.count("password")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\' and \'password\'\"}";
	}

	std::string sql = "DELETE FROM Users WHERE ? = email AND ? = password";
	sqlite3_stmt *stmt = nullptr;

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_DONE) {
		result = "{\"request\": \"0\", \"message\": \"Successfully deleted account!\" }";
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
 * - Ticket amount
 * - Movie ID
 *
 * Returns:
 * - Fail/Success
 * - Ticket ID(s)
 */
std::string buyTicket(std::map<std::string, std::string> params) {
    DBG_PRINT("User called buyticket\n");
	sqlite3 *db;
	int rc = sqlite3_open(DATABASE_FILE, &db);
	// check required fields. look above, for example
	if (!params.count("email")|| !params.count("password") || !params.count("ticket_amount") || !params.count("movie_id")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\', \'password\', \'ticket_amount\', \'movie_id\'\"}";
	}

    // Check if the movie is real
    if(!verifyMovie(params["movie_id"], db)) 
		return "{\"request\": \"1\", \"message\": \"Invalid movie id\"}";

    /*
    // Check if the user has a payment method registered
	std::string payment_query = "SELECT payment_details FROM Users WHERE email = ? AND password = ?";
	sqlite3_stmt *payment_stmt = nullptr;

	if (sqlite3_prepare_v2(db, payment_query.c_str(), -1, &payment_stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(payment_stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(payment_stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(payment_stmt) == SQLITE_ROW)
        if(!sqlite3_column_text(payment_stmt, 0))  // If we got an empty column...
            return "{\"request\": \"1\", \"message\": \"User does not have a registered payment method.\"}";
    */

    if(!verifyPayment(params["email"], params["password"], db))
        return "{\"request\": \"1\", \"message\": \"User does not have a registered payment method.\"}";

    // Please nested query... Please just work
	std::string sql = "INSERT INTO Tickets(user_id, movie_id, quantity, purchase_time) VALUES ( (SELECT id FROM Users WHERE ? = email AND ? = password),?,?,?) ";
	sqlite3_stmt *stmt = nullptr;

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, params["movie_id"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, params["ticket_amount"].c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, std::to_string(time(NULL)).c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if ((rc = sqlite3_step(stmt)) == SQLITE_DONE) {
		int ticket_id = sqlite3_last_insert_rowid(db);
		result = "{\"request\": \"0\", \"message\": \"Success!\", \"ticket_id\": \"" + std::to_string(ticket_id) + "\"}";
	} else {
		result = "{\"request\": \"1\", \"message\": \"Failed to purchase ticket.\"}";
	}

	sqlite3_finalize(stmt);
    //sqlite3_finalize(payment_stmt);
	sqlite3_close(db);

	return result;
} 

/*
 * Requirements:
 * - Email
 * - Hashed Password
 * - Movie Id (optional)
 *
 * Returns:
 * - Fail/Success
 * - Ticket information
 */
std::string getTicket(std::map<std::string, std::string> params) {
    DBG_PRINT("User called getticket\n");
    sqlite3 *db;
    int rc = sqlite3_open(DATABASE_FILE, &db);

    // check required fields. look above, for example
    if (!params.count("email") || !params.count("password")) {
        sqlite3_close(db);
        return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\' and \'password\'\"}";
    }

    // If wanting a specific movie send w/ a movie_id, otherwise just send all tickets attached to a user
    std::string sql = (params.count("movie_id")) ? 
        "SELECT * FROM Tickets WHERE user_id = (SELECT id FROM Users WHERE password = ? and email = ?) AND movie_id = ?" 
        :
        "SELECT * FROM Tickets WHERE user_id = (SELECT id FROM Users WHERE password = ? and email = ?)" ;

    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
    }

    sqlite3_bind_text(stmt, 1, params["password"].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, params["email"].c_str(), -1, SQLITE_TRANSIENT);
    if(params.count("movie_id")) 
        sqlite3_bind_text(stmt, 3, params["movie_id"].c_str(), -1, SQLITE_TRANSIENT);

    std::string result;
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        std::string entries = "{\n";
        int j = 0;
        do {
            std::string row = " \"" + std::to_string(j++) + "\": \"";
            for(int i = 0; i < 5; i++)
                row += std::string((const char*)sqlite3_column_text(stmt,i)) + " ";
            row += "\"";
            entries += row + ",\n";
        } while((rc = sqlite3_step(stmt)) == SQLITE_ROW);
        entries += "}";

        result = "{\"request\": \"0\", \"message\": \"Success!\", \"tickets\": " + entries + "}";
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
std::string adminAdd(std::map<std::string, std::string> params) {
    DBG_PRINT("User called adminadd\n");
	sqlite3 *db;
	int rc = sqlite3_open(DATABASE_FILE, &db);
    std::string result;

    if(!params.count("email") || !params.count("target") || !params.count("password")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\' and \'password\' \'target\'\"}";
    }

    // Check if the user is admin
    if(!isAdmin(params["email"], params["password"], db))
        return "{\"request\": \"1\", \"message\": \"Permission denied.\"}";

    // Insert target
    std::string sql = "INSERT INTO Admins (user_id) VALUES ((SELECT id FROM Users WHERE email = ?))";
    sqlite3_stmt *stmt = nullptr;
    
    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr)) {
        sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
    }

    sqlite3_bind_text(stmt, 1, params["target"].c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(stmt) == SQLITE_DONE) {
        int admin_id = sqlite3_last_insert_rowid(db);
		result = "{\"request\": \"0\", \"message\": \"Success!\", \"admin_id\": \"" + std::to_string(admin_id) + "\"}";
    } else
		result = "{\"request\": \"1\", \"message\": \"Failed to delete account.\"}";

    sqlite3_finalize(stmt);

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
std::string adminDel(std::map<std::string, std::string> params) {
    DBG_PRINT("User called admindel\n");
	sqlite3 *db;
	int rc = sqlite3_open(DATABASE_FILE, &db);
    std::string result;

    if(!params.count("email") || !params.count("target") || !params.count("password")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\' and \'password\' \'target\'\"}";
    }

    if(!isAdmin(params["email"], params["password"], db))
		return "{\"request\": \"1\", \"message\": \"Permission denied.\"}";

    // Remove target
    std::string sql = "DELETE FROM Admins WHERE user_id = (SELECT id FROM Users WHERE email = ?)";
    sqlite3_stmt *stmt = nullptr;
    
    if(sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr)) {
        sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
    }

    sqlite3_bind_text(stmt, 1, params["target"].c_str(), -1, SQLITE_TRANSIENT);

    if(sqlite3_step(stmt) == SQLITE_DONE) 
		result = "{\"request\": \"1\", \"message\": \"Successfully deleted account.\"}";
    else
		result = "{\"request\": \"1\", \"message\": \"Failed to delete account.\"}";

    sqlite3_finalize(stmt);
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
    DBG_PRINT("User called addmovie\n");
	sqlite3 *db;
	int rc = sqlite3_open(DATABASE_FILE, &db);

	if (!params.count("email") || !params.count("password") || !params.count("movie_name") || !params.count("showtime") || !params.count("price") || !params.count("rating")) { // if ANY of these dont exist, returns error
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\', \'password\', \'movie_name\', \'showtime\', \'price\', \'rating\'\"}";
	}

    // Check if user is admin
    if(!isAdmin(params["email"], params["password"], db))
		return "{\"request\": \"1\", \"message\": \"Permission denied.\"}";

	std::string sql = "INSERT INTO Movies(name,showtime,price_per_ticket,rating) VALUES (?,?,?,?)";
	sqlite3_stmt *stmt = nullptr;

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
    DBG_PRINT("User called delmovie\n");
	sqlite3 *db;
	int rc = sqlite3_open(DATABASE_FILE, &db);

	if (!params.count("email") || !params.count("password") || !params.count("movie_id")){ // if ANY of these dont exist, returns error
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\', \'password\', and \'movie_id\'\"}";
	}

    // Check if user is admin
    if(!isAdmin(params["email"], params["password"], db))
		return "{\"request\": \"1\", \"message\": \"Permission denied.\"}";

	std::string sql = "DELETE FROM Movies WHERE id = ?";
	sqlite3_stmt *stmt = nullptr;

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["movie_id"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_DONE) {
		result = "{\"request\": \"0\", \"message\": \"Success!\"}";
	} else result = "{\"request\": \"1\", \"message\": \"Failed to delete movie.\"}";

	sqlite3_finalize(stmt);

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
    DBG_PRINT("User called accdetails\n");
	sqlite3 *db;
	int rc = sqlite3_open(DATABASE_FILE, &db);
    std::string sql, result;

	// check required fields. look above, for example
    if (!params.count("email") || !params.count("password")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\' and \'password\'\"}";
    }

    if(!isAdmin(params["email"], params["password"], db))
		return "{\"request\": \"1\", \"message\": \"Permission denied.\"}";

	sqlite3_stmt *stmt = nullptr;

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
                row += std::string((const char*)sqlite3_column_text(stmt,i)) + "\t";
            entries += row + "\n";
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
    DBG_PRINT("User called listmovies\n");
	sqlite3 *db;
	std::string sql,result;
	int rc = sqlite3_open(DATABASE_FILE, &db);
    char list_mode = 0x0; // Bitflag for options

    sql = "SELECT * FROM Movies";

    if (params.count("movie_name")) {
        sql += " WHERE name LIKE CONCAT(?, '%')";
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
            for(char i = 0; i < 6; i++)
                row += std::string((const char*)sqlite3_column_text(stmt,i)) + "\t";
            entries += row + "\n";
        } while((rc = sqlite3_step(stmt)) == SQLITE_ROW);

		result = "{\"request\": \"0\", \"message\": \"Success!\", \"movies\": \"" + entries + "\"}";
	} else result = "{\"request\": \"1\", \"message\": \"Failed to get movie info.\"}";

	sqlite3_finalize(stmt);

	sqlite3_close(db);
	return result;
}

/*
 * Returns:
 * - User ID
 */
std::string verifyAcc(std::map<std::string, std::string> params) {
    DBG_PRINT("User called verifyacc\n");
	sqlite3 *db;
	if (sqlite3_open(DATABASE_FILE, &db)) {
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
    DBG_PRINT("User called getmovie\n");
	sqlite3 *db;
	int rc = sqlite3_open(DATABASE_FILE, &db);

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
    DBG_PRINT("User called adminverify\n");
	sqlite3 *db;
	std::string result;
	int rc = sqlite3_open(DATABASE_FILE, &db);
	if (rc) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Unable to open database.\"}";
	}

	if (!params.count("email") || !params.count("password")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs 'email' and 'password'\"}";
	}

    // This isn't quite as complete but avoids rewriting the same thing
    if(isAdmin(params["email"], params["password"], db))
		result = "{\"request\": \"0\", \"message\": \"User is admin.\"}";
    else result = "{\"request\": \"1\", \"message\": \"User is not an admin.\"}";

	//sqlite3_finalize(stmt);
	sqlite3_close(db);
	return result;
}

/*
 * Requirements:
 * - Email
 * - Hashed Password
 * - Movie ID
 * - Review
 *
 * Returns:
 * - Fail/Success
 */
std::string reviewAdd(std::map<std::string, std::string> params) {
    DBG_PRINT("User called reviewadd\n");
    sqlite3* db = NULL;
    std::string sql,result;
    sqlite3_stmt* stmt = NULL;

    sqlite3_open(DATABASE_FILE, &db);
    if(!params.count("email") || !params.count("password") || !params.count("movie_id") || !params.count("review")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\', \'password\', and \'review\'\"}";
    }
    
    sql = "INSERT INTO Reviews(user_id, movie_id, review) VALUES ((SELECT id FROM Users WHERE email = ? AND password = ?), ?, ?)";

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

    sqlite3_bind_text(stmt, 1, params["email"].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, params["password"].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, params["movie_id"].c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, params["review"].c_str(), -1, SQLITE_TRANSIENT);

	if (sqlite3_step(stmt) == SQLITE_DONE) {
		result = "{\"request\": \"0\", \"message\": \"Success!\"}";
	} else result = "{\"request\": \"1\", \"message\": \"Failed to submit review.\"}";

	sqlite3_finalize(stmt);
	sqlite3_close(db);

    return result;
}

/*
 * Requirements:
 * - Movie ID
 *
 * Returns:
 * - Fail/Success
 * - Reviewer ID
 * - Review
 */
std::string reviewList(std::map<std::string, std::string> params) {
    DBG_PRINT("User called reviewlist\n");
    sqlite3* db = NULL;
    std::string sql,result;
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_open(DATABASE_FILE, &db);

    if(!params.count("movie_id")) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'movie_id\'\"}";
    }

    sql = "SELECT user_id, review FROM Reviews WHERE movie_id = ?";

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

    sqlite3_bind_text(stmt, 1, params["movie_id"].c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
        std::string entries = "";
        do {
            std::string row = "";
            for(char i = 0; i < 2; i++)
                row += std::string((const char*)sqlite3_column_text(stmt,i)) + "\t" ;
            entries += row + "\n";
        } while((rc = sqlite3_step(stmt)) == SQLITE_ROW);
		result = "{\"request\": \"0\", \"message\": \"Success!\", \"reviews\": \"" + entries + "\"}";
	} else result = "{\"request\": \"1\", \"message\": \"Failed to select reviews.\"}";

	sqlite3_finalize(stmt);
	sqlite3_close(db);

    return result;
}

/*
 * Requirements:
 * - Email (w. admin status)
 * - Hashed Password
 * - Theater Name
 *
 * Returns:
 * - Fail/Success
 * - Theater ID
 */
std::string addTheater(std::map<std::string, std::string> params) {
    DBG_PRINT("User called addtheater\n");
	sqlite3 *db;
	int rc = sqlite3_open(DATABASE_FILE, &db);

	if (!params.count("email") || !params.count("password") || !params.count("theater_name")) { // if ANY of these dont exist, returns error
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\', \'password\', and \'theater_name\'\"}";
	}

    // Check if user is admin
    if(!isAdmin(params["email"], params["password"], db))
		return "{\"request\": \"1\", \"message\": \"Permission denied.\"}";

	std::string sql = "INSERT INTO Theaters(name) VALUES (?)";
	sqlite3_stmt *stmt = nullptr;

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["theater_name"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_DONE) {
		int theater_id = sqlite3_last_insert_rowid(db); // This prolly needs to be changed
		result = "{\"request\": \"0\", \"message\": \"Success!\", \"theater_id\": \"" + std::to_string(theater_id) + "\"}";
	} else result = "{\"request\": \"1\", \"message\": \"Failed to add theater.\"}";

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return result;
}

/*
 * Requirements:
 * - Email (w. admin status)
 * - Hashed Password
 * - Theater ID
 *
 * Returns:
 * - Fail/Success
 */
std::string delTheater(std::map<std::string, std::string> params) {
    DBG_PRINT("User called addtheater\n");
	sqlite3 *db;
	int rc = sqlite3_open(DATABASE_FILE, &db);

	if (!params.count("email") || !params.count("password") || !params.count("theater_id")) { // if ANY of these dont exist, returns error
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\', \'password\', and \'theater_id\'\"}";
	}

    // Check if user is admin
    if(!isAdmin(params["email"], params["password"], db))
		return "{\"request\": \"1\", \"message\": \"Permission denied.\"}";

	std::string sql = "DELETE FROM Theaters WHERE id = ?";
	sqlite3_stmt *stmt = nullptr;

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["theater_id"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_DONE) {
		result = "{\"request\": \"0\", \"message\": \"Success!\"}";
	} else result = "{\"request\": \"1\", \"message\": \"Failed to add movie.\"}";

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return result;
}

/*
 * Requirements:
 * - Theater name
 *
 * Returns:
 * - Fail/Success
 */
std::string getTheater(std::map<std::string, std::string> params) {
    DBG_PRINT("User called addtheater\n");
	sqlite3 *db;
	int rc = sqlite3_open(DATABASE_FILE, &db);

	if (!params.count("theater_name")) { // if ANY of these dont exist, returns error
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Missing fields: Needs \'email\', \'password\', and \'theater_id\'\"}";
	}

	std::string sql = "SELECT id FROM Theaters WHERE name = ?";
	sqlite3_stmt *stmt = nullptr;

	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		sqlite3_close(db);
		return "{\"request\": \"1\", \"message\": \"Failed to prepare statement.\"}";
	}

	sqlite3_bind_text(stmt, 1, params["theater_id"].c_str(), -1, SQLITE_TRANSIENT);

	std::string result;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string entries = std::string((const char*)sqlite3_column_text(stmt,0));
		result = "{\"request\": \"0\", \"message\": \"Success!\", \"id\": \"" + entries + "\"}";
	} else result = "{\"request\": \"1\", \"message\": \"Failed to add movie.\"}";

	sqlite3_finalize(stmt);
	sqlite3_close(db);

	return result;
}


//################################################
int main(int argc, char* argv[]) {
    fprintf(stderr, "\x1b[31;1mWARNING\x1b[0;0m: THIS IS NOT PRODUCTION READY CODE!!!\n");
    
	try {
		asio::io_context ioc;

		int sqlData = sqLiteInitialize();
		if(sqlData == 1) {
			return(1);
		}

		tcp::acceptor acceptor(ioc, {tcp::v4(), PORT});
		std::cout << "Server listening on http://localhost:" << std::to_string(PORT) << "/\n";
		
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
