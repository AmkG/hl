
# The default goal is "all": this goal is to simply build
# the website in the htdocs/ subdirectory.
# In order to upload the site to sourceforge, use "make upload"
# Of course, only almkglor currently has the right to upload
# onto the website...
# Building this website requires markdown, as the sources
# require markdown.  Make sure it's named "markdown" and that
# it's on your path too.

.PHONY : all
.PHONY : upload


htdocs/%.html : %.txt collate
	./collate $< $@

empty:=
space:= $(empty) $(empty)

sources := $(wildcard *.txt)
to_htdocs := $(foreach source, $(sources), htdocs/$(source))
targets := $(subst .txt,.html,$(to_htdocs))

all : $(targets)

upload : htdocs/site-tag

htdocs/site-tag: htdocs/site $(targets)
	cd htdocs ; ./site
	touch htdocs/site-tag


