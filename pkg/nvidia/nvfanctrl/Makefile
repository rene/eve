SOURCES=main.go
BINARY=nvfanctrl

.PHONY: clean

all: $(BINARY)

$(BINARY):$(SOURCES)
	go build -o $@ $<

clean:
	rm -f $(BINARY)
