CXXFLAGS := -ggdb3 -O1 -Wall -Wextra -I.. -I. -I/usr/include/apr-1/ -D_FILE_OFFSET_BITS=64 

src := $(wildcard *.cpp)
obj := $(subst .cpp,.o,$(src))

all: ctags deps libHttp.a libRS.a rsmanager

test: test_http test_rs

ctags:
	ctags *.{cpp,hpp}

deps:
	g++ -MM *.cpp -I.. > Makefile.deps

-include Makefile.deps

libHttp.a: Http.o
	rm -f $@
	ar cq $@ $^

libRS.a: RS.o
	rm -f $@
	ar cq $@ $^

.cc.o:
	g++ $(CXXFLAGS) -c $<

rsmanager: rsmanager.o libHttp.a libRS.a
	g++ -ggdb3 -O1 -L. -o rsmanager rsmanager.o -lRS -lHttp \
	  -lboost_thread -lboost_regex -lboost_filesystem -lcurl -lpthread

test_http: test_http.o libHttp.a 
	g++ -ggdb3 -O1 -L. -o test_http test_http.o -lHttp -lcurl

test_rs: test_rs.o libRS.a libHttp.a
	g++ -ggdb3 -O1 -L. -o test_rs test_rs.o -lRS -lHttp \
	  -lboost_thread -lboost_regex -lboost_filesystem -lcurl -lpthread

clean:
	rm -f *.o *.a Makefile.deps core core.* tags rsmanager test_http test_rs

