TARGET=jvm
CLASS=Hello.class

.PHONY: run valgrnid clean

run: $(TARGET) $(CLASS)
	./jvm Hello

valgrind: $(TARGET) $(CLASS)
	valgrind -s --leak-check=full ./jvm Hello

$(TARGET): jvm.c
	$(CC) -g -Wall -Werror -o $@ -std=c99 $^

clean:
	rm -f $(TARGET)

%.class: %.java
	javac $<
