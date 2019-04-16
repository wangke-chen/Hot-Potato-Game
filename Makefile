TARGETS=ringmaster player

all: $(TARGETS)
clean:
	rm -f $(TARGETS)

ringmaster: ringmaster.c
	gcc -g -o $@ $<

player: player.c
	gcc -g -o $@ $<

