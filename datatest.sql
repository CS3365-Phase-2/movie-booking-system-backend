-- SELECT * FROM Users;
SELECT * FROM Movies;
-- SELECT * FROM Theaters;
-- SELECT * FROM Admins A INNER JOIN Users U ON A.user_id = U.id;

SELECT * FROM Tickets;

SELECT M.name, SUM(T.quantity), SUM(T.id * M.price_per_ticket) AS 'total'
    FROM Movies M
        INNER JOIN Tickets T ON M.id = T.movie_id
        INNER JOIN Theaters H ON H.id = M.theater_id
    GROUP BY M.name
    ORDER BY total DESC;
