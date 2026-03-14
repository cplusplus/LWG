#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

#include "issues.h"
#include "metadata.h"
#include "status.h"
#include "html_utils.h"

#include <algorithm>
#include <cassert>
#include <istream>
#include <ostream>
#include <string>
#include <stdexcept>

#include <iterator>
#include <fstream>
#include <sstream>
#include <format>

#include <iostream>  // eases debugging

#include <ctime>
#include <chrono>

#include <filesystem>
namespace fs = std::filesystem;

using std::size_t;

namespace {
auto parse_date(std::istream & temp) -> std::chrono::year_month_day {
#if __cpp_lib_chrono >= 201803L
   std::chrono::year_month_day date{};
   if (temp >> std::chrono::parse(" %d %b %Y", date))
      return date;
   throw std::runtime_error{"date format error"};
#else
   using month_idx = std::pair<std::string_view,int>;
   constexpr month_idx months[] = {
      {"Apr", 4},{"Aug", 8},{"Dec",12},{"Feb", 2},{"Jan", 1},{"Jul", 7},
      {"Jun", 6},{"Mar", 3},{"May", 5},{"Nov",11},{"Oct",10},{"Sep", 9}
   };

   int d;
   std::string month;
   int y;
   if (temp >> d >> month >> y)
   {
      auto it = std::ranges::lower_bound(months, month, {}, &month_idx::first);

      if (it != std::end(months) and it->first == month)
      {
         int m = it->second;
         if (auto ymd = std::chrono::year{y}/m/d; ymd.ok())
            return ymd;
      }
   }
   throw std::runtime_error{"date format error"};
#endif
}

auto report_date_file_last_modified(std::filesystem::path const & filename, lwg::metadata const& meta) -> std::chrono::year_month_day {
   using namespace std::chrono;
   system_clock::time_point t;
   // NB: Cannot use `native()` instead of `string()`, because on Windows that
   // would result in std::wstring:
   int id = lwg::stoi(filename.filename().stem().string().substr(5));
   // Use the Git commit date of the file if available.
   if (auto it = meta.git_commit_times.find(id); it !=  meta.git_commit_times.end())
      t = system_clock::from_time_t(it->second);
   else {
     // Otherwise use the modification time of the file.
      auto mtime = fs::last_write_time(filename);
#if __cpp_lib_chrono >= 201803L
      t = clock_cast<system_clock>(mtime);
#else
      // clock_cast isn't supported, so convert to sys_time manually.
      static const auto snow = system_clock::now();
      static const auto fnow = fs::file_time_type::clock::now();
      t = snow - round<seconds>(fnow - mtime);
#endif
   }

   return year_month_day(floor<days>(t));
}

// Replace '<' and '>' and '&' with HTML character references.
// This is used to turn backtick-quoted inline code into valid XML/HTML.
std::string escape_special_chars(std::string s) {
   static const std::vector<std::pair<char, std::string_view>> subs {
      { '&', "&amp;" }, // this has to come first!
      { '<', "&lt;" },
      { '>', "&gt;" },
   };
   for (auto [c, r] : subs)
      for (auto p = s.find(c); p != s.npos; p = s.find(c, p+r.size()))
         s.replace(p, 1, r);
   return s;
}

} // close unnamed namespace

auto lwg::parse_issue_from_file(std::string tx, std::string const & filename,
  lwg::metadata & meta) -> issue {
   auto& section_db = meta.section_db;
   struct bad_issue_file : std::runtime_error {
      bad_issue_file(std::string const & filename, std::string error_message)
         : runtime_error{"Error parsing issue file " + filename + ": " + error_message}
         { }
   };

   // Replace ```code block``` with valid XML.
   for (size_t p = tx.find("\n```\n"); p != tx.npos; p = tx.find("\n```\n", p))
   {
      size_t p2 = tx.find("\n```\n", p + 5);
      if (p2 == tx.npos)
         throw bad_issue_file{filename, "Unmatched ``` code block: " + tx.substr(p, 10)};
      auto code = "\n<pre><code>" + escape_special_chars(tx.substr(p + 5, p2 - p - 5)) + "\n</code></pre>\n";
      tx.replace(p, p2 - p + 5, code);
      p += code.size();
   }

   // Replace inline `code` with valid XML.
   for (size_t p = tx.find('`'); p != tx.npos; p = tx.find('`', p))
   {
      if (tx[p+1] == '`')
      {
         // Some issues use double backtick for ``quotes like this''.
         // We don't want to do anything here.
         p += 2;
         continue;
      }
      size_t p2 = tx.find('`', p + 1);
      if (p2 > tx.find('\n', p + 1))
      {
         // Do not treat "`foo\nbar`" as inline code.
         // Move to the next backtick and check that one.
         p = p2;
         continue;
      }
      // This class attribute is used by the CSS in src/report_generator.cpp
      // so that the backticks are still displayed if this occurs inside a
      // <pre> element (because that always displays in code font anyway).
      auto code = "<code class='backtick'>"
         + escape_special_chars(tx.substr(p + 1, p2 - p - 1))
         + "</code>";
      tx.replace(p, p2 - p + 1, code);
      p += code.size();
   }

   // <tt> is obsolete in HTML5, replace with <code>:
   for (auto p = tx.find("<tt"); p != tx.npos; p = tx.find("<tt", p+5))
      if (tx.at(p+3) == '>' || tx.at(p+3) == ' ')
         tx.replace(p, 3, "<code");
   for (auto p = tx.find("</tt>"); p != tx.npos; p = tx.find("</tt>", p+7))
         tx.replace(p, 5, "</code>");

   issue is;

   auto get_or_throw = [&filename](const auto& opt, std::string_view what) {
      return opt ? *opt : throw bad_issue_file(filename, "Unable to find issue " + std::string(what));
   };

   // Get value from "<issue attr='value'>"
   auto get_attr = [&](std::string_view attr) {
      return get_or_throw(lwg::get_attribute_of(attr, "issue", tx), attr);
   };
   // Get content from "<elem>content</elem>"
   auto get_elem_content = [&](std::string_view elem) {
      return get_or_throw(lwg::get_element_content(elem, tx), elem);
   };

   // Get issue number
   std::string_view num = get_attr("num");
   if (!filename.ends_with(std::format("issue{:0>4}.xml", num)))
     std::cerr << "warning: issue number " << num << " in " << filename << " does not match issue number\n";
   is.num = lwg::stoi(std::string(num));

   // Get issue status
   is.stat = get_attr("status");

   // Get issue title
   is.title = get_elem_content("title");

   // Extract doc_prefix from title
   if (is.title[0] == '['
     && is.title.find("[CD]") == std::string::npos) // special case for a few titles
   {
      std::string::size_type pos = is.title.find(']');
      if (pos != std::string::npos)
        is.doc_prefix = is.title.substr(1, pos - 1);
//    std::cout << is.doc_prefix << '\n';
   }

   // Get issue sections
   std::string_view sections = get_elem_content("section");
   while (auto sref = lwg::get_element("sref", sections))
   {
      if (auto attr = lwg::get_attribute("ref", sref->outer))
      {
         if (attr->starts_with('[') and attr->ends_with(']'))
         {
            attr->remove_prefix(1);
            attr->remove_suffix(1);
         }
         else
            throw bad_issue_file{filename, "Invalid section name in <sref>"};

         section_tag tag;
         tag.prefix = is.doc_prefix;
         tag.name = *attr;
         is.tags.emplace_back(tag);
         if (section_db.find(is.tags.back()) == section_db.end()) {
             section_num num{};
             num.prefix = tag.prefix;
             num.num.push_back(99);
             section_db[is.tags.back()] = num;
         }
      }
      else
         throw bad_issue_file{filename, "Missing ref attribute in <sref>"};
      sections.remove_prefix(sref->outer.data() - sections.data() + sref->outer.size());
   }

   if (is.tags.empty()) {
      throw bad_issue_file{filename, "Unable to find issue section"};
   }

   // Get submitter
   is.submitter = get_elem_content("submitter");

   // Get date
   auto datestr = get_elem_content("date");

   try {
#ifdef __cpp_lib_sstream_from_string_view
      std::istringstream temp{datestr};
#else
      std::istringstream temp{std::string{datestr}};
#endif
      is.date = parse_date(temp);
   }
   catch(std::exception const & ex) {
      throw bad_issue_file{filename, "date format error"};
   }

   // Get modification date
   is.mod_date = report_date_file_last_modified(filename, meta);

   // Get priority - this element is optional
   if (auto o = lwg::get_element_content("priority", tx))
      is.priority = lwg::stoi(std::string(*o));

   // Trim text to <discussion>
   if (auto k = tx.find("<discussion>"); k != tx.npos)
      tx.replace(0, k, "<issue>");
   else
      throw bad_issue_file{filename, "Unable to find issue discussion"};

   // Find out if issue has a proposed resolution
   if (is_active(is.stat)  or  "Pending WP" == is.stat) {
      auto resolution = lwg::get_element_content("resolution", tx);
      // Ignore small amounts of whitespace between tags, with no actual resolution
      is.has_resolution = resolution.has_value() && resolution->length() >= 15;

      // The is.text string already contains the <resolution> element,
      // but is.resolution stores another copy of it.
      // That was used to prepare a new report for the editor containing
      // only the issues approved at a meeting.
      // We don't currently do this.
      // is.resolution = *resolution;
   }
   else {
      is.has_resolution = true;
   }

   is.text = std::move(tx);
   return is;
}
