CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -g
LDFLAGS = -lm

INCDIR = include
BINDIR = bin

SOURCES = main.c lexer/lexer.c parser/parser.c typecheck/typecheck.c backend_vm/vm.c backend_c/cgen.c
OBJECTS = $(addprefix $(BINDIR)/,$(notdir $(SOURCES:.c=.o)))
TARGET = jag

.PHONY: all clean install uninstall test

all: $(BINDIR) $(TARGET)

$(BINDIR):
	mkdir -p $(BINDIR)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(BINDIR)/%.o: %.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(BINDIR)/%.o: lexer/%.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(BINDIR)/%.o: parser/%.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(BINDIR)/%.o: typecheck/%.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(BINDIR)/%.o: backend_vm/%.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(BINDIR)/%.o: backend_c/%.c
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

clean:
	rm -rf $(BINDIR) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/
	chmod +x /usr/local/bin/$(TARGET)

uninstall:
	rm -f /usr/local/bin/$(TARGET)

test: $(TARGET)
	./$(TARGET) --version
	./$(TARGET) --help
