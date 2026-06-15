#!/bin/sh

# Script to change an active issue to Ready and commit the change.
# Usage: make_ready.sh <ISSUE-NUM> [MEETING NAME]

die() {
  echo "$0: $*" >&2
  exit 1
}

issue=$1
xml=xml/issue$issue.xml
meeting=${2:+--prefix="$2"}

old_status=$(xpath -q -e 'string(/issue/@status)' $xml)
case "$old_status" in
  New|Open|LEWG|SG1|SG6) ;;
  *) die "Issue $issue does not have an active status: $old_status" ;;
esac
bin/set_status $issue Ready
bin/add_note.sh $issue $meeting "Change status: $old_status &rarr; Ready."
git commit -m "Move $issue to Ready" $xml
