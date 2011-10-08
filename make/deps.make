# Automatically generated stuff
make/%.deps:: src/%/
	set -o pipefail; \
	for F in `find $< -name '*.cpp' -o -name precompiled.hpp | sort`; do \
	    ${CXX} -fpch-deps -MM "$$F" -MT "$$(sed '/precompiled\.hpp/!s/src/obj/;s/\.cpp/.o/;s/\.hpp/&.gch/' <<< "$$F")" \
	        | sed 's/[^\]$$/& \\/;s/\([^:]\) \([^\]\)/\1 \\\n \2/g;s_/[^/]\+/../_/_g;s_/[^/]\+/../_/_g' \
	        | sort -bu; \
	    echo; \
	done | sed '/: /!s/precompiled.hpp/&.gch/' > $@

include make/lib.make
include make/common.make
include make/login.make
include make/char.make
include make/map.make
include make/admin.make
