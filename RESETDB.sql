DELETE 
    FROM Movies
    WHERE id = id;
DELETE 
    FROM Users
    WHERE id = id;
DELETE 
    FROM Admins
    WHERE user_id = user_id;
DELETE 
    FROM Tickets
    WHERE id = id;
