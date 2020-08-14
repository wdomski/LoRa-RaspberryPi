# dragino lora testing
# Single lora testing app

CC=g++
CFLAGS=-c -Wall -pedantic
LIBS=-lwiringPi

all: lora_app loralib

app: lora_app

lib: loralib

lora_app: lora.o
	$(CC) lora.o  $(LIBS) -L/usr/lib/python3.5/ -o lora_app.exe

lora.o: lora.c
	$(CC) $(CFLAGS) -I/usr/include/python3.5 lora.c

loralib.o: lora.c
	$(CC) $(CFLAGS) -DPYTHONMODULE lora.c -I/usr/include/python3.5 -o loralib.o 

loralib: loralib.o
	$(CC) -shared loralib.o -L/usr/lib/python3.5/ $(LIBS) -o loralib.so

clean:
	rm -f *.o *.so *.exe

