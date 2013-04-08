CPP = g++

INCLUDE = -I./ -I/home/project/jsoncpp-src-0.6.0-rc2/include -I/home/project/lab/log4cplus_bin/include
LIBS = -lpthread
C_ARGS = -g -Wall -D_FILE_OFFSET_BITS=64 -D__STDC_FORMAT_MACROS -D_REENTRANT $(INCLUDE) 

BINARY = demo
demo_dep = calypso.o allocator.o netlink.o netlink_config.o \
        calypso_network.o demo_app.o calypso_interface.o calypso_bootstrap_config.o \
        calypso_runtime_config.o calypso_util.o ring_queue.o
demo_lib = -L/home/project/jsoncpp-src-0.6.0-rc2 -ljsoncpp -L/home/project/lab/log4cplus_bin/lib -llog4cplus

ALL_OBJS = $(foreach d,$(BINARY),$($(d)_dep))

%.o : %.cpp
	$(CPP) $(C_ARGS) -c  $< -o $(patsubst %.cpp,%.o,$<)
%.o : %.cc
	$(CPP) $(C_ARGS) -c  $< -o $(patsubst %.cc,%.o,$<)
%.o : %.c
	$(CPP) $(C_ARGS) -c  $< -o $(patsubst %.c,%.o,$<)
	
all : $(BINARY)

$(BINARY) : $(ALL_OBJS)
	@echo "now building:" $@
	@echo "dep:" $($@_dep)
	rm -f $@
	$(CPP) $(C_ARGS) -o $@  $($@_dep) $(LIBS) $($@_lib)

clean:
	rm -f $(ALL_OBJS) $(BINARY)

print:
	@echo "print all vars"
	@echo "all objs:" $(ALL_OBJS)
