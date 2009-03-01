
# The default goal is "all": this goal is to simply build
# the website in the htdocs/ subdirectory.
# In order to upload the site to sourceforge, use "make upload"
# Of course, only almkglor currently has the right to upload
# onto the website...

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
	touch htdocs/site-tag
	cd htdocs ; ./site


