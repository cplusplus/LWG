#include "html_utils.h"
#include <format>

namespace lwg
{
constexpr std::string_view xml_whitespace = " \t\n\r";

inline bool is_whitespace(char c)
{
  return xml_whitespace.find(c) != xml_whitespace.npos;
}

// Find the offset of the first <elem> in xml.
std::string_view::size_type
find_element(std::string_view elem, std::string_view xml)
{
  if (elem.empty()) [[unlikely]]
    return xml.npos;

  std::string start_tag = std::format("<{}", elem);
  std::string_view::size_type pos = 0;
  while ((pos = xml.find(start_tag, pos)) != xml.npos)
  {
    if ((pos + start_tag.size()) == xml.size()) [[unlikely]]
      return xml.npos;

    if (char c = xml[pos + start_tag.size()]; c == '>' or is_whitespace(c) or c == '/')
      return pos;
    pos += start_tag.size();
  }
  return pos;
}

// FIXME: This cannot handle "<p> <p>nested</p> </p>",
// it returns up to the first "</p>" instead of the second.
std::optional<xml_element>
get_element(std::string_view elem, std::string_view xml)
{
  auto pos = find_element(elem, xml); // Find "<elem"

  if (pos == xml.npos) [[unlikely]]
    return std::nullopt;

  xml.remove_prefix(pos); // Remove everything before "<elem"

  pos = xml.find('>');
  if (pos == xml.npos) [[unlikely]]
    return std::nullopt;

  // Check for self-closing tag, e.g. "<p/>"
  // EmptyElemTag ::= '<' Name (S Attribute)* S? '/>'
  if (xml[pos - 1] == '/')
    return xml_element{xml.substr(0, pos + 1), {}};

  ++pos;
  // N.B. we assume no whitespace in end tag, i.e. "</elem>" not "</elem >"
  // although the XML spec does allow it: ETag ::= '</' Name S? '>'
  std::string end_tag = std::format("</{}>", elem);
  auto pos2 = xml.find(end_tag, pos);
  if (pos2 == xml.npos) [[unlikely]]
    return std::nullopt;

  return xml_element{xml.substr(0, pos2 + end_tag.size()), xml.substr(pos, pos2 - pos)};
}

std::optional<std::string_view>
get_element_content(std::string_view elem, std::string_view xml)
{
  if (auto o = lwg::get_element(elem, xml)) [[likely]]
    return o->inner;
  return std::nullopt;
}

std::optional<std::string_view>
get_attribute(std::string_view attr, std::string_view xml)
{
  if (attr.empty() or xml.empty()) [[unlikely]]
    return std::nullopt;

  auto pos = xml.npos;

  while (true)
  {
    pos = xml.find(attr, pos + 1);
    if (pos == xml.npos) [[unlikely]]
      return std::nullopt;

    // Check we found the start of an attribute,
    // not e.g. the first "ref" in "<iref ref="1234"/>"
    if (pos != 0 and not is_whitespace(xml[pos-1]))
      continue;

    // find '='
    pos = xml.find_first_not_of(xml_whitespace, pos + attr.size());
    if (pos == xml.npos) [[unlikely]]
      return std::nullopt;

    if (xml[pos] != '=') [[unlikely]]
      continue;

    // find opening quote
    pos = xml.find_first_not_of(xml_whitespace, pos + 1);
    if (pos == xml.npos) [[unlikely]]
      return std::nullopt;
    const char q = xml[pos];
    if (q != '"' and q != '\'') [[unlikely]]
      continue;

    xml.remove_prefix(pos + 1);
    // Find closing quote
    pos = xml.find(q);
    if (pos == xml.npos) [[unlikely]]
      return std::nullopt;

    return xml.substr(0, pos);
  }
}

std::optional<std::string_view>
get_attribute_of(std::string_view attr, std::string_view elem, std::string_view xml)
{
  std::string_view::size_type pos = 0;
  while ((pos = find_element(elem, xml)) != xml.npos)
  {
    xml.remove_prefix(pos);
    auto start_tag = xml.substr(0, xml.find('>', elem.size() + 1) + 1);
    if (auto o = get_attribute(attr, start_tag))
      return o;
    xml.remove_prefix(start_tag.size());
  }
  return std::nullopt;
}

} // namespace lwg

#ifdef SELF_TEST
#include <cassert>
int main()
{
  std::string_view xml = R"(
<xml>
  <elem>content <x/> ...</elem>
  <elt attr="foo" attr2="bar" />
  <elt attr3="three" />
  <el attr="baz">...</el>
  <p>para <p/>another para</p>
  <blockquote>quote <blockquote>nested quote</blockquote></blockquote>
  <new
line = "ok"
	 lines 
 =
"also ok"/>
  <elem_not_attr_still_not_attr nattr="that" attrattr ="that" attr="this"/>
  <text> not_really_an_attr = "fooled you" </text>
  <quotes single='1' double="2"/>
</xml>)";

  // Empty arguments should fail:
  assert( not lwg::get_element("", xml) );
  assert( not lwg::get_element("elem", "") );
  assert( not lwg::get_attribute("", xml) );
  assert( not lwg::get_attribute("attr", "") );
  assert( not lwg::get_element("elem", "<elem") );

  // Non-existent elements and attributes should fail:
  assert(not lwg::get_element_content("nonesuch", xml));
  assert(not lwg::get_attribute("nonesuch", xml));
  assert(not lwg::get_attribute_of("attr", "nonesuch", xml));

  assert(lwg::get_element("elem", xml)->outer == "<elem>content <x/> ...</elem>");
  assert(lwg::get_element_content("elem", xml) == "content <x/> ...");
  assert(lwg::get_element("x", xml)->outer == "<x/>");
  assert(lwg::get_element_content("x", xml) == "");
  assert(lwg::get_element("elt", xml)->outer == "<elt attr=\"foo\" attr2=\"bar\" />");
  assert(lwg::get_element_content("elt", xml) == "");
  assert(lwg::get_element_content("p", xml) == "para <p/>another para");
  // FIXME: assert(lwg::get_element_content("blockquote", xml) == "quote <blockquote>nested quote</blockquote>");
  assert(lwg::get_attribute("attr", xml) == "foo");
  assert(lwg::get_attribute("attr3", xml) == "three");
  assert(lwg::get_attribute_of("attr3", "elt", xml) == "three");
  assert(lwg::get_attribute_of("attr", "el", xml) == "baz");
  assert(not lwg::get_attribute_of("attr2", "el", xml));
  assert(not lwg::get_attribute("tr2", xml));
  assert(lwg::get_attribute("line", xml) == "ok");
  assert(lwg::get_attribute_of("line", "new", xml) == "ok");
  assert(lwg::get_attribute_of("lines", "new", xml) == "also ok");
  assert(lwg::get_attribute_of("attr", "elem_not_attr_still_not_attr", xml) == "this");

  // This shouldn't really match, but get_attribute matches outside start tags:
  assert(lwg::get_attribute("not_really_an_attr", xml) == "fooled you");
  // Use get_attribute_of to avoid that problem:
  assert(not lwg::get_attribute_of("not_really_an_attr", "text", xml));
  assert(not lwg::get_attribute_of("not_really_an_attr", "", xml));

  assert(lwg::get_attribute_of("single", "quotes", xml) == "1");
  assert(lwg::get_attribute_of("double", "quotes", xml) == "2");
}
#endif
