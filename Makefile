TARGET = DoveFly
SRC = $(wildcard *.c)
OBJ = ${SRC:.c=.o}

CC = tcc
CFLAGS = -c -O2 -Wall
DFLAGS = -lncurses -lpthread

$(TARGET): $(OBJ)
	$(CC) $< -o $@ $(DFLAGS)

%.o: %.c
	$(CC) $< -o $@ $(CFLAGS)

clean:
	rm $(TARGET) $(OBJ)
