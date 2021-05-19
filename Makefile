APPNAME 	= kilo
SRCDIR		= src
SRCFILES	= $(APPNAME).c
OUTDIR		= bin
CFLAGS		= -Wall -Wextra -pedantic -std=c99
CC			= gcc
OBJDIR		= obj
OBJFILES	= $(APPNAME).o

all: clean kilo.o build

build: $(SRCDIR)/$(APPNAME).c
	if [ -d "$(OUTDIR)" ]; then $(CC) $(OBJDIR)/$(OBJFILES) -o $(OUTDIR)/$(APPNAME); else mkdir $(OUTDIR); $(CC) $(OBJDIR)/$(OBJFILES) -o $(OUTDIR)/$(APPNAME); fi 
	
kilo.o: $(SRCDIR)/$(SRCFILES)
	if [ -d "$(OBJDIR)" ]; then $(CC) -c $(SRCDIR)/$(SRCFILES) -o $(OBJDIR)/$(APPNAME).o; else mkdir $(OBJDIR); $(CC) -c $(SRCDIR)/$(SRCFILES) -o $(OBJDIR)/$(APPNAME).o; fi

clean:
	test -f $(OUTDIR)/$(APPNAME) && rm $(OUTDIR)/$(APPNAME) && echo "kilo binary removed" || echo "kilo binary does not exist"
	

# kilo.o.sc:
# 	$(SRCDIR)/$(SRCFILES) -o $(OBJDIR)/$(APPNAME).o;