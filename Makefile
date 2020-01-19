TARGET=jvm
CLASS=Hello.class

.PHONY: run clean

run: $(TARGET) $(CLASS)
	./jvm Hello

$(TARGET): jvm.c
	$(CC) -Wall -Werror -o $@ -std=c99 $^

clean:
	rm -f $(TARGET)

%.class: %.java
	javac $<
