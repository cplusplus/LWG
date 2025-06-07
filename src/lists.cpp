// This program reads all the issues in the issues directory passed as the first command line argument.
// If all documents are successfully parsed, it will generate the standard LWG Issues List documents
// for an ISO SC22 WG21 mailing.

// Based on code originally donated by Howard Hinnant.
// Since modified by Alisdair Meredith and modernised by Jonathan Wakely.

// Note that this program requires a C++17 compiler supporting std::filesystem.

// TODO
// .  Grouping of issues in "Clause X" by TR/TS
// .  Sort the Revision comments in the same order as the 'Status' reports, rather than alphabetically
// .  Lots of tidy and cleanup after merging the revision-generating tool
// .  Refactor more common text
// .  Split 'format' function and usage to that the issues vector can pass by const-ref in the common cases
// .  Document the purpose amd contract on each function
// Waiting on external fix for preserving file-dates
// .  sort-by-last-modified-date should offer some filter or separation to see only the issues modified since the last meeting

// Missing standard facilities that we work around
// . (none at present)

// Missing standard library facilities that would probably not change this program
// . XML parser

// standard headers
#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <filesystem>
namespace fs = std::filesystem;

// solution specific headers
#include "html_utils.h"
#include "issues.h"
#include "mailing_info.h"
#include "report_generator.h"
#include "sections.h"


auto read_file_into_string(fs::path const & filename) -> std::string {
   // read a text file completely into memory, and return its contents as
   // a 'string' for further manipulation.

   std::ifstream infile{filename};
   if (!infile.is_open()) {
      throw std::runtime_error{"Unable to open file " + filename.string()};
   }

   std::istreambuf_iterator<char> first{infile}, last{};
   return std::string {first, last};
}

// Issue-list specific functionality for the rest of this file
// ===========================================================

auto is_issue_xml_file(fs::directory_entry const & e) {
   if (e.is_regular_file()) {
      fs::path f = e.path().filename();
      return f.string().starts_with("issue") && f.extension() == ".xml";
   }
   return false;
}

auto read_issues(fs::path const & issues_path, lwg::metadata & meta) -> std::vector<lwg::issue> {
   // Open the specified directory, 'issues_path', and iterate all the '.xml' files
   // it contains, parsing each such file as an LWG issue document.  Return the set
   // of issues as a vector.

   std::vector<lwg::issue> issues{};
   for (auto ent : fs::directory_iterator(issues_path)) {
      if (is_issue_xml_file(ent)) {
         fs::path const issue_file = ent.path();
         issues.emplace_back(parse_issue_from_file(read_file_into_string(issue_file), issue_file.string(), meta));
      }
   }

   return issues;
}


auto read_issues_from_toc(std::string const & s) -> std::vector<std::tuple<int, std::string> > {
   // parse all issues from the specified stream, 'is'.
   // Throws 'runtime_error' if *any* parse step fails
   //
   // We assume 'is' refers to a "toc" html document, for either the current or a previous issues list.
   // The TOC file consists of a sequence of HTML <tr> elements - each element is one issue/row in the table
   //    First we read the whole stream into a single 'string'
   //    Then we search the string for the first <tr> marker
   //       The first row is the title row and does not contain an issue.
   //       If we cannot find the first row, we flag an error and exit
   //    Next we loop through the string, searching for <tr> markers to indicate the start of each issue
   //       We parse the issue number and status from each row, and append a record to the result vector
   //       If any parse fails, throw a runtime_error
   //    If debugging, display the results to 'cout'

   // Skip the title row
   auto i = s.find("<tr>");
   if (std::string::npos == i) {
      throw std::runtime_error{"Unable to find the first (title) row"};
   }

   auto extract_link_text = [&] (std::string desc) mutable {
      i = s.find("</a>", i);
      auto j = s.rfind('>', i);
      if (j == std::string::npos) {
         throw std::runtime_error{"unable to parse issue "+desc+": can't find beginning bracket"};
      }
      return s.substr(j+1, i-j-1);
   };

   // Read all issues in table
   std::vector<std::tuple<int, std::string> > issues;
   for(;;) {
      i = s.find("<tr>", i+4);
      if (i == std::string::npos) {
         break;
      }
      int num = std::stoi(extract_link_text("number"));
      i += 4;
      std::string status = extract_link_text("status");
      if (status == "(i)") {
        i += 4;
        status = extract_link_text("status");
      }

      issues.emplace_back(num, status);
   }

   return issues;
}

namespace {
   // A struct the captures the context of an error.
   struct Context
   {
      Context(const std::string& txt, std::size_t pos) : text(txt), pos(pos)
      { }

      std::string_view text;
      std::size_t pos;

      // Display the context in a convenient form.
      friend std::ostream& operator<<(std::ostream& os, const Context& ctx)
      {
         auto text = ctx.text;
         auto pos = ctx.pos;
         auto bol = text.rfind('\n', pos);
         if (bol != text.npos)
         {
            text.remove_prefix(bol + 1);
            pos -= bol + 1;
         }

         std::string_view prefix, suffix;

         // We want the context to fit within this field width.
         const size_t maxwidth = 80;

         if (pos > (maxwidth - 10))
         {
            // Need to trim the start of the context.
            size_t len = maxwidth / 2;
            if (pos > len)
            {
               // This puts the error at the centre of the field.
               auto drop = (pos / len - 1) * len + (pos % len);
               text.remove_prefix(drop);
               pos -= drop;
               prefix = "[...] ";
            }
         }

         auto eol = text.find('\n', pos);
         if (eol > 80)
         {
            // Need to trim the end of the context.
            suffix = " [...]";
            eol = 72;
         }
         os << '\n' << prefix << text.substr(0, eol) << suffix
            << '\n' << std::string(pos + prefix.size(), ' ') << "^\n";
         return os;
      }
   };
}

// ============================================================================================================

namespace
{
   std::map<std::string_view, std::pair<std::string_view, std::string_view>, std::less<>> substitutions {
      { "discussion", {"<p><b>Discussion:</b></p>", ""} },
         { "resolution", {} },
         { "rationale", {"<p><b>Rationale:</b></p>", ""} },
         { "duplicate", {} },
         { "note", { "<p><i>[", "]</i></p>\n"} },
         { "superseded",
            { "<p><strong>Previous resolution [SUPERSEDED]:</strong></p>\n"
               "<blockquote class=\"note\">\n",
               "</blockquote>" } },
   };
}

// The title of the specified paper, formatted as an HTML title="..." attribute.
std::string paper_title_attr(std::string paper_number, lwg::metadata& meta) {
   auto title = meta.paper_titles[paper_number];
   if (!title.empty())
   {
      title = lwg::replace_reserved_char(std::move(title), '&', "&amp;");
      title = lwg::replace_reserved_char(std::move(title), '"', "&quot;");
      title = " title=\"" + title + "\"";
   }
   return title;
}

void format_issue_as_html(lwg::issue & is,
                          std::vector<lwg::issue>::iterator first_issue,
                          std::vector<lwg::issue>::iterator last_issue,
                          lwg::metadata & meta) {

   auto& section_db = meta.section_db;
   std::vector<std::string> tag_stack; // stack of open XML tags as we parse

   // Used by fix_tags to report errors.
   auto fail = [&is] (std::string_view reason, const Context& ctx) {
      std::ostringstream err;
      err << reason << " in issue " << is.num << ":\n";
      err << ctx;
      throw std::runtime_error{err.str()};
   };

   // Used by fix_tags to report errors for XML that is not well-formed.
   auto fail_mismatched_tag = [&] (std::string_view closing_tag, const Context& ctx) {
      std::ostringstream err;
      err << "Mismatched tags in issue " << is.num << ".  ";
      if (tag_stack.empty()) {
         err << "Had no open tag.";
      }
      else {
         err << "Open tag was " << tag_stack.back() << '.';
      }
      err << "  Closing tag was " << closing_tag << ":\n";
      err << ctx;
      throw std::runtime_error{err.str()};
   };

   // Return the content of a quoted attribute, e.g. ref="[stable.name]"
   auto get_attribute_value = [&fail](std::string elem_name, std::string_view data, const Context& ctx) {
      auto k = data.find('\"');
      if (k != data.npos) {
         ++k;
         auto l = data.find('\"', k);
         if (l != data.npos) {
            return data.substr(k, l - k);
         }
      }
      fail("Missing '\"' in <" + elem_name + '>', ctx);

      // Can't put [[noreturn]] on a lambda until C++23,
      // so we get warnings about a missing return here.
#if __cpp_lib_unreachable
      std::unreachable();
#else
      return std::string_view{};
#endif
   };

   // Check whether the character following a '<' is well-formed.
   // It has to be an opening or closing tag like <foo> or </foo>
   // or the start of a comment like <!--.
   auto start_element_or_comment = [](char c) {
      return std::isalpha(c) || c == '/' || c == '!';
   };


   // Reformat the issue text for the specified 'is' as valid HTML, replacing all the issue-list
   // specific XML markup as appropriate:
   //   tag             replacement
   //   ---             -----------
   //   iref            internal reference to another issue, replace with an anchor tag to that issue
   //   sref            section-tag reference, replace with formatted tag and section-number
   //   paper           paper reference (PXXXX, PXXXXRX, DXXXX, DXXXXRX, NXXXX); replace with a wg21.link to the paper
   //   discussion      <p><b>Discussion:</b></p>CONTENTS
   //   resolution      <p><b>Proposed resolution:</b></p>CONTENTS
   //   rationale       <p><b>Rationale:</b></p>CONTENTS
   //   duplicate       tags are erased, leaving just CONTENTS
   //   superseded       <p><strong>Previous resolution [SUPERSEDED]:</strong></p> and enclose in a note.
   //   note            <p><i>[NOTE CONTENTS]</i></p>
   //   !--             comments are simply erased
   //
   // In addition, as duplicate issues are discovered, the duplicates are marked up
   // in the supplied range [first_issue,last_issue).  Similarly, if an unexpected
   // (unknown) section is discovered, it will be inserted into the supplied
   // section index, 'section_db'.
   //
   // The behavior is undefined unless the issues in the supplied vector range are sorted by issue-number.
   //
   // Essentially, this function is a tiny xml-parser driven by a stack of open tags, that pops as tags
   // are closed.

   auto fix_tags = [&](std::string &s) {

      // Loop over the input looking for '< characters.
      // Cannot rewrite as range-based for-loop as the string 's' is modified within the loop.
      for (auto i = s.find('<'); i < s.size(); i = s.find('<', i+1)) {
         Context context{s, i};

         if (!start_element_or_comment(s[i+1])) {
            fail("Unescaped '<'", context);
         }

         auto j = s.find('>', i);
         if (j == std::string::npos) {
            fail("Missing '>'", context);
         }

         std::string tag;
         {
            std::istringstream iword{s.substr(i+1, j-i-1)};
            iword >> tag;
         }

         if (tag.empty()) {
            fail("Unexpected <>", context);
         }

         if (tag[0] == '/') { // closing tag
             tag.erase(tag.begin());
             if (tag == "issue"  or  tag == "revision") {
                s.erase(i, j-i + 1);
                --i;
                break;;
             }

             if (tag_stack.empty()  or  tag != tag_stack.back()) {
                fail_mismatched_tag(tag, context);
             }

             tag_stack.pop_back();
             if (auto r = substitutions.find(tag); r != substitutions.end()) {
                 s.replace(i, j-i + 1, r->second.second);
                 i += r->second.second.size() - 1;
             }
             else {
                 i = j;
             }

             continue;
         }

         if (s[j-1] == '/') { // self-closing tag: sref, iref, paper

            std::string_view attrs(s);
            attrs.remove_prefix(i);

            // format section references
            if (tag == "sref") {
               lwg::section_tag tag;
               tag.prefix = is.doc_prefix;
               auto section_name = get_attribute_value("sref", attrs, context);
               tag.name = section_name.substr(1, section_name.size() - 2);

               // heuristic: if the name is not found using the doc_prefix, try
               // using no prefix (i.e. the C++ standard itself)
               if (!tag.prefix.empty() && section_db.find(tag) == section_db.end())
               {
                 //std::cout << "issue:" << is.num << " tag" << tag << '\n';
                 lwg::section_tag fallback_tag;
                 fallback_tag.name = tag.name;
                 if (section_db.find(fallback_tag) != section_db.end())
                 {
                   //std::cout << "bingo\n";
                   tag = fallback_tag;
                   //std::cout << "section_tag=\"" << tag.prefix << "\", \"" << tag.name << "\"\n";
                 }
               }

               j -= i - 1;
               std::string r = lwg::format_section_tag_as_link(section_db, tag);
               s.replace(i, j, r);
               i += r.size() - 1;
               continue;
            }

            // format issue references
            else if (tag == "iref") {
               std::string r{get_attribute_value("iref", attrs, context)};
               int num;
               {
                  std::istringstream temp{r};
                  temp >> num;
                  if (temp.fail()) {
                     fail("Bad number in <iref>", context);
                  }
               }

               auto n = std::lower_bound(first_issue, last_issue, num, lwg::order_by_issue_number{});
               if (n == last_issue  or  n->num != num) {
                  fail("Could not find issue " + r + " for <iref>", context);
               }

               if (!tag_stack.empty()  and  tag_stack.back() == "duplicate") {
                  n->duplicates.insert(make_html_anchor(is));
                  is.duplicates.insert(make_html_anchor(*n));
                  r.clear();
               }
               else {
                  r = make_html_anchor(*n);
                  r += "<sup><a href=\"https://cplusplus.github.io/LWG/issue";
                  r += std::to_string(num);
                  r += "\" title=\"Latest snapshot\">(i)</a></sup>";
               }

               j -= i - 1;
               s.replace(i, j, r);
               i += r.size() - 1;
               continue;
            }
            else if (tag == "paper") {
               std::string paper_number{get_attribute_value("paper", attrs, context)};
               static const std::regex acceptable_numbers(R"(N\d+|[DP]\d+R\d+|[DP]\d+)", std::regex::icase);

               if (!std::regex_match(paper_number, acceptable_numbers)) {
                  fail("Invalid paper number '" + paper_number + "'", context);
               }

               // normalize paper numbers to use uppercase
               std::transform(paper_number.begin(), paper_number.end(), paper_number.begin(),
                     [] (unsigned char c) { return std::toupper(c); });

               auto title = paper_title_attr(paper_number, meta);

               j -= i - 1;
               std::string r = "<a href=\"https://wg21.link/" + paper_number + "\"" + title + ">" + paper_number + "</a>";
               s.replace(i, j, r);
               i += r.size() - 1;
               continue;
            }
            i = j;
            continue;  // don't worry about this <tag/>
         }

         tag_stack.push_back(tag);
         if (tag == "resolution") {
             std::ostringstream os;
             os << "<p id=\"res-" << is.num << "\"><b>Proposed resolution:</b></p>";
             auto r = os.str();
             s.replace(i, j-i + 1, r);
             i += r.length() - 1;
         }
         else if (auto r = substitutions.find(tag); r != substitutions.end()) {
             s.replace(i, j-i + 1, r->second.first);
             i += r->second.first.size() - 1;
         }
         else if (tag == "!--") {
             tag_stack.pop_back();
             j = s.find("-->", i);
             j += 3;
             s.erase(i, j-i);
             --i;
         }
         else {
             i = j;
         }
      }
      if (!tag_stack.empty())
         throw std::runtime_error("Unclosed tag <" + tag_stack.back() + "> in issue " + std::to_string(is.num));
   };

   fix_tags(is.text);
   fix_tags(is.resolution);
}


void prepare_issues(std::vector<lwg::issue> & issues, lwg::metadata & meta) {
   // Initially sort the issues by issue number, so each issue can be correctly 'format'ted
   sort(issues.begin(), issues.end(), lwg::order_by_issue_number{});

   // Then we format the issues, which should be the last time we need to touch the issues themselves
   // We may turn this into a two-stage process, analysing duplicates and then applying the links
   // This will allow us to better express constness when the issues are used purely for reference.
   // Currently, the 'format' function takes a reference-to-non-const-vector-of-issues purely to
   // mark up information related to duplicates, so processing duplicates in a separate pass may
   // clarify the code.
   for (auto & i : issues) { format_issue_as_html(i, issues.begin(), issues.end(), meta); }

   // Issues will be routinely re-sorted in later code, but contents should be fixed after formatting.
   // This suggests we may want to be storing some kind of issue handle in the functions that keep
   // re-sorting issues, and so minimize the churn on the larger objects.
}


// ============================================================================================================

auto prepare_issues_for_diff_report(std::vector<lwg::issue> const & issues) -> std::vector<std::tuple<int, std::string> > {
   std::vector<std::tuple<int, std::string> > result;
   std::transform( issues.begin(), issues.end(), back_inserter(result),
#if 1
                   [](lwg::issue const & iss) { return std::make_tuple(iss.num, iss.stat); }
#else
                   // This form does not work because tuple constructors are explicit
                   [](lwg::issue const & iss) -> std::tuple<int, std::string> { return {iss.num, iss.stat}; }
#endif
                 );
   return result;
}

struct list_issues {
   std::vector<int> const & issues;
};


auto operator<<( std::ostream & out, list_issues const & x) -> std::ostream & {
   auto list_separator = "";
   for (auto number : x.issues) {
      out << list_separator << "<iref ref=\"" << number << "\"/>";
      list_separator = ", ";
   }
   return out;
}


struct find_num {
   // Predicate functor useful to find issue 'y' in a mapping of issue-number -> some string.
    bool operator()(std::tuple<int, std::string> const & x, int y) const noexcept {
      return std::get<0>(x) < y;
   }
};


struct discover_new_issues {
   std::vector<std::tuple<int, std::string> > const & old_issues;
   std::vector<std::tuple<int, std::string> > const & new_issues;
};


auto operator<<( std::ostream & out, discover_new_issues const & x) -> std::ostream & {
   std::vector<std::tuple<int, std::string> > const & old_issues = x.old_issues;
   std::vector<std::tuple<int, std::string> > const & new_issues = x.new_issues;

   struct status_order {
      // predicate for 'map'

      using status_string = std::string;
      auto operator()(status_string const & x, status_string const & y) const noexcept -> bool {
         return lwg::get_status_priority(x) < lwg::get_status_priority(y);
      }
   };

   std::map<std::string, std::vector<int>, status_order> added_issues;
   for (auto const & i : new_issues) {
      auto j = std::lower_bound(old_issues.cbegin(), old_issues.cend(), std::get<0>(i), find_num{});
      if(j == old_issues.end() or std::get<0>(*j) != std::get<0>(i)) {
         added_issues[std::get<1>(i)].push_back(std::get<0>(i));
      }
   }

   for (auto const & i : added_issues) {
      auto const item_count = std::get<1>(i).size();
      if(1 == item_count) {
         out << "<li>Added the following " << std::get<0>(i) << " issue: <iref ref=\"" << std::get<1>(i).front() << "\"/>.</li>\n";
      }
      else {
         out << "<li>Added the following " << item_count << " " << std::get<0>(i) << " issues: " << list_issues{std::get<1>(i)} << ".</li>\n";
      }
   }

   if (added_issues.empty()) {
      out << "<li>No issues added.</li>\n";
   }

   return out;
}


struct discover_changed_issues {
   std::vector<std::tuple<int, std::string> > const & old_issues;
   std::vector<std::tuple<int, std::string> > const & new_issues;
};


auto operator << (std::ostream & out, discover_changed_issues x) -> std::ostream & {
   std::vector<std::tuple<int, std::string> > const & old_issues = x.old_issues;
   std::vector<std::tuple<int, std::string> > const & new_issues = x.new_issues;

   struct status_transition_order {
      using status_string = std::string;
      using from_status_to_status = std::tuple<status_string, status_string>;

      auto operator()(from_status_to_status const & x, from_status_to_status const & y) const noexcept -> bool {
         auto const xp2 = lwg::get_status_priority(std::get<1>(x));
         auto const yp2 = lwg::get_status_priority(std::get<1>(y));
         return xp2 < yp2  or  (!(yp2 < xp2)  and  lwg::get_status_priority(std::get<0>(x)) < lwg::get_status_priority(std::get<0>(y)));
      }
   };

   std::map<std::tuple<std::string, std::string>, std::vector<int>, status_transition_order> changed_issues;
   for (auto const & i : new_issues) {
      auto j = std::lower_bound(old_issues.begin(), old_issues.end(), std::get<0>(i), find_num{});
      if (j != old_issues.end()  and  std::get<0>(i) == std::get<0>(*j)  and  std::get<1>(*j) != std::get<1>(i)) {
         changed_issues[std::tuple<std::string, std::string>{std::get<1>(*j), std::get<1>(i)}].push_back(std::get<0>(i));
      }
   }

   for (auto const & i : changed_issues) {
      auto const item_count = std::get<1>(i).size();
      if(1 == item_count) {
         out << "<li>Changed the following issue to " << std::get<1>(std::get<0>(i))
             << " (from " << std::get<0>(std::get<0>(i)) << "): <iref ref=\"" << std::get<1>(i).front() << "\"/>.</li>\n";
      }
      else {
         out << "<li>Changed the following " << item_count << " issues to " << std::get<1>(std::get<0>(i))
             << " (from " << std::get<0>(std::get<0>(i)) << "): " << list_issues{std::get<1>(i)} << ".</li>\n";
      }
   }

   if (changed_issues.empty()) {
      out << "<li>No issues changed.</li>\n";
   }

   return out;
}


void count_issues(std::vector<std::tuple<int, std::string> > const & issues, int & n_open, int & n_reassigned, int & n_closed) {
   n_open = 0;
   n_reassigned = 0;
   n_closed = 0;

   for(auto const & elem : issues) {
      if (lwg::is_assigned_to_another_group(std::get<1>(elem))) {
      	++n_reassigned;
      }
      else if (lwg::is_active(std::get<1>(elem))) {
         ++n_open;
      }
      else {
         ++n_closed;
      }
   }
}


struct write_summary {
   std::vector<std::tuple<int, std::string> > const & old_issues;
   std::vector<std::tuple<int, std::string> > const & new_issues;
};


auto operator << (std::ostream & out, write_summary const & x) -> std::ostream & {
   std::vector<std::tuple<int, std::string> > const & old_issues = x.old_issues;
   std::vector<std::tuple<int, std::string> > const & new_issues = x.new_issues;

   int n_open_new = 0;
   int n_open_old = 0;
   int n_reassigned_new = 0;
   int n_reassigned_old = 0;
   int n_closed_new = 0;
   int n_closed_old = 0;
   count_issues(old_issues, n_open_old, n_reassigned_old, n_closed_old);
   count_issues(new_issues, n_open_new, n_reassigned_new, n_closed_new);
   auto write_change = [&out](int n_new, int n_old){
      out << (n_new >= n_old ? "up by " : "down by ")
          << std::abs(n_new - n_old);
   };

   out << "<li>" << n_open_new << " open issues, ";
   write_change(n_open_new, n_open_old);
   out << ".</li>\n";

   out << "<li>" << n_closed_new << " closed issues, ";
   write_change(n_closed_new, n_closed_old);
   out << ".</li>\n";

   out << "<li>" << n_reassigned_new << " reassigned issues, ";
   write_change(n_reassigned_new, n_reassigned_old);
   out << ".</li>\n";

   int n_total_new = n_open_new + n_reassigned_new + n_closed_new;
   int n_total_old = n_open_old + n_reassigned_old + n_closed_old;
   out << "<li>" << n_total_new << " issues total, ";
   write_change(n_total_new, n_total_old);
   out << ".</li>\n";

   return out;
}


void print_current_revisions( std::ostream & out
                            , std::vector<std::tuple<int, std::string> > const & old_issues
                            , std::vector<std::tuple<int, std::string> > const & new_issues
                            ) {
   out << "<ul>\n"
          "<li><b>Summary:</b><ul>\n"
       << write_summary{old_issues, new_issues}
       << "</ul></li>\n"
          "<li><b>Details:</b><ul>\n"
       << discover_new_issues{old_issues, new_issues}
       << discover_changed_issues{old_issues, new_issues}
       << "</ul></li>\n"
          "</ul>\n";
}

// ============================================================================================================

void check_is_directory(fs::path const & directory) {
   if (!is_directory(directory)) {
      throw std::runtime_error(directory.string() + " is not an existing directory");
   }
}

int main(int argc, char* argv[]) {
   try {
      fs::path path;
      bool revhist = false;
      std::cout << "Preparing new LWG issues lists..." << std::endl;
      if (argc == 2) {
         path = argv[1];
      }
      else {
         path = fs::current_path();

         if (argc == 3 && std::string(argv[1]) == "revision" && std::string(argv[2]) == "history")
            revhist = true;
      }

      check_is_directory(path);

      const fs::path target_path{path / "mailing"};
      check_is_directory(target_path);

      auto metadata = lwg::metadata::read_from_path(path);
#if defined (DEBUG_LOGGING)
      // dump the contents of the section index
      for (auto const & elem : metadata.section_db) {
         std::string temp = elem.first;
         temp.erase(temp.end()-1);
         temp.erase(temp.begin());
         std::cout << temp << ' ' << elem.second << '\n';
      }
#endif

      auto const old_issues = read_issues_from_toc(read_file_into_string(path / "meta-data" / "lwg-toc.old.html"));

      auto const issues_path = path / "xml";

      lwg::mailing_info lwg_issues_xml = [&issues_path](){
         fs::path filename{issues_path / "lwg-issues.xml"};
         std::ifstream infile{filename};
         if (!infile.is_open()) {
            throw std::runtime_error{"Unable to open " + filename.string()};
         }

         return lwg::mailing_info{infile};
      }();

      //lwg::mailing_info lwg_issues_xml{issues_path};


      std::cout << "Reading issues from: " << issues_path << std::endl;
      auto issues = read_issues(issues_path, metadata);
      prepare_issues(issues, metadata);


      lwg::report_generator generator{lwg_issues_xml, metadata.section_db};


      // issues must be sorted by number before making the mailing list documents
      //sort(issues.begin(), issues.end(), order_by_issue_number{});

      // Collect a report on all issues that have changed status
      // This will be added to the revision history of the 3 standard documents
      auto const new_issues = prepare_issues_for_diff_report(issues);

      if (revhist) {
         std::cout << "\n<revision tag=\"" << lwg_issues_xml.get_revision() << "\">\n"
            << lwg_issues_xml.get_date()  << ' ' << lwg_issues_xml.get_title() << '\n';
         print_current_revisions(std::cout, old_issues, new_issues);
         std::cout << "</revision>\n";
         return 0;
      }

      std::ostringstream os_diff_report;
      print_current_revisions(os_diff_report, old_issues, new_issues );
      auto const diff_report = os_diff_report.str();

      std::vector<lwg::issue> unresolved_issues;
      std::vector<lwg::issue> votable_issues;

      std::copy_if(issues.begin(), issues.end(), std::back_inserter(unresolved_issues), [](lwg::issue const & iss){ return lwg::is_not_resolved(iss.stat); } );
      std::copy_if(issues.begin(), issues.end(), std::back_inserter(votable_issues),    [](lwg::issue const & iss){ return lwg::is_votable(iss.stat); } );

      // If votable list is empty, we are between meetings and should list Ready issues instead
      // Otherwise, issues moved to Ready during a meeting will remain 'unresolved' by that meeting
      auto ready_inserter = votable_issues.empty()
                          ? std::back_inserter(votable_issues)
                          : std::back_inserter(unresolved_issues);
      std::copy_if(issues.begin(), issues.end(), ready_inserter, [](lwg::issue const & iss){ return lwg::is_ready(iss.stat); } );

      // First generate the primary 3 standard issues lists
      generator.make_active(issues, target_path, diff_report);
      generator.make_defect(issues, target_path, diff_report);
      generator.make_closed(issues, target_path, diff_report);

      // unofficial documents
      generator.make_tentative (issues, target_path);
      generator.make_unresolved(issues, target_path);
      generator.make_immediate (issues, target_path);
      generator.make_ready     (issues, target_path);
      generator.make_editors_issues(issues, target_path);
      generator.make_individual_issues(issues, target_path);



      // Now we have a parsed and formatted set of issues, we can write the standard set of HTML documents
      // Note that each of these functions is going to re-sort the 'issues' vector for its own purposes
      generator.make_sort_by_num            (issues, {target_path / "lwg-toc.html"});
      generator.make_sort_by_status         (issues, {target_path / "lwg-status.html"});
      generator.make_sort_by_status_mod_date(issues, {target_path / "lwg-status-date.html"});  // this report is useless, as git checkouts touch filestamps
      generator.make_sort_by_section        (issues, {target_path / "lwg-index.html"});

      // Note that this additional document is very similar to unresolved-index.html below
      generator.make_sort_by_section        (issues, {target_path / "lwg-index-open.html"}, true);

      // Make a similar set of index documents for the issues that are 'live' during a meeting
      // Note that these documents want to reference each other, rather than lwg- equivalents,
      // although it may not be worth attempting fix-ups as the per-issue level
      // During meetings, it would be good to list newly-Ready issues here
      generator.make_sort_by_num            (unresolved_issues, {target_path / "unresolved-toc.html"});
      generator.make_sort_by_status         (unresolved_issues, {target_path / "unresolved-status.html"});
      generator.make_sort_by_status_mod_date(unresolved_issues, {target_path / "unresolved-status-date.html"});
      generator.make_sort_by_section        (unresolved_issues, {target_path / "unresolved-index.html"});
      generator.make_sort_by_priority       (unresolved_issues, {target_path / "unresolved-prioritized.html"});

      // Make another set of index documents for the issues that are up for a vote during a meeting
      // Note that these documents want to reference each other, rather than lwg- equivalents,
      // although it may not be worth attempting fix-ups as the per-issue level
      // Between meetings, it would be good to list Ready issues here
      generator.make_sort_by_num            (votable_issues, {target_path / "votable-toc.html"});
      generator.make_sort_by_status         (votable_issues, {target_path / "votable-status.html"});
      generator.make_sort_by_status_mod_date(votable_issues, {target_path / "votable-status-date.html"});
      generator.make_sort_by_section        (votable_issues, {target_path / "votable-index.html"});

      std::cout << "Made all documents\n";
   }
   catch(std::exception const & ex) {
      std::cout << ex.what() << std::endl;
      return 1;
   }
}
