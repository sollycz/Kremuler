PANDOC  ?= pandoc
ENGINE  ?= xelatex

SRC := dokumentace.md
PDF := $(SRC:.md=.pdf)

.PHONY: all clean

all: $(PDF)

$(PDF): $(SRC) FIT_barevne_CMYK_CZ.pdf automat_hlavni_diagram.pdf automat_interpolateds_diagram.pdf blokove_schema.pdf
	$(PANDOC) $< -o $@ --pdf-engine=$(ENGINE) -V papersize=a4

clean:
	rm -f $(PDF)
