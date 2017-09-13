HEADERS = busbsl.h buspirate.h debug.h serial.h i2c.h
OBJECTS = busbsl

default : busbsl

%.o: %.c $(HEADERS)
	gcc -c $< -o $@

program: $(OBJECTS)
	gcc $(OBECTS) -o $@

clean:
	-rm -f $(OBJECTS)
