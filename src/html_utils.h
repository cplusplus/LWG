#include <string>
#include <string_view>
#include <optional>

namespace lwg
{
// Replace reserved characters with entities, for use as an attribute value.
// Use for attributes like title="..." and content="...".
[[nodiscard]] inline std::string replace_reserved_char(std::string text, char c, std::string_view repl) {
   for (auto p = text.find(c); p != text.npos; p = text.find(c, p+repl.size()))
      text.replace(p, 1, repl);
   return text;
}

// Remove XML elements from the argument to just get the text nodes.
// Used to generate plain text versions of elements like <title>.
[[nodiscard]] inline std::string strip_xml_elements(std::string xml) {
   for (auto p = xml.find('<'); p != xml.npos; p = xml.find('<', p))
      xml.erase(p, xml.find('>', p) + 1 - p);
   return xml;
}

// N.B. the get_element* and get_attribute* functions expect input
// that conforms to the XML specs, not the more relaxed HTML5 spec.
// For example, element names are case-sensitive, attributes must be quoted,
// and end tags are not optional. This matches the LWG issue format.

struct xml_element
{
  std::string_view outer; // "<elem>content</elem>"
  std::string_view inner; // just "content"
};

// Find "<elem>...</elem>" in xml and return as two string views.
[[nodiscard]]
std::optional<xml_element> get_element(std::string_view elem, std::string_view xml);

// As above, but only return the inner part.
[[nodiscard]]
std::optional<std::string_view> get_element_content(std::string_view elem, std::string_view xml);

// Find attr="..." in xml and return the attribute's value
[[nodiscard]]
std::optional<std::string_view> get_attribute(std::string_view attr, std::string_view xml);

// Find <elem attr="..."> in xml and return the attribute's value
[[nodiscard]]
std::optional<std::string_view> get_attribute_of(std::string_view attr, std::string_view elem,
                                                 std::string_view xml);

struct issue;

// Create an <a> element linking to an issue
auto make_html_anchor(issue const & iss) -> std::string;

}
