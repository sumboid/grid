cc := mpicc.mpich2
cflags := -std=gnu99
run := mpirun.mpich2

exec := ./1
src := main.c global.c communication.c state.c
obj := $(src:.c=.o)

all: $(exec)

$(exec): $(obj)
	$(cc) $(obj) -o $@

.c.o:
	$(cc) $(cflags) -c $<

run: $(exec)
	rm -f current-port
	$(run) -np 5 $(exec) &
	sleep 3
	$(run) -np 10 $(exec)

clean:
	rm -f $(obj) $(exec)

.PHONY: clean run
