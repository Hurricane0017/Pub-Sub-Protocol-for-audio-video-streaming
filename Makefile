# "make" builds all three programs, "make clean" deletes them and the received/ folder.

all: broker publisher subscriber

# Rule: build program X from X.cpp (rebuild it if X.cpp or protocol.h changed).
# -g = keep debug info for lldb, -Wall = show warnings.
%: %.cpp protocol.h
	g++ -std=c++17 -g -Wall -o $@ $<

clean:
	rm -rf broker publisher subscriber *.dSYM received
