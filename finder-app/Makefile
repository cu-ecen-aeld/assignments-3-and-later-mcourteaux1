CC=$(CROSS_COMPILE)gcc
CFLAGS=-I.
OBJ= writer.o

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

writer: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

# ✅ This is what Buildroot calls during its build step
all: writer

clean:
	rm -f *.o writer

