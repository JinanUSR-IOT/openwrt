
all: libusrdtu.so dtu_demo

dtu_demo: example/dtu_demo.c
	$(CC) $(CFLAGS) $(LDFLAGS) example/dtu_demo.c -o dtu_demo -I./ -L./ -lusrdtu -Werror
	$(STRIP) dtu_demo
clean:
	rm *.o dtu_demo -rf
install:
	@echo "none"
