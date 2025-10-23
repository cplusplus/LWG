#!/bin/bash
# Set the <priority> of an issue, for use after reflector prioritization polls.

if [[ $1 == --help ]]
then
cat <<EOT
Usage: $0 ISSUE PRIORITY
    ISSUE    - the number of the issue to change.
    PRIORITY - a single digit from 1 to 4.
Set priority of ISSUE to PRIORITY and add a <note>.

Usage: $0 ISSUE 0 [VOTES] [--commit]
    ISSUE    - the number of the issue to change.
    VOTES    - number of P0 votes to record in the <note> (default: five).
When PRIORITY is 0 change the <status> to Tentatively Ready instead.
If --commit is given, do 'git commit' as well.
EOT
  exit
fi

issue=${1:?}
priority=${2:?}
do_commit=no
date=$(date +%Y-%m-%d)

xml=xml/issue$issue.xml

die()
{
  echo "Issue $issue: $*" >&2
  exit 1
}

grep -q -E '^<issue .* status="(New|Open)"' $xml || die "Status not New or Open"

if [[ $priority = 0 ]]
then

  votes="$3"
  if [ -n "$votes" ] && [ "${votes:0:1}" != "-" ]
  then
    shift # $3 looks like a valid string for number of votes
  else
    votes=five # minimum number of P0 votes needed for Tentatively Ready
  fi

  sed -i -e '/^<issue /s/ status=".*">$/ status="Tentatively Ready">/' $xml
  bin/add_note.sh $issue "Reflector poll" - <<< "Set status to Tentatively Ready after $votes votes in favour during reflector poll."

  do_commit=$([[ $3 == --commit ]] && echo Ready)

elif [[ $priority = NAD ]]
then

  sed -i -e '/^<issue /s/ status=".*">$/ status="Tentatively NAD">/' $xml
  bin/add_note.sh $issue "Reflector poll. Status &rarr; Tentatively NAD "

  do_commit=$([[ $3 == --commit ]] && echo NAD)

else

  grep -q '<priority>99</priority>' $xml || die "Priority already set"

  sed -i -e '/^<priority>99</s/<priority>99</<priority>'$priority'</' $xml
  bin/add_note.sh $issue "Reflector poll" - <<< "Set priority to $priority after reflector poll."

fi

# A final "-" argument means read some more text from stdin:
if [ "${!#}" = "-" ]
then
  bin/add_note.sh $issue -
fi

if [[ $do_commit != no ]]
then
  git commit -m "Set $issue to Tentatively $do_commit" $xml
fi
