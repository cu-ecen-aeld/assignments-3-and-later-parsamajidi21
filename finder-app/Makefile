include source.mk



ifeq ($(TAR), CROSS_COMPILE)
	CC=aarch64-none-linux-gnu-gcc
	LD=aarch64-none-linux-gnu-ld
else
	CC=gcc
	LD=ld
endif

BASENAME = writer
TARGET = $(BASENAME)

OBJS = $(SRCS:.c=.o)

%.o : %.c
	$(CC) -c $< -o $@

.PHONY: build
build: all

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@

.PHONY: clean
clean: 
	rm -f $(OBJS) $(TARGET) 
