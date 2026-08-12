./a.out "SELECT name,age FROM user WHERE id = (SELECT id FROM admin)"
./yanshi sql.txt --dump-automaton --dump-assoc > 1.log

./a.out "SELECT*FROMA"
grep elapse  | sort -k7,7nr | less

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

