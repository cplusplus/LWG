#!/bin/bash

usage() { echo "Usage: $0 <issue number>" ; exit $1 ; }

test $# -eq 1 || usage 1 >&2
test "$1" == -h -o "$1" == --help && usage

(( "$1" )) 2>/dev/null && test -f xml/issue$1.xml || {
  echo "$0: Invalid issue number: $1"
  exit 1
} >&2

issue=xml/issue$1.xml

date=$(date +%Y-%m-%d)

sed -i -e '/<\/discussion>/i\
<superseded>' -e '/<\/discussion>/,/^<resolution>/d' -e '/^<\/resolution>/i\
</superseded>\
\
<note>'$date'; <<AUTHOR>> provides new wording</note>\
\
</discussion>\
\
<resolution>' $issue
