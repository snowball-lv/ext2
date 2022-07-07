
BIN = bin/ext2
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:src/%.c=out/%.o)
DEPS = $(SRCS:src/%.c=out/%.d)

CFLAGS = -c -O2 -MMD -I inc -Wall

IMG = image.ext2

all: $(BIN)

-include $(DEPS)

out:
	mkdir out

out/%.o: src/%.c | out
	$(CC) $(CFLAGS) $< -o $@

bin:
	mkdir bin

$(BIN): $(OBJS) | bin
	$(CC) $^ -o $@

clean:
	rm -rf out bin root
	rm -f $(IMG)

$(IMG):
	mkdir root
	./genfile.rb 1 >root/blocks-one.txt
	./genfile.rb 1 >root/blocks-direct.txt
	genext2fs -b $$((16 * 1024)) -d root $(IMG)

test: $(IMG) all
	$(BIN) $(IMG) ls /
