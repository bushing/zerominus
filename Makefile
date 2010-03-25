CFLAGS = -O0 -g -Wall -W -std=gnu99 `pkg-config --cflags libusb-1.0`
LDFLAGS = -g -Wall -std=gnu99 `pkg-config --libs libusb-1.0`
ZEROMINUS_OBJS = analyzer.o gl.o vcd.o zerominus.o

all: zerominus

clean:
	-rm -f zerominus $(ZEROMINUS_OBJS)

zerominus: $(ZEROMINUS_OBJS)
	$(CC) $(LDFLAGS) -o $@ $^
