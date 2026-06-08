#!/bin/bash
# Generate the "Issues to be moved" document for plenary polls

# TODO it would be useful to be able to select which statuses to include.
# Currently the only option is to include "Tentatively NAD" issues or not.
# Would be useful to create a pre-meeting document of "Ready" issues,
# then another document of "Immediate" issues during the meeting.
# Maybe take a JSON file as input like:
# {"Ready": "Intro text for <b>Ready</b>", "Immediate": "Intro text ..."}

usage()
{
  echo "Usage: $0 [OPTIONS] <MEETING NAME> <DOCUMENT NUMBER> [DATE] [AUTHOR NAME] [AUTHOR EMAIL]"
  echo
  echo "The meeting name and document number must be provided."
  echo "Date and author will use sensible defaults."
  echo
  echo "Options:"
  echo "--help    Print this usage and exit."
  echo "--no-toc  Do not include each issue in table of contents"
  echo "--intros  Read a block of HTML text from standard input"
  echo "          as an introductory paragraph for each status."
  echo "--nad     Include Tentatively NAD issues in the report."
  echo "--ts      Include TS issues in the report."
  echo
  echo "Example: $0 'Virtual Plenary, Nov. 2020' 'P2236R0'"
}

die()
{
  exec >&2
  echo "$0: $*"
  echo
  usage
  exit 1
}

intros=no
nad=no
ts=no
toc=yes
while [[ "$1" == --* ]]
do
  case "$1" in
    --help) usage ; exit ;;
    --no-toc) toc=no ;;
    --intros) intros=yes ;;
    --nad) nad=yes ;;
    --ts) ts=yes ;;
    *) die "Unknown option: $1" ;;
  esac
  shift
done

if [ $# -lt 2 ]
then
  die "Missing arguments"
fi

meeting="$1"
docno="$2"
date=$(date +%F ${3:+-d "$3"}) # YYYY-MM-DD (either today, or $3 if given)
author="${4:-Jonathan Wakely}"
email="${5:-lwgchair@gmail.com}"

# declare an indexed array
declare -a statuses
# declare associative arrays
declare -A issues anchors

# Always include these statuses:
statuses=("Immediate" "Ready" "Tentatively Ready")

# Include Tentatively NAD issues if --nad option was given:
if [[ $nad == yes ]]
then
  statuses=("${statuses[@]}" "Tentatively NAD")
fi

# Find list of issue numbers for each status
for st in "${statuses[@]}"
do
  issues[$st]=$(bin/list_issues "$st" | sort -n)
  anchors[$st]=$(echo $st | LANG=C tr 'A-Z ' 'a-z_')
done

dump_issues()
{
  # for each status
  for st in "${statuses[@]}"
  do
    count=$(echo "${issues[$st]}" | wc -w)
    echo "Processing $count $st issues ..." >&2
    test $count -ne 0 || continue
    printf '<h2 id="%s">%s Issues</h2>\n\n' "${anchors[$st]}" "$st"
    if [ "$intros" = "yes" ]
    then
      echo "* Enter HTML text, followed by EOF (Ctrl-D on UNIX):" >&2
      echo '<p>'
      cat
      echo '</p>'
    fi
    echo "${issues[$st]}" | while read i
    do
      if [[ $ts = no ]]
      then
        match=$(grep -o '^<title>\[[^]]\+\.ts\.[^]]\+]' "xml/issue$i.xml")
        if [[ $match ]]
        then
          echo "Skipping $i (${match:7})" >&2
          continue
        fi
      fi
      echo "$i" >&2
      # extract relevant parts of the per-issue HTML page
      html=mailing/issue$i.html
      sed \
        -e '1,/^<body>/d' \
        -e '/^<p><em>This page is a snapshot/d' \
        -e '/^<p><b>View all issues with/d' \
        -e 's;href="lwg-;href="https://cplusplus.github.io/LWG/lwg-;g' \
        -e '/^<\/body>/,$d' \
        $html
    done
  done
}

dump_toc()
{
  echo '<ul>'
  # for each status
  for st in "${statuses[@]}"
  do
    count=$(echo "${issues[$st]}" | wc -w)
    test $count -ne 0 || continue
    echo "Summarizing $count $st issues ..." >&2
    printf '<li><a href="#%s">%s Issues</a>' "${anchors[$st]}" "$st"
    if [[ $toc = yes ]]
    then
      printf '\n<ul>\n'

      echo "${issues[$st]}" | while read i
      do
        if [[ $ts = no ]]
        then
          match=$(grep -o '^<title>\[[^]]\+\.ts\.[^]]\+]' "xml/issue$i.xml")
          if [[ $match ]]
          then
            continue
          fi
        fi
        printf '<li><a href="#%d">%d</a>.\n' $i $i
        # Extracting just the <title> from the HTML is tricky, due to some
        # multiline titles like issue2642.html so extract from the XML instead.
        xpath -q -e '/issue/title/node()' xml/issue$i.xml \
          | sed -E -e 's;`([^`]+)`;<code>\1</code>;g' # replace backticks
        printf '\n</li>\n'
      done
      echo '</ul>'
    fi
    echo '</li>'
  done
  echo '</ul>'
}

cat <<EOT
<!DOCTYPE HTML>
<html lang="en">
<head>
<meta charset="utf-8">
<title>C++ Standard Library Issues to be moved in $meeting</title>
<style>
  p {text-align:justify}
  li {text-align:justify}
  pre code.backtick::before { content: "\`" }
  pre code.backtick::after { content: "\`" }
  details.superseded { border-left: solid grey 2px; }
  details.superseded > summary { font-weight: bold; cursor: pointer; padding: 0px 5px;}
  details.superseded > summary::after { font-weight: normal; color: grey; }
  details.superseded[open] > summary::after { content: " <click to hide>"; }
  details.superseded:not([open]) > summary::after { content: " <click to show>"; }
  blockquote.note, details.superseded[open]::details-content
  {
    background-color: #E0E0E0;
    padding-left: 15px;
    padding-right: 15px;
    padding-top: 1px;
    padding-bottom: 1px;
  }
  ins {background-color:#A0FFA0}
  del {background-color:#FFA0A0}
  table.issues-index { border: 1px solid; border-collapse: collapse; }
  table.issues-index th { text-align: center; padding: 4px; border: 1px solid; }
  table.issues-index td { padding: 4px; border: 1px solid; }
  table.issues-index td:nth-child(1) { text-align: right; }
  table.issues-index td:nth-child(2) { text-align: left; }
  table.issues-index td:nth-child(3) { text-align: left; }
  table.issues-index td:nth-child(4) { text-align: left; }
  table.issues-index td:nth-child(5) { text-align: center; }
  table.issues-index td:nth-child(6) { text-align: center; }
  table.issues-index td:nth-child(7) { text-align: left; }
  table.issues-index td:nth-child(5) span.no-pr { color: red; }
  @media (prefers-color-scheme: dark) {
     html {
        color: #ddd;
        background-color: black;
     }
     ins {
        background-color: #225522
     }
     del {
        background-color: #662222
     }
     a {
        color: #6af
     }
     a:visited {
        color: #6af
     }
     blockquote.note, details.superseded[open]::details-content {
        background-color: rgba(255, 255, 255, .10)
     }
  }
</style>
</head>
<body>
<h1>C++ Standard Library Issues to be moved in $meeting</h1>
<table>
<tr>
<td align="left">Doc. no.</td>
<td align="left">$docno</td>
</tr>
<tr>
<td align="left">Date:</td>
<td align="left"><p>$date</p>
</td>
</tr>
<tr>
<td align="left">Audience:</td>
<td align="left">WG21</td>
</tr>
<tr>
<td align="left">Reply to:</td>
<td align="left">$author &lt;<a href="mailto:$email">$email</a>&gt;</td>
</tr>
</table>
EOT

dump_toc
dump_issues

echo '</body>'
echo '</html>'
