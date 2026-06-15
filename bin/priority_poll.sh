#!/bin/bash
# Send email for LWG issue prioritization poll.
# To batch send use a command like:
# git pull && do bin/priority_poll 3858-3864

# Email will be sent using 'mutt' if present, or 'mailx' otherwise.
# The sender address can be set by the first one found of: --from option;
# the $EMAIL environment variable; or git config user.email and user.name.

die() {
  echo "$0: $*" >&2
  exit 1
}

To="C++ Library Working Group <lib@lists.isocpp.org>"
From=${EMAIL}

usage()
{
  echo "Usage: $0 [--help] [--to=<address>] [--from=<address>] ISSUE-NUMBER..."
  echo
  echo "ISSUE-NUMBER can be a single number or a range, e.g. 4321 or 4321-4325"
  exit $1
}

while [[ "$1" == -* ]]
do
  case "$1" in
    --help | -h) usage ;;
    --to=*) To="${1#--to=}" ;;
    --to) To="$2" ; shift ;;
    --from=*) From="${1#--from=}" ;;
    --from) From="$2" ; shift ;;
    *) die "Invalid argument: $1" ;;
  esac
  shift
done

if [ -z "$From" ]; then
  if From="$(git config user.email)"; then
    if name="$(git config user.name)"; then
      From="$name <$From>"
      unset name
    fi
  fi
fi

[ -n "$From" ] || die 'address not set, use --from or $EMAIL'

issues=''

# Get list of issues
for i
do
  if [[ $i =~ [[:digit:]]+-[[:digit:]]+ ]]
  then
    range=$(seq ${i/-/ })
    [ "$range" == '' ] && die "$0: Invalid argument: $i"
    issues="$issues $range"
  elif [[ $i =~ [[:digit:]]+ ]]
  then
    issues="$issues $i"
  fi
done

[ "$issues" == '' ] && usage 1 >&2

# Make unique, sorted list of issue numbers
issues=$(for i in $issues; do echo $i ; done | sort -u -n)

if [ -n "$DO_NOT_SEND" ] # define in the environment to test the script
then
  echo 'Not sending email due to $DO_NOT_SEND in the environment' >&2
fi

send_email()
{
issue=$1

if ! xml=$(printf "xml/issue%04d.xml" $issue) || [ ! -f "$xml" ]
then
  die "No such issue: $issue"
fi

if [ "$(xpath -q -e 'string(/issue/@status)' $xml)" != New ]
then
  die "Status is not \"New\": $issue"
fi

if ! prio=$(xpath -q -e '/issue/priority/text()' $xml) || [ -z "$prio" ]
then
  die "Issue has no priority element: $issue"
elif [ "$prio" != 99 ]
then
  die "Priority already set: $issue [priority=$prio]"
fi

# Convert HTML to plain text (specifically, replace entities).
# html2txt() { w3m -dump -T text/html -cols 1000 ; }
# title=$(xpath -q -s '' -e '/issue/title//text()' $xml | html2txt)

# Use xpath to select text content of <title> element, with entities replaced.
title=$(xpath -q -e 'normalize-space(/issue/title)' $xml)

  if [ -n "$DO_NOT_SEND" ]
  then
    echo "Not Sent: Issue $issue: $title"
    return 1
  fi

draft=`mktemp /tmp/draft.prio.mail.XXXXXX` || exit

subject="Issue $issue: $title"

if [ -x "$(command -v mutt)" ]; then
  use_mutt=yes
  cat > $draft <<EOT
From: $From
Subject: $subject
EOT
else
  use_mutt=no
fi

cat >> $draft <<EOT
It's bug prioritization time!
Thanks to everyone who participates.

https://cplusplus.github.io/LWG/issue$issue

Priority levels:
P0 - Has a proposed resolution and that resolution is clearly correct. Requires unanimity among the people doing prioritization. Move the issue to "Tentatively Ready" or "Ready" (whichever is appropriate for the group doing the review); we don't want to spend any more time discussing this issue. [Shortened: Approve, and move on.]
P1 - Showstopper bug; don't ship a standard w/o resolving this.
P2 - Important bug.
P3 - Normal bug.
P4 - Less important bug.
NAD - Not A Defect. If this has consensus, move the issue to "Tentatively NAD" and close as NAD after the next plenary.

The most common priority should be P3.

Please add your comments to this thread. Comments which are insightful or summarize the group's consensus might be added to the issue, without attribution, possibly paraphrased. If you would prefer not to have your comments shared in public, please make that clear.

EOT

if [ "$use_mutt" = yes ]; then
  mutt -H $draft -- "$To" </dev/null || exit 1
else
  mailx -s "$subject" -S "from=$From" "$To" < $draft || exit 1
fi

echo "Sent: Issue $issue: $title"

rm $draft
}

delay=0

# Send the emails
for i in $issues
do
  sleep $delay
  send_email $i && delay=1
done
