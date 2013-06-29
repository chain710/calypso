CPP = g++

INCLUDE = -I../ -I/home/project/lab/log4cplus_bin/include
LIBS = 
C_ARGS = -g -Wall -D_FILE_OFFSET_BITS=64 -D__STDC_FORMAT_MACROS -D_REENTRANT $(INCLUDE) 

BINARY = demo
demo_dep = demo_app.o harvester.o
demo_lib = -L../ -lcalypso_main -L/home/project/jsoncpp-src-0.6.0-rc2 -ljsoncpp -llog4cplus -lgflags -lpthread -ldl

ALL_OBJS = $(foreach d,$(BINARY),$($(subst .,_,$(d))_dep))

%.o : %.cpp
	$(CPP) $(C_ARGS) -c  $< -o $(patsubst %.cpp,%.o,$<)
%.o : %.cc
	$(CPP) $(C_ARGS) -c  $< -o $(patsubst %.cc,%.o,$<)
%.o : %.c
	$(CPP) $(C_ARGS) -c  $< -o $(patsubst %.c,%.o,$<)
	
all : $(BINARY)

$(BINARY) : $(ALL_OBJS)
	@echo "now building:" $@
	@echo "dep:" $($(subst .,_,$@)_dep)
	rm -f $@
	$(CPP) $(C_ARGS) -o $@ $($(subst .,_,$@)_dep) $(LIBS) $($(subst .,_,$@)_lib)

clean:
	rm -f $(ALL_OBJS) $(BINARY)

print:
	@echo "print all vars"
	@echo "all objs:" $(ALL_OBJS)
