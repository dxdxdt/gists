CC ?= cc

.PHONY: test clean

EXEC = memalign-errno

test: $(EXEC)
	./$(EXEC) || true

$(EXEC): $(EXEC).c
	$(CC) -Wall -Wextra -std=c11 -O0 -g -o $(EXEC) $(EXEC).c $(LD_ADD)

clean:
	rm -f $(EXEC)
