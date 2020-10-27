# dragino lora testing
# Single lora testing app

CC=g++
CFLAGS=-c -Wall -pedantic
LIBS=-lwiringPi
PYTHONVER=python3.7

all: lora_app loralib

app: lora_app

lib: loralib

lora_app: lora.o
	$(CC) lora.o  $(LIBS) -L/usr/lib/$(PYTHONVER)/ -o lora_app.exe

lora.o: lora.c
	$(CC) $(CFLAGS) -I/usr/include/$(PYTHONVER) lora.c

loralib.o: lora.c
	$(CC) $(CFLAGS) -DPYTHONMODULE lora.c -I/usr/include/$(PYTHONVER) -o loralib.o 

loralib: loralib.o
	$(CC) -shared loralib.o -L/usr/lib/$(PYTHONVER)/ $(LIBS) -o loralib.so

clean:
	rm -f *.o *.so *.exe

