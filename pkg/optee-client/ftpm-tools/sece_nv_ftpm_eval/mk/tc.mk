

include ../conf.mk
include ../names.mk

CC      = $(CROSS_COMPILE)gcc
LD      = $(CROSS_COMPILE)ld
AR      = $(CROSS_COMPILE)ar
NM      = $(CROSS_COMPILE)nm
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
READELF = $(CROSS_COMPILE)readelf

O ?= out
LIB_O ?= out/lib
LIB_PUBLIC ?= lib/public
LIB_O_PUBLIC = $(LIB_O)/public

LIB_OBJPATH = $(LIB_O)/obj
LIB_OBJFILES = $(LIB_SRCS:.c=.o)
LIB_OBJS = $(patsubst $(LIB_SRCPATH)/%,$(LIB_OBJPATH)/%,$(LIB_OBJFILES))

LIB_CFLAGS += -Wall -I../include -I$(TEEC_EXPORT)/include -I$(LIB_PUBLIC)

ifeq ($(USE_LIBRARY),dynamic)
LIB_CFLAGS += -fPIC
endif


LIB_STAT_NAME = $(TA_NAME).a
LIB_SO_NAME = $(TA_NAME).so




OBJPATH = $(O)/obj
OBJFILES = $(SRCS:.c=.o)
OBJS = $(patsubst $(SRCPATH)/%,$(OBJPATH)/%,$(OBJFILES))

CFLAGS += -Wall -I../include -I$(TEEC_EXPORT)/include

CLIENTDEPS = $(OBJS)

ifeq ($(USE_LIBRARY),static)
CLIENTDEPS += $(LIB_STAT_PRODUCT)
LDADD += -l$(TA_NAME) -L$(LIB_O)
CFLAGS += -I$(LIB_PUBLIC)
endif

ifeq ($(USE_LIBRARY),dynamic)
CLIENTDEPS += $(LIB_SO_PRODUCT)
LDADD += -l$(TA_NAME) -L$(LIB_O)
CFLAGS += -I$(LIB_PUBLIC)
endif

LDADD += -lteec -L$(TEEC_EXPORT)/lib




PRODUCT := $(O)/$(TC_NAME)
LIB_STAT_PRODUCT := $(LIB_O)/lib$(LIB_STAT_NAME)
LIB_SO_PRODUCT := $(LIB_O)/lib$(LIB_SO_NAME)

ALL_PRODUCTS := $(PRODUCT)

ifeq ($(USE_LIBRARY),static)
ALL_PRODUCTS += $(LIB_STAT_PRODUCT)
endif

ifeq ($(USE_LIBRARY),dynamic)
ALL_PRODUCTS += $(LIB_SO_PRODUCT)
endif




.PHONY: clean all


all: $(ALL_PRODUCTS)


$(PRODUCT): $(CLIENTDEPS)
	$(CC) -o $@ $(OBJS) $(LDADD)

$(OBJPATH)/%.o: $(SRCPATH)/%.c $(OBJPATH)
	$(CC) -c -o $@ $< $(CFLAGS)


$(LIB_SO_PRODUCT): $(LIB_OBJS) $(LIB_O_PUBLIC)
	$(CC) -shared -o $(LIB_SO_PRODUCT) $(LIB_OBJS)


$(LIB_STAT_PRODUCT): $(LIB_OBJS) $(LIB_O_PUBLIC)
	$(AR) -rcs $(LIB_STAT_PRODUCT) $(LIB_OBJS)


$(LIB_OBJPATH)/%.o: $(LIB_SRCPATH)/%.c $(LIB_OBJPATH)
	$(CC) -c -o $@ $< $(LIB_CFLAGS)


$(LIB_O_PUBLIC):
	rm -rf $(LIB_O_PUBLIC)
	mkdir -p $(LIB_O_PUBLIC)
	cp -r $(LIB_PUBLIC)/*.h $(LIB_O_PUBLIC)


# paths

$(LIB_OBJPATH): $(LIB_O)
	mkdir -p $@

$(OBJPATH): $(O)
	mkdir -p $@

$(LIB_O):
	mkdir -p $@

$(O):
	mkdir -p $@



clean:
	rm -rf out

test:
	@echo $(SRCS)
	@echo $(OBJS)
