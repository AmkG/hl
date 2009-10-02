OTHER_INCLUDES =
if BOOST_PATH_SPECIFIED
OTHER_INCLUDES += -I@BOOST_PATH@
endif

SUBDIRS = src test hl2b

test_vm:
	perl test/dotest.pl 'src/hlvma --bc ' test/executors/tests
