# Automatically generated stuff
make/%.deps:: src/%/
	set -o pipefail; \
	for F in `find $< -name '*.cpp' | sort`; do \
	    ${CXX} -MM "$$F" -MT "$$(sed 's/src/obj/;s/\.cpp/.o/' <<< "$$F")" \
	        | sed 's/[^\]$$/& \\/;s/\([^:]\) \([^\]\)/\1 \\\n \2/g;s_/[^/]\+/../_/_g;s_/[^/]\+/../_/_g' \
	        | sort -bu; \
	    echo; \
	done > $@

include make/lib.make
include make/common.make
include make/login.make
include make/char.make
include make/map.make
include make/admin.make
