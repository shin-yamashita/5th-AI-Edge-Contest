
CC            = gcc
LINKER        = $(CC)

CFLAGS        = -O -g -Wall -fPIC
LDFLAGS       = -O -g -Wall
LFLAGS        = -lcma

SFLAG         = -Wl,-soname,$(SLIB).1

SLIB          = libtrack_if.so

OBJS = track_if.o

all:	$(SLIB).1.0

$(SLIB).1.0:	$(OBJS)
				@echo  "Make Shared liblaly Loading $(SLIB) ... "
				$(LINKER) -shared $(SFLAG) -o $(SLIB).1.0 $(OBJS) $(LFLAGS)

clean:	
	rm $(OBJS) $(SLIB).1.0
	