# Makefile

# A Makefile isn't really necessary at this point, because there is only
# one source file.  I include it anyway to serve as a nucleus in case
# catpath gets fancier and needs a fancier build.  Also it's a
# convenient way to apply compiler options.

targets = catpath

CXX = g++
CFLAGS = -ansi -pedantic -Wall -Wextra
CXXFLAGS = $(CFLAGS)

all : $(targets)

catpath : catpath.cpp
	$(CXX) $(CXXFLAGS) catpath.cpp -o catpath

clean :
	rm -f *.o $(targets)

