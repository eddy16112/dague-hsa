CC=gcc
YACC=yacc -d -y --verbose
LEX=flex -d
CFLAGS=-Wall -pedantic -ansi -g
LDFLAGS=

all: parse

parse: expr.o lex.yy.o y.tab.o
	$(CC) -o parse expr.o lex.yy.o y.tab.o $(LDFLAGS)

test-expr.o: test-expr.c $(wildcard *.h)
	$(CC) -o test-expr.o $(CFLAGS) -c test-expr.c

expr.o: expr.c $(wildcard *.h)
	$(CC) -o expr.o $(CFLAGS) -c expr.c

y.tab.o: y.tab.c $(wildcard *.h)
	$(CC) -o y.tab.o $(CFLAGS) -c y.tab.c

y.tab.h y.tab.c: dplasma.y
	$(YACC) dplasma.y

lex.yy.o: lex.yy.c y.tab.h $(wildcard *.h)
	$(CC) -o lex.yy.o $(CFLAGS) -c lex.yy.c

lex.yy.c: dplasma.l
	$(LEX) dplasma.l

clean:
	rm -f *.o test-expr lex.yy.c y.tab.c y.tab.h
