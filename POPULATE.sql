INSERT INTO Movies (
    name,
    showtime,
    price_per_ticket,
    rating,
    theater_id 
) VALUES ('movie_1', '5:00', 3.02, 'G', 1),
         ('movie_2', '4:00', 5.3, 'R', 2),
         ('movie_3', '3:00', 5.02, 'PG13', 1),
         ('another_movie', '3:00', 6.19, 'NC17', 3);


INSERT INTO Users (
    name,
    email,
    password,
    payment_details 
) VALUES ('greg', 'greg@email.com', 'abcd', 'venmo'),
         ('george', 'george@georgemail.net', '1234', 'paypal'),
         ('bob', 'bob@bob.com', '1234', 'paypal'),
         ('aaa', 'bbb', 'ccc', 'credit'), 
         ('a', 'b', 'c', 'credit');

INSERT INTO Admins (
   user_id 
) VALUES ((SELECT id FROM Users WHERE email = 'b' AND password = 'c'));

INSERT INTO Theaters (
    name
) VALUES ('theater_1'),
         ('theater_2'),
         ('theater_3');

INSERT INTO Tickets (
    user_id,
    movie_id,
    quantity,
    purchase_time
) 
VALUES
( 1, 1, 3, 0 ),
( 2, 1, 3, 0 ),
( 1, 2, 1, 0 ),
( 3, 1, 3, 0 );

