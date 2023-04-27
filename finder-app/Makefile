include source.mk



ifeq ($(CROSS_COMPILE), aarch64-none-linux-gnu-)
	CC=aarch64-none-linux-gnu-gcc
else
	CC=gcc
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
