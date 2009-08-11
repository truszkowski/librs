
all: ctags workspace
	make -C rs all
	make -C bot all
	cp bot/Bot workspace/Bot
	@echo "Everything done!"
	
workspace: 
	mkdir workspace

ctags:
	@ctags ./*/*.{cc,hh}

clean:
	make -C rs clean
	make -C bot clean
	@rm -f tags
	@echo "Everything cleaned!"

