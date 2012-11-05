DEMO=dejong
CC=gcc

INCS= -I ./include -I ../corange/include
LIBS= -L ./lib

CFLAGS= $(INCS) -std=gnu99 -Wall -Werror -Wno-unused -O3 -g -march=corei7 -mtune=corei7 -ffast-math
C_FILES= $(wildcard src/*.c)
OBJ_FILES= $(addprefix obj/,$(notdir $(C_FILES:.c=.o)))

PLATFORM = $(shell uname)

ifeq ($(findstring Linux,$(PLATFORM)),Linux)
	OUT=$(DEMO)
	LFLAGS= $(LIBS) -lcorange -lGL -lSDLmain -lSDL
	CORANGE= ./lib/libcorange.a
	CORANGE_LIB= ../corange/libcorange.a
endif

ifeq ($(findstring Darwin,$(PLATFORM)),Darwin)
	OUT=$(DEMO)
	LFLAGS= $(LIBS) -lcorange -lGL -lSDLmain -lSDL
	CORANGE= ./lib/libcorange.a
	CORANGE_LIB= ../corange/libcorange.a
endif

ifeq ($(findstring MINGW,$(PLATFORM)),MINGW)
	OUT=$(DEMO).exe
	LFLAGS= $(LIBS) -L../corange/lib ../corange/corange.res -lcorange -lmingw32 -lSDLmain -lSDL -lopengl32 -pthread
	CORANGE= ./lib/corange.lib
	CORANGE_LIB= ../corange/corange.lib
endif

$(OUT): $(OBJ_FILES) $(CORANGE)
	$(CC) -g $(OBJ_FILES) $(LFLAGS) -o $@

obj/%.o: src/%.c | obj
	$(CC) $< -c $(CFLAGS) -o $@ -Wa,-aln=$@.s -masm=intel

obj:
	mkdir obj
	
$(CORANGE): $(CORANGE_LIB) | lib
	cp $(CORANGE_LIB) $(CORANGE)
	
lib:
	mkdir lib
	
clean:
	rm $(OUT) $(CORANGE) $(OBJ_FILES)

