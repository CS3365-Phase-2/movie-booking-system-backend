Movie Booking System
====================

How to use:
-----------
When ran from the terminal, the default port for the webui is `http://localhost:4444`. You can use a reverse proxy solution to host the API on the wider internet. To interact with the api, you can do the following GET curl request (as an example:)
```
curl "mbs.lavato.net?action=addmovie&moviename=12345678&showtime=12345678&price=12345678&rating=pg13&adminid=12345678&password=12345678"
```

There are multiple different "actions" that can be used from the URL (for the average user:)
- `createacc` (to create an account)
  - Requires: Email/Username (Field: `email`)
  - Requires: Password (Field: `password`)
  - Requires: Name (Field: `name`)
  - Optional: Payment Details (Field: `payment_details`)
  - Returns: User ID (Field: `user_id`)
  - Returns: Message (Field: `message`)
- `delacc` (to delete an account)
  - Requires: Email/Username (Field: `email`)
  - Requires: Password (Field: `password`)
  - Returns: Message (Field: `message`)
- `buyticket` (to buy a ticket) Requires:
  - Requires: Email/Username (Field: `email`)
  - Requires: Password (Field: `password`)
  - Requires: Payment Details (Field: `payment_details`)
  - Requires: Ticket Amount (Field: `ticket_amount`)
  - Requires: Movie ID (Field: `movie_id`)
  - Returns: Message (Field: `message`)
- `getticket` (to get the tickets bought)
  - Requires: Email/Username (Field: `email`)
  - Requires: Password (Field: `password`)
  - Returns: Movie Name (Field: `movies["movie_name"]`)
  - Returns: Showtime (Field: `movies["showtime"]`)
  - Returns: Movie ID (Field: `movies["movie_id"]`)
  - Returns: Message (Field: `message`)
- `listmovies` (to show the list of movies available)
  - Requires: NONE
  - Returns: Movies (Field: `movie_id`)
  - Returns: Message (Field: `message`)
- `accdetails` (to show the details of the account)
  - Requires: Email/Username (Field: `email`)
  - Requires: Password (Field: `password`)
  - Returns: Message (Field: `message`)

For admins, there are a few other "actions" that can also be used:
- `adminadd` (adds admin role to a user)
  - Requires: Email/Username (Field: `email`)
  - Requires: Password (Field: `password`)
  - Requires: Target user ID (Field: `target`)
  - Returns: Admin ID (Field: `admin_id`)
  - Returns: Message (Field: `message`)
- `admindel` (removes admin role from a user)
  - Requires: Email/Username (Field: `email`)
  - Requires: Password (Field: `password`)
  - Requires: Target user ID (Field: `target`)
  - Returns: Message (Field: `message`)
- `addmovie` (adds movie)
  - Requires: Email/Username (Field: `email`)
  - Requires: Password (Field: `password`)
  - Requires: Movie Name (Field: `movie_name`)
  - Requires: Showtime (Field: `showtime`)
  - Requires: Price (Field: `price`)
  - Requires: Rating (Field: `rating`)
  - Returns: Movie ID (Field: `movie_id`)
  - Returns: Message (Field: `message`)
- `delmovie` (deletes movie)
  - Requires: Email/Username (Field: `email`)
  - Requires: Password (Field: `password`)
  - Requires: Movie ID (Field: `movie_id`)
  - Returns: Message (Field: `message`)

How to build:
-------------
Ensure you have the correct dependencies. Follow these steps to build the program:
```
git clone https://github.com/CS3365-Phase-2/movie-booking-system-backend.git
cd movie-booking-system-backend
make
```

If you want to contribute, use the git url when cloning:
```
git clone git@github.com:CS3365-Phase-2/movie-booking-system-backend.git
```

Dependencies:
-------------
- Libboost (including beast)
- Libsqlite
- Bits (stdc++)
For more details, read `Util.cpp`
