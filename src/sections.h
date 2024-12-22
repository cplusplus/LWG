#ifndef INCLUDE_LWG_SECTIONS_H
#define INCLUDE_LWG_SECTIONS_H

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

namespace lwg
{

struct section_tag
{
  std::string  prefix;        // example: fund.ts.v2 
  std::string  name;          // example: meta.logical
  auto operator<=>(const section_tag&) const = default;
};

struct section_num {
  std::string       prefix;    // example: fund.ts.v2 
  std::vector<int>  num;       // sequence of numbers corresponding to section number
                               // in relevant doc, e.g,, 17.5.2.1.4.2
  auto operator<=>(const section_num&) const = default;
};

using section_map = std::map<section_tag, section_num>;

auto operator << (std::ostream & os,
  section_tag const & tag) -> std::ostream &; // with square brackets
std::string as_string(section_tag const & x); // without square brackets

auto operator >> (std::istream & is, section_num & sn) -> std::istream &;
auto operator << (std::ostream & os, section_num const & sn) -> std::ostream &;

auto read_section_db(std::istream & stream) -> section_map;
   // Read the current C++ standard tag -> section number index
   // from the specified 'stream', and return it as a new
   // 'section_map' object.

auto format_section_tag_as_link(section_map & section_db, section_tag const & tag) -> std::string;

} // close namespace lwg


#endif // INCLUDE_LWG_SECTIONS_H
