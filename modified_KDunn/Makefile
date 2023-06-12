PRGS =  codeml_FEmod
CC = cc -fast  -xarch=native64 # cc, gcc, cl

#CFLAGS = -O2
#CFLAGS = -O4
CFLAGS = -xvector=simd

# for linux running gcc 3 on pentium and athlon
#CFLAGS = -march=athlon -mcpu=athlon -O4 -funroll-loops -fomit-frame-pointer -finline-functions
#CFLAGS = -march=pentiumpro -mcpu=pentiumpro -O4 -funroll-loops -fomit-frame-pointer -finline-functions 

LIBS = -lm # -lM

all : $(PRGS)

codeml_FEmod : codeml_FE.o tools.o
	$(CC) $(CFLAGS) -o $@ codeml_FE.o tools.o $(LIBS)

tools.o : paml.h tools.c
	$(CC) $(CFLAGS) -c tools.c
codeml_FE.o : paml.h codeml_FE.c treesub.c treespace.c
	$(CC) $(CFLAGS) -c codeml_FE.c
