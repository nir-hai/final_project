CC = gcc
CFLAGS = -Wall -ansi -pedantic
TARGET = assembler
SOURCES = main.c first_pass.c second_pass.c symbols.c opcodes.c pre_assembler.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) *.ob *.ent *.ext *.am

.PHONY: all clean