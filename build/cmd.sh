./a.out "SELECT name,age FROM user WHERE id = (SELECT id FROM admin)"
./yanshi sql.txt --dump-automaton --dump-assoc > 1.log

./a.out "SELECT*FROMA"
grep elapse  | sort -k7,7nr | less

