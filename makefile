
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
	dd bs=1024 count=1 if=/dev/urandom of=root/blocks-one.bin
	dd bs=1024 count=5 if=/dev/urandom of=root/blocks-direct.bin
	./genfile.rb 5 >root/blocks.txt
	echo hello >root/file.txt
	touch root/tmp.bin
	genext2fs -b $$((16 * 1024)) -d root $(IMG)

test-read:
	$(BIN) $(IMG) cat /file.txt | diff - root/file.txt
	$(BIN) $(IMG) cat /blocks-one.bin | diff - root/blocks-one.bin
	$(BIN) $(IMG) cat /blocks-direct.bin | diff - root/blocks-direct.bin

test-write:
	cat root/blocks-one.bin | $(BIN) $(IMG) write /tmp.bin
	$(BIN) $(IMG) cat /tmp.bin | diff - root/blocks-one.bin
	cat root/blocks-direct.bin | $(BIN) $(IMG) write /tmp.bin
	$(BIN) $(IMG) cat /tmp.bin | diff - root/blocks-direct.bin

test: $(IMG) all
	$(BIN) $(IMG) ls /
	$(BIN) $(IMG) create hello.txt
	echo hello | $(BIN) $(IMG) write hello.txt
	$(BIN) $(IMG) stat hello.txt
