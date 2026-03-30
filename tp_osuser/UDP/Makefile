CC=cc
CFLAGS=-Wall -Werror

all: servudp cliudp servbeuip clibeuip

cliudp: cliudp.c
	$(CC) $(CFLAGS) -o cliudp cliudp.c

servudp: servudp.c
	$(CC) $(CFLAGS) -o servudp servudp.c

clibeuip: clibeuip.c
	$(CC) $(CFLAGS) -o clibeuip clibeuip.c

servbeuip: servbeuip.c
	$(CC) $(CFLAGS) -o servbeuip servbeuip.c

trace: CFLAGS += -DTRACE
trace: clean all

clean:
	rm -f cliudp servudp clibeuip servbeuip

