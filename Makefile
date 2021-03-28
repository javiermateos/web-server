##
# Compilador server
#
# @file Makefile
# @version 1.0

EDIR := .
SDIR := src
IDIR := include
ODIR := obj
SLDIR := srclib
LDIR := lib

NAME := server
C_NAMES := main.c http.c # Archivos en src
L_NAMES := picohttpparser.c tpool.c iniparser.c socket.c # Archivos en srclib

CC := gcc
CFLAGS := -g -I$(IDIR) -pedantic -Wall -Wextra
LFLAGS := -L$(LDIR) -liniparser -lpicohttpparser -ltpool -lpthread -lsocket

SFILES := c
OFILES := o
LFILES := a
SOURCES := $(foreach sname, $(C_NAMES), $(SDIR)/$(sname))
LSOURCES := $(foreach sname, $(L_NAMES), $(SLDIR)/$(sname))
OBJECTS := $(patsubst $(SDIR)/%.$(SFILES), $(ODIR)/%.$(OFILES), $(SOURCES))
LOBJECTS := $(patsubst $(SLDIR)/%.$(SFILES), $(ODIR)/%.$(OFILES), $(LSOURCES))

LIBRARIES := $(patsubst $(SLDIR)/%.$(SFILES), $(LDIR)/lib%.$(LFILES), $(LSOURCES))
EXE := $(EDIR)/$(NAME)

all: exe

exe: $(LIBRARIES) $(EXE)

$(EXE): $(OBJECTS) # Compilacion del ejecutable
	$(CC) $^ -o $@ $(LFLAGS)

$(LDIR)/lib%$(LFILES): $(ODIR)/%$(OFILES) # Compilacion de las librerias
	@mkdir -p lib
	ar rcs $@ $<

$(ODIR)/%$(OFILES): */%$(SFILES) # Compilacion de los objetos
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -fv $(EXE) $(DEPEND_FILES)
	rm -rfv $(ODIR) $(LDIR)

.PHONY: run
run:
	@echo "> Ejecutando servidor..."
	./server

.PHONY: runv
runv:
	@echo "> Ejecutando servidor con valgrind..."
	valgrind -s --leak-check=full --track-origins=yes --show-leak-kinds=all ./server

request:
	curl -v -H "Date: Fri, 31 Dec 1999 23:59:59 GMT" --http1.1 127.0.0.1:3490/index.html

# Deteccion de dependencias automatica
CFLAGS += -MMD
DEPEND_FILES := $(wildcard $(ODIR)/*.d)
-include $(DEPEND_FILES)

# end
