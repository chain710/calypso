CPP = g++

INCLUDE = -I../ -I/home/project/jsoncpp-src-0.6.0-rc2/include
LIBS =
C_ARGS = -g -Wall -D_FILE_OFFSET_BITS=64 -D__STDC_FORMAT_MACROS $(INCLUDE) 
USER_DIR = ..

BINARY = mass_client
mass_client_dep = mass_client.o $(USER_DIR)/allocator.o $(USER_DIR)/netlink.o $(USER_DIR)/netlink_config.o \
        $(USER_DIR)/calypso_network.o $(USER_DIR)/calypso_util.o
mass_client_lib = -L/home/project/jsoncpp-src-0.6.0-rc2 -ljsoncpp -L/home/project/lab/log4cplus_bin/lib -llog4cplus

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
	$(CPP) $(C_ARGS) -o $@  $($@_dep) $(LIBS) $($@_lib) -ldl -lpthread

clean:
	rm -f $(ALL_OBJS) $(BINARY)

print:
	@echo "print all vars"
	@echo "all objs:" $(ALL_OBJS)
