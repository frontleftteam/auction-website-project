sample files for reference

build & deploy
•	put all four binaries in your server’s /cgi-bin/ (or equivalent).
•	make sure they’re executable: chmod 755 *.cgi
•	compile:

g++ -O2 -std=c++17 auth.cpp      -lsqlite3 -o auth.cgi
gcc  -O2          transactions.c  -lsqlite3 -o transactions.cgi
g++ -O2 -std=c++17 bid_sell.cpp   -lsqlite3 -o bid_sell.cgi
g++ -O2 -std=c++17 list_open.cpp  -lsqlite3 -o list_open.cgi

•	ensure your process can read/write the DB file:
o	first run will create auction.db in the working dir of the CGI (often the cgi-bin folder). You can also precreate with sqlite3 auction.db < schema.sql.

