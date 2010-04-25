#! /bin/bash

HLBC_FILES=`ls cs/*.hl | sort`
COMPILER_FILES=`ls *.hl *.arc`

rm Makefile.am
cat > Makefile.am <<PREFIX

ARCF = arc
HL2B = \$(ARCF) bootstrap.arc
OUTFILE = /tmp/hl-tmp
HLVMA = ../src/hlvma

# %.hlbc : %.hl is non-portable
SUFFIXES = .hlbc .hl
.hl.hlbc :
	\$(HL2B) \$< --next && mv \$(OUTFILE) \$@

PREFIX

# construct $(OUTFILES)
echo -n "OUTFILES = " >> Makefile.am
for FILE in $HLBC_FILES; do
	TARGET=`echo $FILE | sed -e "s/.hl$/.hlbc/"`
	echo "\\" >> Makefile.am
	echo -n "  $TARGET" >> Makefile.am
done
echo >> Makefile.am
echo >> Makefile.am

# construct $(INFILES)
echo -n "INFILES = " >> Makefile.am
for FILE in $HLBC_FILES; do
	echo "\\" >> Makefile.am
	echo -n "  $FILE" >> Makefile.am
done
echo >> Makefile.am
echo >> Makefile.am

# construct $(COMPILERFILES)
echo -n "COMPILERFILES = " >> Makefile.am
for FILE in $COMPILER_FILES; do
	echo "\\" >> Makefile.am
	echo -n "  $FILE" >> Makefile.am
done
echo >> Makefile.am
echo >> Makefile.am

cat >> Makefile.am <<POSTFIX

.PHONY : run boot

run : boot
	\$(HLVMA) --bc \$(OUTFILES)

boot : \$(OUTFILES)

EXTRA_DIST = \$(OUTFILES) \$(INFILES) \$(COMPILERFILES) autogen.sh cs/info.txt
MAINTAINERCLEANFILES = \$(OUTFILES)

hlvmabootdir = \$(datadir)/hlvmaboot/
hlvmaboot_DATA = \$(OUTFILES)

POSTFIX

