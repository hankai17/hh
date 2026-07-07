#CXXFLAGS := -g3 -std=c++1y -fsanitize=undefined,address
CXXFLAGS := -g -O0 -std=c++1y -fsanitize=undefined,address
LDFLAGS  := -fsanitize=undefined,address
O := lexer_helper.o lexer.o location.o main.o parser.o syntax.o

all: hh

hh: $O
	$(LINK.cc) $^ -o $@

parser.o: lexer.hh
lexer.o: parser.hh
main.o: parser.hh syntax.hh

lexer.cc lexer.hh: lexer.l
	flex --header-file=lexer.hh -o lexer.cc $<

parser.cc parser.hh: parser.y location.hh syntax.hh
	#bison --defines=parser.hh -o parser.cc $< -Wcounterexamples
	bison --defines=parser.hh -o parser.cc $<

clean:
	$(RM) *.o lexer.cc lexer.hh parser.cc parser.hh

distclean: clean
	$(RM) {lexer,parser}.{cc,hh}

.PHONY: all clean distclean
