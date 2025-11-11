#!/bin/sh

# Sweep all issues from Voting -> WP

if [ $# -ne 2 ]
then
  echo "Usage: $0 '<Meeting Name> YYYY-MM-DD'" >&2
  exit 1
fi

today=$(date +%F)
meeting=$1
date=$2

# Change this to "Immediate" to sweep issues after ballot resolution meeting.
sweep_status=Voting

bin/list_issues "$sweep_status" | while read inum
do
  bin/set_status $inum WP
  sed -i "s/^<note>$today /<note>$meeting $date; /" xml/issue$inum.xml
done
