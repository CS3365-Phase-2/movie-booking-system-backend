Movie Booking System
====================

How to use:
-----------
When ran from the terminal, the default port for the webui is `http://localhost:4444`. You can use a reverse proxy solution to host the API on the wider internet. To interact with the api, you can do the following GET curl request (as an example:)
```
curl "https://mbs.lavato.net?userid=12345678&password=12345678&action=addmovie&movieid=12345678"
```

There are multiple different "actions" that can be used from the URL (for the average user:)
- createacc (to create an account)
- deleteacc (to delete an account)
- buyticket (to buy a ticket)
- getticket (to get the tickets bought)

For admins, there are a few other "actions" that can also be used:
- adminadd (adds admin role to a user)
- admindel (removes admin role from a user)
- addmovie (adds movie)
- delmovie (deletes movie)

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
