#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <utility>
#include <iterator>
#include <stdexcept>
#include <map>
#include <algorithm>
#include <string_view>
#include <filesystem>
#include <span>

namespace fs = std::filesystem;

// DEBUG VISUALIZATION TOOL ONLY
void display_issues(std::span<const std::pair<int, std::string>> issues) {
   for( auto const & x : issues) {
      std::cout << x.first << "\t" << x.second << '\n';
   }
   std::cout << '\n';
}


// PRODUCTION CODE STARTS HERE

auto remove_prefix(std::string_view stat, std::string_view prefix) -> std::string_view {
   if (stat.starts_with(prefix)) {
      stat.remove_prefix(prefix.size());
   }
   return stat;
}


auto remove_pending(std::string_view stat) -> std::string_view {
   return remove_prefix(stat, "Pending");
}


auto remove_tentatively(std::string_view stat) -> std::string_view {
   return remove_prefix(stat, "Tentatively");
}


auto remove_qualifier(std::string_view stat) -> std::string_view {
    return remove_tentatively(remove_pending(stat));
}


auto find_file(std::string_view status) -> std::string_view {
    if (status.starts_with("Tentatively")) {
        return "lwg-active.html";
    }

    auto stat = remove_qualifier(status);
    return stat == "TC1"            ?  "lwg-defects.html"
         : stat == "CD1"            ?  "lwg-defects.html"
         : stat == "TS"             ?  "lwg-defects.html"
         : stat == "WP"             ?  "lwg-defects.html"
         : stat == "C++23"          ?  "lwg-defects.html"
         : stat == "C++20"          ?  "lwg-defects.html"
         : stat == "C++17"          ?  "lwg-defects.html"
         : stat == "C++14"          ?  "lwg-defects.html"
         : stat == "C++11"          ?  "lwg-defects.html"
         : stat == "Resolved"       ?  "lwg-defects.html"
         : stat == "DR"             ?  "lwg-defects.html"
         : stat == "TRDec"          ?  "lwg-defects.html"
         : stat == "Dup"            ?  "lwg-closed.html"
         : stat == "NAD"            ?  "lwg-closed.html"
         : stat == "NAD Future"     ?  "lwg-closed.html"
         : stat == "NAD Editorial"  ?  "lwg-closed.html"
         : stat == "NAD Concepts"   ?  "lwg-closed.html"
         : stat == "NAD Arrays"     ?  "lwg-closed.html"
         : stat == "Deferred"       ?  "lwg-active.html"
         : stat == "Ready"          ?  "lwg-active.html"
         : stat == "Review"         ?  "lwg-active.html"
         : stat == "New"            ?  "lwg-active.html"
         : stat == "Open"           ?  "lwg-active.html"
         : throw std::runtime_error{"unknown status " + std::string(stat)};
}


auto is_active(std::string_view stat) -> bool {
    return find_file(stat) == "lwg-active.html";
}


auto read_issues(std::istream& is) -> std::vector<std::pair<int, std::string> > {
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

   std::istreambuf_iterator<char> first{is}, last{};
   std::string s{first, last};

   // Skip the title row
   auto i = s.find("<tr>");
   if (std::string::npos == i) {
      throw std::runtime_error{"Unable to find the first (title) row"};
   }

   // Read all issues in table
   std::vector<std::pair<int, std::string> > issues;
   while (true) {
      i = s.find("<tr>", i+4);
      if (i == std::string::npos) {
         break;
      }
      i = s.find("</a>", i);
      auto j = s.rfind('>', i);
      if (j == std::string::npos) {
         throw std::runtime_error{"unable to parse issue number: can't find beginning bracket"};
      }
      std::istringstream instr{s.substr(j+1, i-j-1)};
      int num;
      instr >> num;
      if (instr.fail()) {
         throw std::runtime_error{"unable to parse issue number"};
      }
      i = s.find("</a>", i+4);
      if (i == std::string::npos) {
         throw std::runtime_error{"partial issue found"};
      }
      j = s.rfind('>', i);
      if (j == std::string::npos) {
         throw std::runtime_error{"unable to parse issue status: can't find beginning bracket"};
      }
      issues.push_back({num, s.substr(j+1, i-j-1)});
   }

   //display_issues(issues);
   return issues;
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
   bool operator()(const std::pair<int, std::string>& x, int y) const noexcept {
      return x.first < y;
   }
};


struct discover_new_issues {
   std::vector<std::pair<int, std::string> > const & old_issues;
   std::vector<std::pair<int, std::string> > const & new_issues;
};


auto operator<<( std::ostream & out, discover_new_issues const & x) -> std::ostream & {
   std::vector<std::pair<int, std::string> > const & old_issues = x.old_issues;
   std::vector<std::pair<int, std::string> > const & new_issues = x.new_issues;

   std::map<std::string, std::vector<int> > added_issues;
   for( auto const & i : new_issues) {
      auto j = std::lower_bound(old_issues.cbegin(), old_issues.cend(), i.first, find_num{});
      if(j == old_issues.end()) {
         added_issues[i.second].push_back(i.first);
      }
   }

   for( auto const & i : added_issues) {
      auto const item_count = i.second.size();
      if(1 == item_count) {
         out << "<li>Added the following " << i.first << " issue: <iref ref=\"" << i.second.front() << "\"/>.</li>\n";
      }
      else {
         out << "<li>Added the following " << item_count << " " << i.first << " issues: " << list_issues{i.second} << ".</li>\n";
      }
   }

   if (added_issues.empty()) {
      out << "<li>No issues added.</li>\n";
   }

   return out;
}


struct reverse_pair {
   // member template means we cannot make this a local class
   template <class T, class U>
   auto operator()(const T& x, const U& y) const noexcept -> bool {
      return static_cast<bool>(  x.second < y.second  or
                              (!(y.second < x.second)  and  x.first < y.first));
   }
};

struct discover_changed_issues {
   std::vector<std::pair<int, std::string> > const & old_issues;
   std::vector<std::pair<int, std::string> > const & new_issues;
};


auto operator<<( std::ostream & out, discover_changed_issues x) -> std::ostream & {
   std::vector<std::pair<int, std::string> > const & old_issues = x.old_issues;
   std::vector<std::pair<int, std::string> > const & new_issues = x.new_issues;

   std::map<std::pair<std::string, std::string>, std::vector<int>, reverse_pair> changed_issues;
   for (auto const & i : new_issues) {
      auto j = std::lower_bound(old_issues.begin(), old_issues.end(), i.first, find_num{});
      if (j != old_issues.end()  and  i.first == j->first  and  j->second != i.second) {
         changed_issues[make_pair(j->second, i.second)].push_back(i.first);
      }
   }

   for (auto const & i : changed_issues) {
      auto const item_count = i.second.size();
      if(1 == item_count) {
         out << "<li>Changed the following issue from " << i.first.first << " to " << i.first.second
             << ": <iref ref=\"" << i.second.front() << "\"/>.</li>\n";
      }
      else {
         out << "<li>Changed the following " << item_count << " issues from " << i.first.first << " to " << i.first.second
             << ": " << list_issues{i.second}
             << ".</li>\n";
      }
   }

   if (changed_issues.empty()) {
      out << "<li>No issues changed.</li>\n";
   }

   return out;
}


void count_issues(std::span<const std::pair<int, std::string>> issues, unsigned & n_open, unsigned & n_closed) {
   n_open = 0;
   n_closed = 0;

   for (auto const & elem : issues) {
      if (is_active(elem.second)) {
         ++n_open;
      }
      else {
         ++n_closed;
      }
   }
}


struct write_summary {
   std::vector<std::pair<int, std::string> > const & old_issues;
   std::vector<std::pair<int, std::string> > const & new_issues;
};


auto operator<<( std::ostream & out, write_summary const & x) -> std::ostream & {
   std::vector<std::pair<int, std::string> > const & old_issues = x.old_issues;
   std::vector<std::pair<int, std::string> > const & new_issues = x.new_issues;

   unsigned n_open_new = 0;
   unsigned n_open_old = 0;
   unsigned n_closed_new = 0;
   unsigned n_closed_old = 0;
   count_issues(old_issues, n_open_old, n_closed_old);
   count_issues(new_issues, n_open_new, n_closed_new);

   out << "<li>" << n_open_new << " open issues, ";
   if (n_open_new >= n_open_old) {
      out << "up by " << n_open_new - n_open_old;
   }
   else {
      out << "down by " << n_open_old - n_open_new;
   }
   out << ".</li>\n";

   std::cout << "<li>" << n_closed_new << " closed issues, ";
   if (n_closed_new >= n_closed_old) {
      out << "up by " << n_closed_new - n_closed_old;
   }
   else {
      out << "down by " << n_closed_old - n_closed_new;
   }
   out << ".</li>\n";

   unsigned n_total_new = n_open_new + n_closed_new;
   unsigned n_total_old = n_open_old + n_closed_old;
   out << "<li>" << n_total_new << " issues total, ";
   if (n_total_new >= n_total_old) {
      out << "up by " << n_total_new - n_total_old;
   }
   else {
      out << "down by " << n_total_old - n_total_new;
   }
   out << ".</li>\n";

   return out;
}


void print_current_revisions( std::ostream & out
                            , std::vector<std::pair<int, std::string> > const & old_issues
                            , std::vector<std::pair<int, std::string> > const & new_issues
                            ) {
   out << "<ul>\n"
       << "<li><b>Summary:</b><ul>\n"
       << write_summary{old_issues, new_issues}
       << "</ul></li>\n"
       << "<li><b>Details:</b><ul>\n"
       << discover_new_issues{old_issues, new_issues}
       << discover_changed_issues{old_issues, new_issues}
       << "</ul></li>\n"
       << "</ul>\n";
}


auto read_issues(fs::path const & filename) -> std::vector<std::pair<int, std::string> > {
   std::ifstream new_html{filename};
   if(!new_html.is_open()) {
      throw std::runtime_error{"Unable to open toc file: " + filename.string()};
   }
   return read_issues(new_html);
}


int main (int argc, char* const argv[]) {
   try {
      fs::path path;
      if (2 == argc) {
         path = argv[1];
      }
      else {
         path = fs::current_path();
      }

      auto const old_issues = read_issues(path / "meta-data" / "lwg-toc.old.html");
      auto const new_issues = read_issues(path / "lwg-toc.html");

      print_current_revisions(std::cout, old_issues, new_issues );
   }
   catch(std::exception const & ex) {
      std::cout << ex.what() << std::endl;
      return 1;
   }
}
