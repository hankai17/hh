./a.out "SELECT name,age FROM user WHERE id = (SELECT id FROM admin)"
./yanshi sql.txt --dump-automaton --dump-assoc > 1.log

./a.out "(-exprbase || b * (c + a) + b & a >> 3 <= 9),(a),(SELECT * FROM aa-,)"
./a.out "SELECT*FROMA"
grep elapse  | sort -k7,7nr | less

./a.out "SELECT abc,d FROM user WHERE id = (SELECT abc,e FROM a)"
./a.out "SELECT a  FROM user WHERE id = (SELECT a  FROM a )"
./a.out "SELECT a FROM user WHERE id = (SELECT a FROM a )"

SELECT *
FROM t
WHERE
    -a + b * c / d % e || f -> g ->> h << i >> j & k | l < m
    AND
    a <= n
    AND
    b > o
    AND
    c >= p;

