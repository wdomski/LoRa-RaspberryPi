CC=gcc
CFLAGS=-fPIC -c -Wall -pedantic
LIBS=-lwiringPi
PYTHONVER=python3.13
PYTHONPATH=/usr/include/$(PYTHONVER)/

# Check if we are building for Docker
ifdef TARGET
	ifeq ($(TARGET),DOCKER)
		PYTHONPATH=/usr/local/include/$(PYTHONVER)/
$(info Building for docker)
	endif
endif

all: lora_app liblora

app: lora_app

lib: liblora

lora_app: lora.o
	$(CC) lora.o  $(LIBS) -L$(PYTHONPATH) -o lora_app.exe

lora.o: loralib.c
	$(CC) $(CFLAGS) -I$(PYTHONPATH) loralib.c -o lora.o

liblora.o: loralib.c
	$(CC) $(CFLAGS) -DPYTHONMODULE loralib.c -I$(PYTHONPATH) -o liblora.o 

liblora: liblora.o
	$(CC) -shared liblora.o -L/usr/lib/$(PYTHONVER)/ $(LIBS) -o liblora.so

clean:
	rm -f *.o *.so *.exe

