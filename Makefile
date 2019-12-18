## make file for shoop

all: shoop

DEBUG_LIBS=
# DEBUG_LIBS=-lefence -lpthread

ARGS=-g -Wall -O2
#RQ_LIBS=-lrispbuf -lrisp -lexpbufpool -lmempool -lexpbuf -levent_core -lrq -llinklist
#LIBS=$(RQ_LIBS) -lrq-http -lrq-blacklist -lrq-http-config -lparsehttp

OBJS=


 
shoop: shoop.c
	gcc -o $@ shoop.c $(OBJS) $(LIBS) $(ARGS)



install: shoop
	@cp shoop /usr/bin

clean:
	@-rm shoop
	@-rm $(OBJS)

