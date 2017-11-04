all: otm-join

otm-join: main.cpp
	g++ -o otm-join main.cpp -O3 -Wall -Wextra

clean:
	- $(RM) otm-join

test:
	./test.sh
	$(RM) a1 b1 r1 r2

.PHONY: all, clean, test
