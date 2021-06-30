CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Og $(shell pkg-config --cflags glib-2.0 julea julea-item)
LDLIBS := $(shell pkg-config --libs glib-2.0 julea julea-item)

BIN = dedup-io

all: $(BIN)

clean:
	rm -f $(BIN)
