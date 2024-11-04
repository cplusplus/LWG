# The binaries that we want to build
PGMS := bin/lists bin/section_data bin/toc_diff bin/list_issues bin/set_status
CXXFLAGS := -std=c++20 -Wall -g -O2 -D_GLIBCXX_ASSERTIONS
CPPFLAGS := -MMD

# Running 'make debug' is equivalent to 'make DEBUG=1'
ifeq "$(MAKECMDGOALS)" "debug"
DEBUG := 1
endif

# Running 'make DEBUG=blah' rebuilds all binaries with debug settings
ifdef DEBUG
debug: pgms
CPPFLAGS += -DDEBUG_SUPPORT -D_GLIBCXX_DEBUG
CXXFLAGS += -O0
ifdef LOGGING
CPPFLAGS += -DDEBUG_LOGGING
endif
# Add UBSAN=1 to the command line to enable Undefined Behaviour Sanitizer
ifdef UBSAN
CXXFLAGS += -fsanitize=undefined
endif
# Add ASAN=1 to the command line to enable Address Sanitizer
ifdef ASAN
CXXFLAGS += -fsanitize=address
endif
# Make 'clean' a prerequisite of all object files, so everything is rebuilt:
src/*.o: clean
.DEFAULT_GOAL: all
endif

all: pgms

pgms: $(PGMS)

-include src/*.d

bin/lists: src/issues.o src/status.o src/sections.o src/mailing_info.o src/report_generator.o src/lists.o src/metadata.o

bin/section_data: src/section_data.o

bin/toc_diff: src/toc_diff.o

bin/list_issues: src/issues.o src/status.o src/sections.o src/list_issues.o src/metadata.o

bin/set_status: src/set_status.o src/status.o

$(PGMS):
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(PGMS) src/*.o src/*.d

# Remove everything.
# Caution: Regenerating meta-data/dates will take about 30 minutes.
distclean: clean
	rm -f meta-data/annex-f meta-data/networking-annex-f
	rm -f meta-data/dates
	rm -f meta-data/index.json meta-data/paper_titles.txt
	rm -f -r mailing

history: bin/lists
	bin/lists revision history

mailing:
	mkdir $@

lists: mailing bin/lists dates meta-data/paper_titles.txt
	bin/lists

define update
  if diff -N -u $(1) $(1).tmp ; then rm $(1).tmp ; else mv $(1).tmp $(1) ; fi ; touch $(1)
endef

WG21 := $(HOME)/src/cplusplus
DRAFT := $(WG21)/draft
NET := $(WG21)/networking-ts
filter-annex-f := grep -v '\\newlabel{\(fig\|tab\|eq\):' | sed 's/^.*\.aux://' | sed 's/^\\newlabel{\([^}]*\)}{{\([^}]*\)}.*/\1 \2/' | grep -v '^\\' | sed 's/\(Clause\|Annex\) //' | sort
# For memoir < 3.8
filter-annex-f-old-memoir := sed 's/\\newlabel{\([^}]*\)}.*TitleReference {\([^}]*\)}.*/\1 \2/' | sed 's/\\newlabel{\([^}]*\)}{{\(Clause\|Annex\) \([^}]*\)}.*/\1 \3/' | grep -v "aux:tab:" | grep -v "aux:fig:" | sed 's/\(.*\).aux://' | grep -v '^\\' | sort

filter-net-ts-annex-f := sed 's/\\newlabel{\([^}]*\)}.*TitleReference {\([^}]*\)}.*/\1 \2/' | sed 's/\\newlabel{\([^}]*\)}.*Clause \([^}]*\)}.*/\1 \2/' | sed 's/\\newlabel{\([^}]*\)}.*Annex \([^}]*\)}.*/\1 \2/' | grep -v "aux:tab:" | grep -v "aux:fig:" | sed 's/\(.*\).aux://' | grep -v '^\\' | sort

# Before running this, rebuild the C++ draft at the desired commit.
# That ensures the .aux files match the content of the relevant draft.
# We exclude back.aux, which contains some spurious \newlabels for indices
# and shouldn't have any real labels.
meta-data/annex-f: $(filter-out %/back.aux, $(wildcard $(DRAFT)/source/*.aux))
	test -d "$(DRAFT)"
	if grep -q TitleReference $^; then \
	    grep '^\\newlabel{' $^ | $(filter-annex-f-old-memoir); \
	else \
	    grep '^\\newlabel{' $^ | $(filter-annex-f); \
	fi > $@

net-ts-sources := $(wildcard $(NET)/src/*.aux)
# This target has no prerequisites, so it won't complain if the net-ts sources
# are not cloned and built in $(NET), and it won't be regenerated unless the
# meta-data/networking-annex-f file is removed first.
meta-data/networking-annex-f: # $(net-ts-sources)
	grep newlabel $(net-ts-sources) /dev/null | $(filter-net-ts-annex-f) > $@

meta-data/networking-section.data: meta-data/networking-annex-f bin/section_data
	if [ -s $< ]; then \
	  grep '^[^[:upper:][:digit:]]' $< | grep -v ISO/IEC | bin/section_data networking.ts >> $@.tmp ; \
	  $(call update,$@) ; \
	fi

meta-data/section.data: meta-data/annex-f meta-data/networking-section.data bin/section_data
	grep '^[^[:upper:][:digit:]]' $< | grep -v ISO/IEC | bin/section_data > $@.tmp
	cat meta-data/networking-section.data >> $@.tmp
	cat meta-data/tr1_section.data >> $@.tmp
	cat meta-data/filesystem-section.data >> $@.tmp
	cat meta-data/lfts-old-section.data >> $@.tmp
	cat meta-data/lfts-v3-section.data >> $@.tmp
	$(call update,$@)

.PHONY: all pgms clean distclean lists history

dates: meta-data/dates

optcmd = $(shell command -v $(1) || echo :)

# Generate file with issue number and unix timestamp of last change.
python := $(call optcmd,python)
meta-data/dates: xml/issue[0-9]*.xml bin/make_dates.py
	@echo "Refreshing 'Last modified' timestamps for issues..."
	@if [ "$(python)" = ":" ]; then \
	  for i in xml/issue[0-9]*.xml ; do \
	    n="$${i#xml/issue}" ; n="$${n%.xml}" ; \
	    grep -s -q "^$$n " $@ && test $$i -ot $@ && continue ; \
	    echo $$i >&2 ; \
	    git log -1 --pretty="format:$$n %ct%n" $$i ; \
	  done > $@.new; \
	  cat $@ $@.new | sort -n -r | sort -n -k 1 -u > $@.tmp; \
	  rm $@.new; \
	  $(call update,$@); \
	else \
	  git log --raw --no-show-signature --pretty=%ct | $(python) bin/make_dates.py > $@; \
	fi

new-papers:
	rm -f meta-data/index.json meta-data/paper_titles.txt
	$(MAKE) meta-data/paper_titles.txt

# If curl is not installed then create an empty meta-data/index.json
meta-data/index.json:
	$(call optcmd,curl) https://wg21.link/index.json > $@

# If python is not installed then create an empty meta-data/paper_titles.txt
meta-data/paper_titles.txt: | meta-data/index.json
	$(python) bin/make_paper_titles.py $| > $@

.PRECIOUS: meta-data/dates
.PRECIOUS: meta-data/paper_titles.txt

.PHONY: dates new-papers
