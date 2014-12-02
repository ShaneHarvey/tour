CC = gcc
CFLAGS = -Wall -Werror -std=gnu89 -DCOLOR

USER=cse533-14
TMP=tour arp
BINS=$(TMP:=_$(USER))
TESTS = check_cache

all: $(BINS)

debug: CFLAGS += -DDEBUG -g
debug: $(BINS)

arp_%: arp.o get_hw_addrs.o
	$(CC) -o $@ $^

tour_%: tour.o get_hw_addrs.o api.o ping.o
	$(CC) -pthread -o $@ $^

ping.o: ping.c ping.h
	$(CC) $(CFLAGS) -pthread -c $<

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

prhwaddrs: prhwaddrs.c get_hw_addrs.o
	$(CC) $(CFLAGS) -o $@ $^

test: CFLAGS += -g
test: minunit_cache.o cache.o
	$(CC) $(CFLAGS) -o $@ $^
	./test
	@rm -f test *cache.o

clean:
	rm -f *.o $(BINS) prhwaddrs test

PHONY: all clean
SECONDARY: tour.o arp.o api.o get_hw_addrs.o
