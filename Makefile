CC = gcc
CFLAGS = -Wall -Werror -std=gnu89 -DCOLOR

USER=cse533-14
TMP=tour arp
BINS=$(TMP:=_$(USER))

all: $(BINS)

debug: CFLAGS += -DDEBUG -g
debug: $(BINS)

arp_%: arp.o get_hw_addrs.o
	$(CC) -o $@ $^

tour_%: tour.o api.o
	$(CC) -o $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

prhwaddrs: prhwaddrs.c get_hw_addrs.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o $(BINS) prhwaddrs

PHONY: all clean
SECONDARY: tour.o arp.o api.o get_hw_addrs.o
