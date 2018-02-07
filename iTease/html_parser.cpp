#include "stdinc.h"
#include "common.h"
#include "html.h"

using namespace iTease;

HTMLParser::HTMLParser(string src) : m_src(src)
{ }
auto HTMLParser::create(string src)->shared_ptr<HTMLSelector> {
	auto parser = std::make_shared<HTMLParser>(HTMLParser{src});
	return parser->parse_selector_group();
}
auto HTMLParser::parse_selector_group()->shared_ptr<HTMLSelector> {
	auto ret = parse_selector();
	while (m_offset < m_src.size()) {
		if (m_src[m_offset] != ',')
			return ret;

		m_offset++;

		auto sel = parse_selector();
		auto oldRet = ret;
		ret = std::make_shared<HTMLBinarySelector>(HTMLBinarySelector::Operator::Union, ret, sel);
	}
	return ret;
}
auto HTMLParser::parse_selector()->shared_ptr<HTMLSelector> {
	skip_whitespace();
	auto ret = parse_simple_selector_sequence();
	while (true) {
		char combinator = 0;
		if (skip_whitespace())
			combinator = ' ';

		if (m_offset >= m_src.size()) 
			return ret;

		char c = m_src[m_offset];
		if (c == '+' || c == '>' || c == '~') {
			combinator = c;
			m_offset++;
			skip_whitespace();
		}
		else if (c == ',' || c == ')')
			return ret;

		if (combinator == 0)
			return ret;

		auto sel = parse_simple_selector_sequence();
		if (combinator == ' ')
			ret = std::make_shared<HTMLBinarySelector>(HTMLBinarySelector::Operator::Descendant, ret, sel);
		else if (combinator == '>')
			ret = std::make_shared<HTMLBinarySelector>(HTMLBinarySelector::Operator::Child, ret, sel);
		else if (combinator == '+')
			ret = std::make_shared<HTMLBinarySelector>(ret, sel, true);
		else if (combinator == '~')
			ret = std::make_shared<HTMLBinarySelector>(ret, sel, true);
		else
			throw error("impossible");
	}
	return ret;
}
auto HTMLParser::parse_simple_selector_sequence()->shared_ptr<HTMLSelector> {
	shared_ptr<HTMLSelector> ret;
	if (m_offset >= m_src.size())
		throw error("expected selector, found EOF instead");

	char c = m_src[m_offset];
	if (c == '*')
		m_offset++;
	else if (c != '#' && c != '.' && c != '[' && c != ':')
		ret = parse_type_selector();

	while (m_offset < m_src.size()) {
		char c = m_src[m_offset];
		shared_ptr<HTMLSelector> sel;
		if (c == '#')
			sel = parse_id_selector();
		else if (c == '.')
			sel = parse_class_selector();
		else if (c == '[')
			sel = parse_attribute_selector();
		else if (c == ':')
			sel = parse_pseudoclass_selector();
		else
			break;

		ret = ret ? std::make_shared<HTMLBinarySelector>(HTMLBinarySelector::Operator::Intersection, ret, sel) : sel;
	}
	return ret ? ret : std::make_shared<HTMLSelector>();
}
auto HTMLParser::parse_nth(int& a, int& b)->void {
	auto eof = [this]() {
		throw error("unexpected EOF while attempting to parse expression of form an+b");
	};
	auto invalid = [this]() {
		throw error("unexpected character while attempting to parse expression of form an+b");
	};
	auto readN = [this, eof, &b]() {
		skip_whitespace();
		if (m_offset >= m_src.size()) eof();

		char c = m_src[m_offset];
		if (c == '+') {
			m_offset++;
			skip_whitespace();
			b = parse_integer();
			return;
		}
		else if (c == '-') {
			m_offset--;
			skip_whitespace();
			b = -parse_integer();
			return;
		}
		else {
			b = 0;
			return;
		}
	};
	auto readA = [this, readN, eof, &a, &b] {
		if (m_offset >= m_src.size()) eof();

		char c = m_src[m_offset];
		if (c == 'n' || c == 'N') {
			m_offset++;
			readN();
		}
		else {
			b = a;
			a = 0;
			return;
		}
	};
	auto positiveA = [this, &a, readA, readN, eof, invalid]() {
		if (m_offset >= m_src.size()) eof();
		char c = m_src[m_offset];
		if (c >= '0' && c <= '9') {
			a = parse_integer();
			readA();
		}
		else if (c == 'n' || c == 'N') {
			a = 1;
			m_offset++;
			readN();
		}
		else invalid();
	};
	auto negativeA = [this, &a, readA, readN, eof, invalid]() {
		if (m_offset >= m_src.size()) eof();
		char c = m_src[m_offset];
		if (c >= '0' && c <= '9') {
			a = -parse_integer();
			readA();
		}
		else if (c == 'n' || c == 'N') {
			a = -1;
			m_offset++;
			readN();
		}
		else invalid();
	};

	if (m_offset >= m_src.size())
		eof();

	char c = m_src[m_offset];
	char lc = std::isalpha(c) ? std::tolower(c) : 0;
	if (c == '-') {
		m_offset++;
		negativeA();
	}
	else if (c == '+') {
		m_offset++;
		positiveA();
	}
	else if (c >= '0' && c <= '9') {
		positiveA();
	}
	else if (lc) {
		if (c == 'n' || c == 'N') {
			readN();
		}
		else if (c == 'o' || c == 'O' || c == 'e' || c == 'E') {
			auto id = strtolower(parse_name());
			if (id == "odd") {
				a = 2;
				b = 1;
			}
			else if (id == "even") {
				a = 2;
				b = 0;
			}
			else throw error("expected 'odd' or 'even', invalid found");
		}
		else invalid();
	}
	else invalid();
}
auto HTMLParser::parse_integer()->int {
	size_t offset = m_offset;
	int i = 0;
	for (; offset < m_src.size(); offset++) {
		char c = m_src[offset];
		if (c < '0' || c > '9') {
			break;
		}
		i = i * 10 + c - '0';
	}

	if (offset == m_offset)
		throw error("expected integer, but didn't find it.");

	m_offset = offset;
	return i;
}
auto HTMLParser::parse_pseudoclass_selector()->shared_ptr<HTMLSelector> {
	if (m_offset >= m_src.size() || m_src[m_offset] != ':')
		throw error("expected pseudoclass selector (:pseudoclass), found invalid char");

	m_offset++;
	string name = parse_identifier();
	name = strtolower(name);
	if (name == "not" || name == "has" || name == "haschild") {
		if (!consume_parenthesis())
			throw error("expected '(' but didn't find it");

		auto sel = parse_selector_group();
		if (!consume_closing_parenthesis())
			throw error("expected ')' but didn't find it");

		HTMLUnarySelector::Operator op;
		if (name == "not")
			op = HTMLUnarySelector::Operator::Not;
		else if (name == "has")
			op = HTMLUnarySelector::Operator::HasDescendant;
		else if (name == "haschild")
			op = HTMLUnarySelector::Operator::HasChild;
		else
			throw error("impossbile");
		return std::make_shared<HTMLUnarySelector>(op, sel);
	}
	else if (name == "contains" || name == "containsown") {
		if (!consume_parenthesis() || m_offset >= m_src.size())
			throw error("expected '(' but didn't find it");

		string value;
		char c = m_src[m_offset];
		if (c == '\'' || c == '"')
			value = parse_string();
		else
			value = parse_identifier();

		value = strtolower(value);
		skip_whitespace();

		if (!consume_closing_parenthesis())
			throw error("expected ')' but didn't find it");

		HTMLTextSelector::Operator op;
		if (name == "contains")
			op = HTMLTextSelector::Operator::Contains;
		else if (name == "containsown")
			op = HTMLTextSelector::Operator::OwnContains;
		else
			throw error("impossibile");
		return std::make_shared<HTMLTextSelector>(op, value);
	}
	else if (name == "matches" || name == "matchesown") {
		if (!consume_parenthesis() || m_offset >= m_src.size())
			throw error("expected '(' but didn't find it");

		string pattern;
		if (!consume_until_escapable_parenthesis(pattern))
			throw error("expected ')' but didn't find it");
		
		try {
			regex expr{pattern};
		}
		catch (const std::regex_error& ex) {
			throw error("regex error: "s + ex.what());
		}

		HTMLTextSelector::Operator op;
		if (name == "matches")
			op = HTMLTextSelector::Operator::RegexMatch;
		else if (name == "matches")
			op = HTMLTextSelector::Operator::RegexMatch;
		else
			throw error("impossible");
		
		return std::make_shared<HTMLTextSelector>(op, pattern);
	}
	else if (name == "nth-child" || name == "nth-last-child" || name == "nth-of-type" || name == "nth-last-of-type") {
		if (!consume_parenthesis())
			throw error("expected '(' but didn't find it");

		int a, b;
		parse_nth(a, b);

		if (!consume_closing_parenthesis())
			throw error("expected ')' but didn't find it");

		return std::make_shared<HTMLSelector>(a, b, name == "nth-last-child" || name == "nth-last-of-type", name == "nth-of-type" || name == "nth-last-of-type");
	}
	else if (name == "first-child")
		return std::make_shared<HTMLSelector>(0, 1, false, false);
	else if (name == "last-child")
		return std::make_shared<HTMLSelector>(0, 1, true, false);
	else if (name == "first-of-type")
		return std::make_shared<HTMLSelector>(0, 1, false, true);
	else if (name == "last-of-type")
		return std::make_shared<HTMLSelector>(0, 1, true, true);
	else if (name == "only-child")
		return std::make_shared<HTMLSelector>(false);
	else if (name == "only-of-type")
		return std::make_shared<HTMLSelector>(true);
	else if (name == "empty")
		return std::make_shared<HTMLSelector>(HTMLSelector::Operator::Empty);
	throw error("unsupported op:" + name);
}
auto HTMLParser::parse_attribute_selector()->shared_ptr<HTMLSelector> {
	if (m_offset >= m_src.size() || m_src[m_offset] != '[')
		throw error("expected attribute selector ([attribute]), found invalid char");

	m_offset++;
	skip_whitespace();

	string key = parse_identifier();
	skip_whitespace();
	if (m_offset >= m_src.size()) {
		throw error("unexpected EOF in attribute selector");
	}

	if (m_src[m_offset] == ']') {
		m_offset++;
		return std::make_shared<HTMLAttributeSelector>(HTMLAttributeSelector::Operator::Exists, key);
	}

	if (m_offset + 2 > m_src.size()) {
		throw error("unexpected EOF in attribute selector");
	}

	string op = m_src.substr(m_offset, 2);
	if (op[0] == '=') {
		op = "=";
	}
	else if (op[1] != '=') {
		throw error("expected equality operator, found invalid char");
	}

	m_offset += op.size();
	skip_whitespace();
	if (m_offset >= m_src.size()) {
		throw error("unexpected EOF in attribute selector");
	}

	string value;
	if (op == "#=") {
		//TODo
		while (m_src[m_offset] != ']')
			value.push_back(m_src[m_offset]);
		try {
			regex reg(value);
		}
		catch (const std::regex_error& ex) {
			throw error("regex error: "s + ex.what());
		}
	}
	else {
		char c = m_src[m_offset];
		if (c == '\'' || c == '"')
			value = parse_string();
		else
			value = parse_identifier();
	}
	skip_whitespace();
	if (m_offset >= m_src.size() || m_src[m_offset] != ']') {
		throw error("expected attribute selector ([attribute]), found invalid char");
	}
	++m_offset;

	HTMLAttributeSelector::Operator aop;
	if (op == "=")
		aop = HTMLAttributeSelector::Operator::Equals;
	else if (op == "~=")
		aop = HTMLAttributeSelector::Operator::Includes;
	else if (op == "|=")
		aop = HTMLAttributeSelector::Operator::DashMatch;
	else if (op == "^=")
		aop = HTMLAttributeSelector::Operator::Prefix;
	else if (op == "$=")
		aop = HTMLAttributeSelector::Operator::Suffix;
	else if (op == "*=")
		aop = HTMLAttributeSelector::Operator::SubString;
	else if (op == "#=")
		aop = HTMLAttributeSelector::Operator::RegexMatch;
	else
		throw error("unsupported op:" + op);
	return std::make_shared<HTMLAttributeSelector>(aop, key, value);
}
auto HTMLParser::parse_class_selector()->shared_ptr<HTMLSelector> {
	if (m_offset >= m_src.size() || m_src[m_offset] != '.')
		throw error("expected class selector (.class), found invalid char");

	m_offset++;
	return std::make_shared<HTMLAttributeSelector>(HTMLAttributeSelector::Operator::Includes, "class", parse_identifier());
}
auto HTMLParser::parse_id_selector()->shared_ptr<HTMLSelector> {
	if (m_offset >= m_src.size() || m_src[m_offset] != '#') {
		throw error("expected id selector (#id), found invalid char");
	}
	m_offset++;
	return std::make_shared<HTMLAttributeSelector>(HTMLAttributeSelector::Operator::Equals, "id", parse_name());
}
auto HTMLParser::parse_type_selector()->shared_ptr<HTMLSelector> {
	string tag = parse_identifier();
	return std::make_shared<HTMLSelector>(gumbo_tag_enum(tag.c_str()));
}
auto HTMLParser::consume_until_escapable_parenthesis(string& out, char openParen, char closeParen)->bool {
	int depth = 0;
	bool bslash = false;
	auto offset = m_offset;
	while (m_offset < m_src.size()) {
		auto c = m_src[m_offset];
		if (c == '\\' && !bslash) {
			bslash = true;
		}
		else {
			if (bslash) bslash = false;
			else {
				if (c == closeParen) {
					if (!depth) break;
					--depth;
				}
				else if (c == openParen)
					++depth;
			}
			out.push_back(c);
		}
	}
	return m_offset < m_src.size() && m_src[m_offset] == closeParen;
}
auto HTMLParser::consume_closing_parenthesis()->bool {
	size_t offset = m_offset;
	skip_whitespace();
	if (m_offset < m_src.size() && m_src[m_offset] == ')') {
		m_offset++;
		return true;
	}
	m_offset = offset;
	return false;
}
auto HTMLParser::consume_parenthesis()->bool {
	if (m_offset < m_src.size() && m_src[m_offset] == '(') {
		m_offset++;
		skip_whitespace();
		return true;
	}
	return false;
}
auto HTMLParser::skip_whitespace()->bool {
	size_t offset = m_offset;
	while (offset < m_src.size()) {
		char c = m_src[offset];
		if (c == ' ' || c == '\r' || c == '\t' || c == '\n' || c == '\f') {
			offset++;
			continue;
		}
		else if (c == '/') {
			if (m_src.size() > offset + 1 && m_src[offset + 1] == '*') {
				size_t pos = m_src.find("*/", offset + 2);
				if (pos != string::npos) {
					offset = pos + 2;
					continue;
				}
			}
		}
		break;
	}

	if (offset > m_offset) {
		m_offset = offset;
		return true;
	}
	return false;
}
auto HTMLParser::parse_string()->string {
	size_t offset = m_offset;
	if (m_src.size() < offset + 2)
		throw error("expected string, found EOF instead");

	string ret;
	char quote = m_src[offset];
	offset++;

	while (offset < m_src.size()) {
		char c = m_src[offset];
		if (c == '\\') {
			if (m_src.size() > offset + 1) {
				char c = m_src[offset + 1];
				if (c == '\r') {
					if (m_src.size() > offset + 2 && m_src[offset + 2] == '\n') {
						offset += 3;
						continue;
					}
				}

				if (c == '\r' || c == '\n' || c == '\f') {
					offset += 2;
					continue;
				}
			}
			m_offset = offset;
			ret += parse_escape();
			offset = m_offset;
		}
		else if (c == quote)
			break;
		else if (c == '\r' || c == '\n' || c == '\f')
			throw error("unexpected end of line in string");
		else {
			size_t start = offset;
			while (offset < m_src.size()) {
				char c = m_src[offset];
				if (c == quote || c == '\\' || c == '\r' || c == '\n' || c == '\f')
					break;

				offset++;
			}
			ret += m_src.substr(start, offset - start);
		}
	}

	if (offset >= m_src.size()) {
		throw error("EOF in string");
	}

	offset++;
	m_offset = offset;

	return ret;
}
auto HTMLParser::parse_name()->string {
	size_t offset = m_offset;
	string ret;
	while (offset < m_src.size()) {
		char c = m_src[offset];
		if (name_char(c)) {
			size_t start = offset;
			while (offset < m_src.size() && name_char(m_src[offset])) {
				offset++;
			}
			ret += m_src.substr(start, offset - start);
		}
		else if (c == '\\') {
			m_offset = offset;
			ret += parse_escape();
			offset = m_offset;
		}
		else break;
	}

	if (ret == "")
		throw error("expected name, found EOF instead");

	m_offset = offset;
	return ret;
}
auto HTMLParser::parse_identifier()->string {
	bool startingDash = false;
	if (m_src.size() > m_offset && m_src[m_offset] == '-') {
		startingDash = true;
		m_offset++;
	}

	if (m_src.size() <= m_offset)
		throw error("expected identifier, found EOF instead");

	char c = m_src[m_offset];
	if (!name_start(c) && c != '\\')
		throw error("expected identifier, found invalid char");

	string name = parse_name();
	if (startingDash) name = "-" + name;
	return name;
}
auto HTMLParser::name_char(char c)->bool {
	return name_start(c) || (c == '-') || (c >= '0' && c <= '9');
}
auto HTMLParser::name_start(char c)->bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_') || (c > 127);
}
auto HTMLParser::hex_digit(char c)->bool {
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
auto HTMLParser::parse_escape()->string {
	if (m_src.size() < m_offset + 2 || m_src[m_offset] != '\\')
		throw error("invalid escape sequence");

	size_t start = m_offset + 1;
	char c = m_src[start];
	if (c == '\r' || c == '\n' || c == '\f')
		throw error("escaped line ending outside string");

	if (!hex_digit(c)) {
		string ret = m_src.substr(start, 1);
		m_offset += 2;
		return ret;
	}

	size_t i = 0;
	string ret;
	c = 0;
	for (i = start; i < m_offset + 6 && i < m_src.size() && hex_digit(m_src[i]); i++) {
		unsigned int d = 0;
		char ch = m_src[i];
		if (ch >= '0' && ch <= '9')
			d = ch - '0';
		else if (ch >= 'a' && ch <= 'f')
			d = ch - 'a' + 10;
		else if (ch >= 'A' && ch <= 'F')
			d = ch - 'A' + 10;
		else
			throw error("impossible");

		if ((i - start) % 2) {
			c += d;
			ret.push_back(c);
			c = 0;
		}
		else c += (d << 4);
	}

	if (ret.size() == 0 || c != 0)
		throw error("invalid hex digit");

	if (m_src.size() > i) {
		switch (m_src[i]) {
		case '\r':
			i++;
			if (m_src.size() > i && m_src[i] == '\n') {
				i++;
			}
			break;
		case ' ':
		case '\t':
		case '\n':
		case '\f':
			i++;
			break;
		}
	}

	m_offset = i;
	return ret;
}
auto HTMLParser::error(string msg)->HTMLParserError {
	size_t d = m_offset;
	string ds;
	if (d == 0) {
		ds = '0';
	}

	while (d) {
		ds.push_back(d % 10 + '0');
		d /= 10;
	}

	string ret = msg + " at:";
	for (string::reverse_iterator rit = ds.rbegin(); rit != ds.rend(); ++rit) {
		ret.push_back(*rit);
	}
	return ret;
}