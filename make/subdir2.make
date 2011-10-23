.SUFFIXES:
DIR := $(notdir ${CURDIR})
default:
	${MAKE} -C ../.. tmwa-${DIR}
clean:
	rm -r ../../obj/${DIR}/
%::
	${MAKE} -C ../.. obj/${DIR}/$@
