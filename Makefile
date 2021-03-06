CC = g++
SRC := circuit.cpp
BIN = circuit-simulator-burrow.app

.PHONY: clean

build: $(SRC)
	$(CC) $^ -o $(BIN)

O3: $(SRC)
	$(CC) $^ -o $(BIN) -O3

clean:
	rm -rf $(BIN) *.o
