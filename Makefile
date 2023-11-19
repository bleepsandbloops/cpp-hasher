standard:
	g++ -std=c++11 -O3 hasher.cpp -o bin/hasher -lssl -lcrypto

debug:
	g++ -std=c++11 -O3 hasher.cpp -o bin/hasher -lssl -lcrypto -g -ggdb
