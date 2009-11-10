
all: ctags workspace
	make -C rs all
	cp rs/rsmanager workspace/rsmanager
	
workspace: 
	mkdir workspace

ctags:
	ctags rs/*.{cc,hh}

clean:
	make -C rs clean
	rm -f tags

