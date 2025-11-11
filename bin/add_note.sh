#!/bin/bash
# Add notes to an issue.
# Usage: add_note.sh ISSUE [NOTE] [-]
# If NOTE is present, a <note>YYYY-MM-DD; NOTE</note> element will be added.
# If - is present, text will be read from stdin and added, enclosed in <p>.
# If neither is present, exit with an error (what are you trying to do?)

issue=${1:?}

xml=xml/issue$issue.xml

if ! [ -w "$xml" ]
then
  echo "$0: $xml: No such issue" >&2
  exit 1
fi

prefix=''

case "$2" in
  --prefix=*) prefix="${2:9} " ; shift 1 ;;
  --prefix) prefix="$3 " ; shift 2 ;;
esac

note=${2:?}
if [[ $note = - ]]
then
  note=''
else
  if [[ ${note: -1} != . ]]
  then
    note="${note}."
  fi
  date=$(date +%Y-%m-%d)
  note=$(printf "\\\n<note>%s%s; %s</note>" "$prefix" "$date" "$note")
  shift
fi

text=''
if [ "$2" = "-" ]
then
  if [ -t 0 ] # stdin is a terminal
  then
    echo "# Enter additional discussion (followed by EOF, i.e. Ctrl-D)."
    echo -n "# This will be enclosed in <p>"
    test -n "$note" && echo -n " after the new <note>"
    echo .
  fi
  while read line
  do
    text="$(printf "%s%s\\\n" "$text" "$line")"
  done
  if [ -n "$text" ]
  then
    if [ -n "$note" ]
    then
      note=$(printf "%s\\\n" "$note")
    fi
    text=$(printf "<p>\\\n%s</p>" "$text")
  fi
fi

sed -i '/<\/discussion>$/i\
'"${note}${text}
" $xml

# Check that the XML is still valid
bin/lint.sh
