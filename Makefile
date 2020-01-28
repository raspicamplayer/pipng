TARGETS=pipng

default :all

all:
	for target in $(TARGETS); do ($(MAKE) -C $$target); done

clean:
	for target in $(TARGETS); do ($(MAKE) -C $$target clean); done

install:
	for target in $(TARGETS); do ($(MAKE) -C $$target install); done

uninstall:
	for target in $(TARGETS); do ($(MAKE) -C $$target uninstall); done

