CXXFLAGS += -std=c++14 -Wall -g

ebnf: algorithm.o ebnf.o rd.o lexer.o parse.o
	c++ -o $@ $^

preamble.inc: preamble.in
	./gen

lexer.o: tokens.h

algorithm.o: algorithm.cpp ebnf.h
ebnf.o: ebnf.cpp ebnf.h preamble.inc
rd.o: rd.cpp ebnf.h
parse.o: parse.cpp ebnf.h tokens.h

clean:
	rm -f ebnf *.o lexer.c

.PHONY: clean
