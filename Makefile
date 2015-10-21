
all: clean build test

build:
	mkdir -p build; cd build; cmake ..; make

test: build
	cd build; make test

clean:
	rm -rf build