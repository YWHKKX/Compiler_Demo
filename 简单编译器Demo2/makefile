OBJS=main.o tool1.o tool2.o
CC=gcc
FL=flex
BI=bison

CFLAGS=-c -Wall -g

all:
	$(FL) $^ lexer.l
	$(BI) $^ -d -v parser.y
	$(CC) $^ display.c semantic_analysis.c parser.tab.c lex.yy.c -lfl -o trans
