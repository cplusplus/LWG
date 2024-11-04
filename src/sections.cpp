#include "sections.h"

#include <cassert>
#include <sstream>
#include <iostream>
#include <cctype>
#include <utility>

auto lwg::operator << (std::ostream& os, section_tag const & tag) -> std::ostream & {
  os << '[';
   if (!tag.prefix.empty()) { os << tag.prefix << "::"; }
   os <<  tag.name << ']';
   return os;
}

std::string lwg::as_string(section_tag const & x)
{
  return x.prefix.empty()
    ? x.name
    : x.prefix + "::" + x.name;
}

auto lwg::operator >> (std::istream& is, section_num& sn) -> std::istream & {
   sn.prefix.clear();
   sn.num.clear();
   ws(is);
   if (is.peek() == 'T') {
      is.get();
      if (is.peek() == 'R' || is.peek() == 'S') {
        sn.prefix = 'T';
        std::string w;
        is >> w;
        sn.prefix += w;
        ws(is);
      }
      else {
         sn.num.push_back(100 + 'T' - 'A');
         if (is.peek() != '.') {
            return is;
         }
         is.get();
      }
   }

   while (true) {
      if (std::isdigit(is.peek())) {
         int n;
         is >> n;
         sn.num.push_back(n);
      }
      else {
         char c;
         is >> c;
         sn.num.push_back(100 + c - 'A');
      }
      if (is.peek() != '.') {
         break;
      }
      char dot;
      is >> dot;
   }
   return is;
}

auto lwg::operator << (std::ostream& os, section_num const & sn) -> std::ostream & {
//   if (!sn.prefix.empty()) { os << sn.prefix << " "; }

   bool use_period{false};
   for (auto sub : sn.num) {
      if (std::exchange(use_period, true)) {
         os << '.';
      }

      if (sub >= 100) {
         os << char(sub - 100 + 'A');
      }
      else {
         os << sub;
      }
   }
   return os;
}

auto lwg::read_section_db(std::istream & infile) -> section_map {
   section_map section_db;
   while (infile) {
      ws(infile);
      std::string line;
      getline(infile, line);
      if (!line.empty()) {
         // get [x.x....] symbolic tag 
         assert(line.back() == ']');
         auto p = line.rfind('[');
         assert(p != std::string::npos);
         section_tag tag;
         tag.name = line.substr(p);   // save [x.x....] symbolic tag
         assert(tag.name.size() > 2);
         assert(tag.name[0] == '[');
         assert(tag.name[tag.name.size()-1] == ']');
         tag.name.erase(0, 1);  // erase '[' from name
         tag.name.erase(tag.name.size() - 1);  // erase ']' from name
         line.erase(p-1);    // remove symbolic tag from line

         // get the prefix if any
         section_num num;
         while (!line.empty() && std::isspace(line[0]))
           line.erase(0,1);
         if (line.size() > 1 && std::isalpha(line[0])
           && line[1] != ' ' && line[1] != '.') // not an annex
         {
           std::string::size_type end;
           if ((end = line.find(' ')) != std::string::npos) {
             num.prefix = line.substr(0, end);  // save prefix
             line.erase(0, end+1);  // remove prefix + trailing space
           }
         }
         tag.prefix = num.prefix;

         // save [n.n....] numeric tag
         std::istringstream temp(line);
         if (!std::isdigit(line[0])) {
            char c;
            temp >> c;
            num.num.push_back(100 + c - 'A');
            temp >> c;
         }

         while (temp) {
            int n;
            temp >> n;
            if (!temp.fail()) {
               num.num.push_back(n);
               char dot;
               temp >> dot;
            }
         }
//         std::cout << "tag=\"" << tag.prefix << "\", \"" << tag.name << "\"\n";
//         std::cout << "num=\"" << num.prefix << "\", \"" << num.num[0] << "\"\n";
         section_db[tag] = num;  // stuff tag / num pair into section database
      }
   }
   return section_db;
}

auto lwg::format_section_tag_as_link(section_map & section_db, section_tag const & tag) -> std::string {
   std::ostringstream o;
   const auto& num = section_db[tag];
   o << num << ' ';
   std::string url;
   if  (!tag.prefix.empty()) {
      std::string_view fund_ts = "fund.ts";
      if (tag.prefix.starts_with(fund_ts)) {
         std::string_view version = tag.prefix;
         version.remove_prefix(fund_ts.size());
         if (version.empty())
            version = "v1";
         else // Should be in the form "fund.ts.v2"
            version.remove_prefix(1);
         assert(version.size() == 2 && version[0] == 'v');
         url = "https://cplusplus.github.io/fundamentals-ts/";
         url += version;
         url += ".html#";
         url += tag.name;
      }
   }
   else if (!num.num.empty() && num.num.front() != 99) {
      url = "https://wg21.link/" + tag.name;
   }

   if (url.empty())
      o << tag;
   else
      o << "<a href=\"" << url << "\">" << tag << "</a>";
   return o.str();
}

