CC=$(CROSS_COMPILE)gcc
CFLAGS=-I.
OBJ= writer.o

%.o: %.c 
	$(CC) -c -o $@ $< $(CFLAGS)

writer: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)


clean:
	rm -f *.o writer
