TARGET_NAME=netmon

monitor:
	$(CC) -Wall -Wextra -O2 -std=c11 -o build/$(TARGET_NAME) src/netmon.c

clean:
	rm -f build/$(TARGET_NAME)

install:
	cp build/$(TARGET_NAME) /usr/local/bin/$(TARGET_NAME)

uninstall:
	rm /usr/local/bin/$(TARGET_NAME)
