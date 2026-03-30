CC=cc
CFLAGS=-Wall -Werror
AR=ar
ARFLAGS=rcs

OBJS=creme.o
LIB=libcreme.a

all: triceps

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $(LIB) $(OBJS)

creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c creme.c

triceps.o: triceps.c creme.h
	$(CC) $(CFLAGS) -c triceps.c

triceps: triceps.o $(LIB)
	$(CC) $(CFLAGS) -o triceps triceps.o -L. -lcreme

trace: CFLAGS += -DTRACE
trace: clean all

symbols: $(LIB)
	nm -g $(LIB)

clean:
	rm -f triceps triceps.o creme.o $(LIB)

