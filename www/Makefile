
.PHONY : all


htdocs/%.html : %.txt collate
	./collate $< $@

empty:=
space:= $(empty) $(empty)

sources := $(wildcard *.txt)
to_htdocs := $(foreach source, $(sources), htdocs/$(source))
targets := $(subst .txt,.html,$(to_htdocs))

htdocs/site-tag: htdocs/site $(targets)
	touch htdocs/site-tag
	cd htdocs ; ./site

all : htdocs/site-tag


