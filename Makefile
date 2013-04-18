CPP = g++
AR = ar
INCLUDE = -I./ -I/home/project/jsoncpp-src-0.6.0-rc2/include -I/home/project/lab/log4cplus_bin/include
C_ARGS = -g -Wall -D_FILE_OFFSET_BITS=64 -D__STDC_FORMAT_MACROS -D_REENTRANT $(INCLUDE)
ARFLAGS = rcs
#r Insert the files member... into archive (with replacement). This operation differs from q in that any previously existing members are deleted if their names match those being added.
#c Create the archive.
#s Write an object-file index into the archive, or update an existing one, even if no other change is made to the archive.

BINARY = libcalypso_main.a
libcalypso_main_a_dep = calypso.o allocator.o netlink.o netlink_config.o \
        calypso_network.o calypso_bootstrap_config.o calypso_runtime_config.o calypso_signal.o \
        utility.o ring_queue.o app_interface.o timer_engine.o

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
	$(AR) $(ARFLAGS) $@ $($(subst .,_,$@)_dep)

clean:
	rm -f $(ALL_OBJS) $(BINARY)

print:
	@echo "print all vars"
	@echo "all objs:" $(ALL_OBJS)
