#include <iostream>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
#include <algorithm>
#include <iostream>

struct section_num
{
    std::string prefix;
    std::vector<int> num;

    auto operator<=>(const section_num&) const = default;
};

std::istream&
operator >> (std::istream& is, section_num& sn)
{
    sn.prefix.clear();
    sn.num.clear();
    ws(is);
    if (is.peek() == 'T')
    {
        is.get();
        if (is.peek() == 'R' || is.peek() == 'S')
        {
            sn.prefix = 'T';
            std::string w;
            is >> w;
            sn.prefix += w;
            ws(is);
        }
        else
        {
            sn.num.push_back(100 + 'T' - 'A');
            if (is.peek() != '.')
                return is;
             is.get();
        }
    }
    while (true)
    {
        if (std::isdigit(is.peek()))
        {
            int n;
            is >> n;
            sn.num.push_back(n);
        }
        else
        {
            char c;
            is >> c;
            sn.num.push_back(100 + c - 'A');
        }
        if (is.peek() != '.')
            break;
        char dot;
        is >> dot;
    }
    return is;
}

std::ostream&
operator << (std::ostream& os, const section_num& sn)
{
    if (!sn.prefix.empty())
        os << sn.prefix << " ";
    if (sn.num.size() > 0)
    {
        if (sn.num.front() >= 100)
            os << char(sn.num.front() - 100 + 'A');
        else
            os << sn.num.front();
        for (unsigned i = 1; i < sn.num.size(); ++i)
        {
            os << '.';
            if (sn.num[i] >= 100)
                os << char(sn.num[i] - 100 + 'A');
            else
                os << sn.num[i];
        }
    }
    return os;
}

typedef std::string section_tag;

std::string
replace_all(std::string s, const std::string& old, const std::string& nw)
{
    typedef std::string::size_type size_type;
    size_type x = s.size() - 1;
    while (true)
    {
        size_type i = s.rfind(old, x);
        if (i == std::string::npos)
            break;
        s.replace(i, old.size(), nw);
        if (i == 0)
            break;
        x = i - 1;
    }
    return s;
}

int main (int argc, char** argv)
{
    std::string prefix;
    if(argc > 1)
        prefix = std::string(argv[1]);

    std::vector<std::pair<section_num, section_tag>> v;
    while (std::cin)
    {
        section_tag t;
        std::cin >> t;
        if (std::cin.fail())
            break;
        section_num n;
        std::cin >> n;
        if (std::cin.fail())
            throw std::runtime_error("incomplete tag / num pair");
        if(!prefix.empty())
            n.prefix = prefix;

        t = replace_all(t, "&", "&amp;");
        t = replace_all(t, "<", "&lt;");
        t = replace_all(t, ">", "&gt;");
        t = '[' + t + ']';
        v.push_back({n, t});
    }
    std::sort(v.begin(), v.end());
    const std::string_view indent = "    ";
    for (auto& e : v)
    {
        const int depth = e.first.num.size() - 1;
        for (int k=0; k < depth; ++k)
            std::cout << indent;
        std::cout << e.first << ' ' << e.second << '\n';
    }
}
