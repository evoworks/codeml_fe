PRGS =  codeml_FE
CC = cc # cc, gcc, cl

CFLAGS = -O2
#CFLAGS = -O4
#CFLAGS = -fast

# for linux running gcc 3 on pentium and athlon
#CFLAGS = -march=athlon -mcpu=athlon -O4 -funroll-loops -fomit-frame-pointer -finline-functions
#CFLAGS = -march=pentiumpro -mcpu=pentiumpro -O4 -funroll-loops -fomit-frame-pointer -finline-functions 

LIBS = -lm # -lM

all : $(PRGS)

codeml_FE : codeml.o tools.o
	$(CC) $(CFLAGS) -o $@ codeml.o tools.o $(LIBS)

tools.o : paml.h tools.c
	$(CC) $(CFLAGS) -c tools.c
codeml.o : paml.h codeml.c treesub.c treespace.c
	$(CC) $(CFLAGS) -c codeml.c
