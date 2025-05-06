INSERT INTO Movies (
    name,
    showtime,
    price_per_ticket,
    rating
) VALUES ('movie_1', '5:00', 3.02, 'G'),
         ('movie_2', '4:00', 5.3, 'R'),
         ('movie_3', '3:00', 5.02, 'PG13'),
         ('another_movie', '3:00', 6.19, 'NC17');


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
