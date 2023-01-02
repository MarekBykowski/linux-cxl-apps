check_defined = \
    $(strip $(foreach 1,$1, \
        $(call __check_defined,$1,$(strip $(value 2)))))
__check_defined = \
    $(if $(value $1),, \
      $(error Undefined $1$(if $2, ($2))))
#eg. check if SRC provided from command line
#make SRC=file
#target: depend
#	$(call check_defined, SRC)

CC=gcc
CFLAGS=-g -Wall -I./include/linux
PKG=pkg-config --cflags --libs glib-2.0

SRC=cxl_app.c
OBJ=$(SRC:.c=.o)
APP=$(patsubst %.c,%,$(SRC))

#$@ - output file/target
#$< - takes only the first item on the dependencies list
#$^ - takes all the items on the dependencies list

all: $(APP)

source_files:
	@echo $(SRC)

%.o: %.c
	$(call check_defined, SRC)
	$(CC) -o $@ -c $< $(CFLAGS)

$(APP): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o *.a $(APP)

.PHONY: all clean
