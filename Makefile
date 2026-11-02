CC ?= gcc
CFLAGS ?= -Wall -Wextra

SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
TARGET = temperature-controller

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
