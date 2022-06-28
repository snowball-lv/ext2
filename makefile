
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
	./genfile.rb 1 >root/blocks-1.txt
	./genfile.rb 5 >root/blocks-5.txt
	./genfile.rb 100 >root/blocks-100.txt
	genext2fs -b $$((128 * 1024)) -d root $(IMG)

test: $(IMG) all
	$(BIN) $(IMG) ls /
	$(BIN) $(IMG) cat /file.txt
	cat root/blocks-100.txt | $(BIN) $(IMG) write /file.txt
	$(BIN) $(IMG) ls /
	$(BIN) $(IMG) cat /file.txt