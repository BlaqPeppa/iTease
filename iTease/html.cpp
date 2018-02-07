#include "stdinc.h"
#include "html.h"
#include <gumbo/Document.h>
#include <gumbo/Node.h>

using namespace iTease;

void HTML::init_module(Application&)
{ }
void HTML::init_js(JS::Context& js) {
	using namespace std::placeholders;
	JS::StackAssert sa(js, 1);
	// html
	js.push_object(
		// html.__HTMLQuery (prototype)
		JS::Property<JS::Object>{HTMLQuery::JSName, js.push_object(
			JS::Property<bool>{HTMLQuery::JSName, true},
			JS::Property<JS::Function>{"find", JS::Function{[this](JS::Context& js)->int {
				JS::StackAssert sa(js, 1);
				auto qry = js.self<JS::Shared<HTMLQuery>>();
				js.push(qry->find(js.require<string>(0)));
				return 1;
			}, 1}},
			JS::Property<JS::Function>{"each", JS::Function{[](JS::Context& js)->int {
				JS::StackAssert sa(js, 1);
				if (!js.is<JS::Function>(0)) js.raise(JS::TypeError{"function"});
				auto qry = js.self<JS::Shared<HTMLQuery>>();
				int index = 0;
				for (auto node : *qry) {
					js.dup(0);
					js.push(HTMLQuery{node});
					js.push(index);
					++index;
					js.call_method(1);
					if (js.is<bool>(-1) && !js.get<bool>(-1))
						break;
					js.pop();
				}
				return 1;
			}, 1}}
		)},
		// html.parser
		JS::Property<JS::Object>{"parser", js.push_function_object(
			// html.parser (constructor)
			JS::Function{[this](JS::Context& js) {
				/*js.push(JS::This{});
				if (!js.is_constructor_call() || js.is<JS::Undefined>(-1)) {
					// return html.parser.call(html.parser, ...)
					js.pop();
					js.push_current_function();
					//js.dup();
					//js.insert(0);
					js.insert(0);
					//js.call_method(js.top() - 2);
					js.create(js.top() - 1);
					return 1;
				}
				js.print_dump();

				// if (!arg0) return this;
				js.dup(0);
				if (!js.to_boolean(-1)) {
					js.pop();
					js.push(JS::This{});
					return 1;
				}
				else js.pop();

				shared_ptr<HTMLDocument> doc;

				// if (this instanceof HTMLDocument)
				if (js.is<JS::Object>(-1)) {
					auto obj = js.get<JS::Object>(-1);
					if (auto ptr = JS::to_object<HTMLDocument>(obj))
						doc = ptr->shared_from_this();
				}
				js.pop();		// JS::This

				if (!doc) {
					if (js.is<string>(0))
						doc = std::make_shared<HTMLDocument>(js.get<string>(0));
					else if (js.is<JS::Object>(0)) {
						auto obj = js.get<JS::Object>(0);
						if (auto ptr = JS::to_object<HTMLDocument>(obj))
							doc = ptr->shared_from_this();
						else
							return 0;
					}
					else return 0;
					js.construct(JS::Shared<HTMLDocument>{doc});
					js.construct(*doc);
				}
				else {
					if (js.is<string>(0)) {
						js.push(doc->find(js.get<string>(0)));
						return 1;
					}
				}

				js.push(JS::This{});
				js.print_dump();*/
				return 1;
			}, 1},
			// html.parser.load(content, options)
			JS::Property<JS::Function>{"load", JS::Function{[this](JS::Context& js) {
				auto doc = std::make_shared<HTMLDocument>(js.require<string>(0));
				auto qry = std::make_shared<HTMLQuery>(doc->root());
				js.push(JS::Shared<HTMLQuery>{qry});
				JS::TypeInfo<HTMLQuery>::apply(js, *qry);
				//js.create(1);
				return 1;
			}, 2}}
		)}
	);
	// html.__HTMLDocument (prototype)
	js.push_object(
		JS::Property<bool>{HTMLDocument::JSName, true}
	);
	js.get_property<void>(-2, HTMLQuery::JSName);
	js.set_prototype(-2);
	js.put_property(-2, HTMLDocument::JSName);
	js.dup();
	js.put_global("\xff""\xff""module-html");
}

bool nodeExists(vector<GumboNode*> nodes, GumboNode* node) {
	for (vector<GumboNode*>::iterator it = nodes.begin(); it != nodes.end(); it++) {
		GumboNode* pNode = *it;
		if (pNode == node) return true;
	}
	return false;
}
vector<GumboNode*> unionNodes(vector<GumboNode*> nodes1, vector<GumboNode*> nodes2) {
	nodes1.reserve(nodes1.size() + nodes2.size());
	for (vector<GumboNode*>::iterator it = nodes2.begin(); it != nodes2.end(); it++) {
		GumboNode* node = *it;
		if (nodeExists(nodes1, node))
			continue;
		nodes1.push_back(node);
	}
	return nodes1;
}
void writeNodeText(GumboNode* node, string& out) {
	switch (node->type) {
	case GUMBO_NODE_TEXT:
		out.append(node->v.text.text);
		break;
	case GUMBO_NODE_ELEMENT: {
			GumboVector children = node->v.element.children;
			for (unsigned int i = 0; i < children.length; i++) {
				GumboNode* child = (GumboNode*)children.data[i];
				writeNodeText(child, out);
			}
		}
		break;
	default:
		break;
	}
}
string nodeText(GumboNode* node) {
	string text;
	writeNodeText(node, text);
	return text;
}
string nodeOwnText(GumboNode* node) {
	string text;
	if (node->type != GUMBO_NODE_ELEMENT)
		return text;

	GumboVector children = node->v.element.children;
	for (unsigned int i = 0; i < children.length; i++) {
		GumboNode* child = (GumboNode*)children.data[i];
		if (child->type == GUMBO_NODE_TEXT) {
			text.append(child->v.text.text);
		}
	}

	return text;
}

HTMLSelector::HTMLSelector(Operator op) : m_op(op)
{ }
HTMLSelector::HTMLSelector(bool ofType) : m_op(Operator::OnlyChild), m_ofType(ofType)
{ }
HTMLSelector::HTMLSelector(unsigned int a, unsigned int b, bool last, bool ofType) : m_op(Operator::NthChild), m_a(a), m_b(b), m_last(last), m_ofType(ofType)
{ }
HTMLSelector::HTMLSelector(GumboTag tag) : m_op(Operator::Tag), m_tag(tag)
{ }
HTMLSelector::~HTMLSelector()
{ }
auto HTMLSelector::match(GumboNode* node)->bool {
	switch (m_op) {
	case Operator::Dummy:
		return true;
	case Operator::Empty: {
		if (node->type != GUMBO_NODE_ELEMENT)
			return false;

		GumboVector children = node->v.element.children;

		for (unsigned int i = 0; i < children.length; i++) {
			GumboNode* child = (GumboNode*)children.data[i];
			if (child->type == GUMBO_NODE_TEXT || child->type == GUMBO_NODE_ELEMENT)
				return false;
		}
		return true;
	}
	case Operator::OnlyChild: {
		if (node->type != GUMBO_NODE_ELEMENT)
			return false;

		GumboNode* parent = node->parent;
		if (!parent) return false;

		unsigned int count = 0;
		for (unsigned int i = 0; i < parent->v.element.children.length; i++) {
			GumboNode* child = (GumboNode*)parent->v.element.children.data[i];
			if (child->type != GUMBO_NODE_ELEMENT || (m_ofType && node->v.element.tag == child->v.element.tag))
				continue;

			count++;
			if (count > 1) return false;
		}

		return count == 1;
	}
	case Operator::NthChild: {
		if (node->type != GUMBO_NODE_ELEMENT)
			return false;

		GumboNode* parent = node->parent;
		if (!parent) return false;

		unsigned int i = 0;
		unsigned int count = 0;
		for (unsigned int j = 0; j < parent->v.element.children.length; j++) {
			GumboNode* child = (GumboNode*)parent->v.element.children.data[j];
			if (child->type != GUMBO_NODE_ELEMENT || (m_ofType && node->v.element.tag == child->v.element.tag))
				continue;

			count++;
			if (node == child) {
				i = count;
				if (!m_last) break;
			}
		}

		if (m_last)
			i = count - i + 1;

		i -= m_b;
		if (m_a == 0)
			return i == 0;
		return i % m_a == 0 && i / m_a > 0;
	}
	case Operator::Tag:
		return node->type == GUMBO_NODE_ELEMENT && node->v.element.tag == m_tag;
	default:
		return false;
	}
}
auto HTMLSelector::filter(vector<GumboNode*> nodes)->vector<GumboNode*> {
	vector<GumboNode*> ret;
	for (vector<GumboNode*>::iterator it = nodes.begin(); it != nodes.end(); it++) {
		GumboNode* n = *it;
		if (match(n)) ret.push_back(n);
	}
	return ret;
}
auto HTMLSelector::match_all(GumboNode* node)->vector<GumboNode*> {
	vector<GumboNode*> ret;
	match_all_into(node, ret);
	return ret;
}
auto HTMLSelector::match_all_into(GumboNode* node, vector<GumboNode*>& nodes)->void {
	if (match(node))
		nodes.push_back(node);

	if (node->type != GUMBO_NODE_ELEMENT)
		return;

	for (unsigned int i = 0; i < node->v.element.children.length; i++) {
		GumboNode* child = (GumboNode*)node->v.element.children.data[i];
		match_all_into(child, nodes);
	}
}
HTMLBinarySelector::HTMLBinarySelector(Operator op, shared_ptr<HTMLSelector> s1, shared_ptr<HTMLSelector> s2) {
	m_sel1 = s1;
	m_sel2 = s2;
	m_op = op;
	m_adjacent = false;
}
HTMLBinarySelector::HTMLBinarySelector(shared_ptr<HTMLSelector> s1, shared_ptr<HTMLSelector> s2, bool aAdjacent) {
	m_sel1 = s1;
	m_sel2 = s2;
	m_op = Operator::Adjacent;
	m_adjacent = aAdjacent;
}
HTMLBinarySelector::~HTMLBinarySelector()
{ }
auto HTMLBinarySelector::match(GumboNode* node)->bool {
	switch (m_op) {
	case Operator::Union:
		return m_sel1->match(node) || m_sel2->match(node);
	case Operator::Intersection:
		return m_sel1->match(node) && m_sel2->match(node);
	case Operator::Child:
		return m_sel2->match(node) && node->parent != NULL && m_sel1->match(node->parent);
	case Operator::Descendant: {
		if (!m_sel2->match(node)) 
			return false;
		for (GumboNode* p = node->parent; p != NULL; p = p->parent) {
			if (m_sel1->match(p))
				return true;
		}
		return false;
	}
	case Operator::Adjacent: {
		if (!m_sel2->match(node))
			return false;
		if (node->type != GUMBO_NODE_ELEMENT)
			return false;

		size_t pos = node->index_within_parent;
		GumboNode* parent = node->parent;
		if (m_adjacent) {
			for (long i = pos; i >= 0; i--) {
				GumboNode* sibling = (GumboNode*)parent->v.element.children.data[i];
				if (sibling->type == GUMBO_NODE_TEXT || sibling->type == GUMBO_NODE_COMMENT)
					continue;
				return m_sel1->match(sibling);
			}
			return false;
		}

		for (long i = pos; i >= 0; i--) {
			GumboNode* sibling = (GumboNode*)parent->v.element.children.data[i];
			if (m_sel1->match(sibling))
				return true;
		}
		return false;
	}
	}
	return false;
}
HTMLAttributeSelector::HTMLAttributeSelector(Operator op, string key, string value) {
	m_key = key;
	m_value = value;
	m_op = op;
}
HTMLAttributeSelector::~HTMLAttributeSelector()
{ }
auto HTMLAttributeSelector::match(GumboNode* node)->bool {
	if (node->type != GUMBO_NODE_ELEMENT)
		return false;

	GumboVector attributes = node->v.element.attributes;
	for (unsigned int i = 0; i < attributes.length; i++) {
		GumboAttribute* attr = (GumboAttribute*)attributes.data[i];
		if (m_key != attr->name)
			continue;

		string value = attr->value;
		switch (m_op) {
		case Operator::Exists:
			return true;
		case Operator::Equals:
			return m_value == value;
		case Operator::Includes:
			for (unsigned int i = 0, j = 0; i < value.size(); i++) {
				if (value[i] == ' ' || value[i] == '\t' || value[i] == '\r' || value[i] == '\n' || value[i] == '\f' || i == value.size() - 1) {
					unsigned int length = i - j;
					if (i == value.size() - 1)
						length++;
					string segment = value.substr(j, length);
					if (segment == m_value)
						return true;
					j = i + 1;
				}
			}
			return false;
		case Operator::DashMatch:
			if (m_value == value)
				return true;
			if (value.size() < m_value.size())
				return false;
			return value.substr(0, m_value.size()) == m_value && value[m_value.size()] == '-';
		case Operator::Prefix:
			return value.size() >= m_value.size() && value.substr(0, m_value.size()) == m_value;
		case Operator::Suffix:
			return value.size() >= m_value.size()
				&& value.substr(value.size() - m_value.size(), m_value.size()) == m_value;
		case Operator::SubString:
			return value.find(m_value) != std::string::npos;
		default:
			return false;
		}
	}
	return false;
}
HTMLUnarySelector::HTMLUnarySelector(Operator op, shared_ptr<HTMLSelector> sel) : m_op(op), m_sel(sel)
{ }
HTMLUnarySelector::~HTMLUnarySelector()
{ }
auto HTMLUnarySelector::has_descendant_match(GumboNode* node, shared_ptr<HTMLSelector> sel)->bool {
	for (unsigned int i = 0; i < node->v.element.children.length; i++) {
		GumboNode* child = (GumboNode*)node->v.element.children.data[i];
		if (sel->match(child) || (child->type == GUMBO_NODE_ELEMENT && has_descendant_match(child, sel)))
			return true;
	}
	return false;
}
bool HTMLUnarySelector::has_child_match(GumboNode* node, shared_ptr<HTMLSelector> sel) {
	for (unsigned int i = 0; i < node->v.element.children.length; i++) {
		GumboNode* child = (GumboNode*)node->v.element.children.data[i];
		if (sel->match(child))
			return true;
	}
	return false;
}
auto HTMLUnarySelector::match(GumboNode* node)->bool {
	switch (m_op) {
	case Operator::Not:
		return !m_sel->match(node);
	case Operator::HasDescendant:
		if (node->type != GUMBO_NODE_ELEMENT)
			return false;
		return has_descendant_match(node, m_sel);
	case Operator::HasChild:
		if (node->type != GUMBO_NODE_ELEMENT)
			return false;
		return has_child_match(node, m_sel);
	default:
		return false;
	}
}

HTMLTextSelector::HTMLTextSelector(Operator op, string value) : m_op(op), m_value(value)
{ }
HTMLTextSelector::~HTMLTextSelector()
{ }
auto HTMLTextSelector::match(GumboNode* node)->bool {
	string text;
	switch (m_op) {
	case Operator::Contains:
	case Operator::RegexMatch:
		text = nodeText(node);
		break;
	case Operator::OwnContains:
	case Operator::OwnRegexMatch:
		text = nodeOwnText(node);
		break;
	default:
		return false;
	}
	if (m_op == Operator::RegexMatch || m_op == Operator::OwnRegexMatch) {
		regex reg{m_value};
		return std::regex_match(text, reg);
	}
	text = strtolower(text);
	return text.find(m_value) != string::npos;
}
HTMLQuery::HTMLQuery(HTMLNode node) : HTMLQuery(node.m_doc, node.m_node)
{ }
HTMLQuery::HTMLQuery(shared_ptr<const HTMLDocument> doc, GumboNode* node) : m_doc(doc), m_nodes({node})
{ }
HTMLQuery::HTMLQuery(shared_ptr<const HTMLDocument> doc, vector<GumboNode*> nodes) : m_doc(doc), m_nodes(nodes)
{ }
HTMLQuery::~HTMLQuery()
{ }
auto HTMLQuery::at(size_t i) const->HTMLNode {
	return HTMLNode{m_doc, m_nodes.at(i)};
}
auto HTMLQuery::size() const->size_t {
	return m_nodes.size();
}
auto HTMLQuery::begin()->vector<GumboNode*>::iterator {
	return m_nodes.begin();
}
auto HTMLQuery::end()->vector<GumboNode*>::iterator {
	return m_nodes.end();
}
auto HTMLQuery::find(string_view selector) const->HTMLQuery {
	auto sel = HTMLParser::create(selector.data());
	vector<GumboNode*> ret;
	for (auto node : m_nodes) {
		vector<GumboNode*> matched = sel->match_all(node);
		ret = unionNodes(ret, matched);
	}
	return HTMLQuery{m_doc, ret};
}
HTMLNode::HTMLNode()
{ }
HTMLNode::HTMLNode(GumboNode* node) : m_node(node)
{ }
HTMLNode::HTMLNode(shared_ptr<const HTMLDocument> doc, GumboNode* node) : m_doc(doc), m_node(node)
{ }
auto HTMLNode::child(size_t i) const->HTMLNode {
	if (m_node->type != GUMBO_NODE_ELEMENT || i >= m_node->v.element.children.length) {
		return HTMLNode{ };
	}
	return HTMLNode{m_doc, (GumboNode*)m_node->v.element.children.data[i]};
}
auto HTMLNode::tag() const->string {
	return m_node->type != GUMBO_NODE_ELEMENT ? gumbo_normalized_tagname(m_node->v.element.tag) : "";
}
auto HTMLNode::name() const->string {
	switch (m_node->type) {
	case GUMBO_NODE_CDATA:
		return "#cdata-section";
	case GUMBO_NODE_COMMENT:
		return "#comment";
	case GUMBO_NODE_DOCUMENT:
		return "#document";
	case GUMBO_NODE_ELEMENT:
		return strtoupper(gumbo_normalized_tagname(m_node->v.element.tag));
	case GUMBO_NODE_TEMPLATE:
		break;		// ???
	case GUMBO_NODE_TEXT:
	case GUMBO_NODE_WHITESPACE:
		return "#text";
	}
	return "";
}
auto HTMLNode::text() const->string {
	string str;
	str.reserve(100);
	writeNodeText(m_node, str);
	return str;
}
auto HTMLNode::html() const->string_view {
	auto sv = m_doc->html();
	auto start = start_pos();
	return sv.substr(start, end_pos() - start);
}
auto HTMLNode::attr(string_view sv) const->optional<string> {
	if (m_node->type == GUMBO_NODE_ELEMENT) {
		GumboVector attributes = m_node->v.element.attributes;
		for (unsigned int i = 0; i < attributes.length; i++) {
			GumboAttribute* attr = (GumboAttribute*)attributes.data[i];
			if (sv == attr->name)
				return attr->value;
		}
	}
	return std::nullopt;
}
auto HTMLNode::attributes() const->map<string, string> {
	map<string, string> attrs;
	if (m_node->type == GUMBO_NODE_ELEMENT) {
		GumboVector attributes = m_node->v.element.attributes;
		for (unsigned int i = 0; i < attributes.length; i++) {
			GumboAttribute* attr = static_cast<GumboAttribute*>(attributes.data[i]);
			attrs.emplace(attr->name, attr->value);
		}
	}
	return attrs;
}
auto HTMLNode::start_pos() const->size_t {
	switch (m_node->type) {
	case GUMBO_NODE_ELEMENT:
		return m_node->v.element.start_pos.offset + m_node->v.element.original_tag.length;
	case GUMBO_NODE_TEXT:
		return m_node->v.text.start_pos.offset;
	}
	return 0;
}
auto HTMLNode::end_pos() const->size_t {
	switch (m_node->type) {
	case GUMBO_NODE_ELEMENT:
		return m_node->v.element.end_pos.offset;
	case GUMBO_NODE_TEXT:
		return m_node->v.text.original_text.length + start_pos();
	}
	return 0;
}
auto HTMLNode::start_pos_outer() const->size_t {
	switch (m_node->type) {
	case GUMBO_NODE_ELEMENT:
		return m_node->v.element.start_pos.offset;
	case GUMBO_NODE_TEXT:
		return m_node->v.text.start_pos.offset;
	}
	return 0;
}
auto HTMLNode::end_pos_outer() const->size_t {
	switch (m_node->type) {
	case GUMBO_NODE_ELEMENT:
		return m_node->v.element.end_pos.offset + m_node->v.element.original_end_tag.length;
	case GUMBO_NODE_TEXT:
		return m_node->v.text.original_text.length + start_pos();
	}
	return 0;
}
auto HTMLNode::find(string_view selector) const->HTMLQuery {
	HTMLQuery qry{m_doc, m_node};
	return qry.find(selector);
}
auto HTMLNode::type() const->HTMLNodeType {
	switch (m_node->type) {
	case GumboNodeType::GUMBO_NODE_CDATA:
		return HTMLNodeType::CDATA;
	case GumboNodeType::GUMBO_NODE_COMMENT:
		return HTMLNodeType::Comment;
	case GumboNodeType::GUMBO_NODE_DOCUMENT:
		return HTMLNodeType::Document;
	case GumboNodeType::GUMBO_NODE_ELEMENT:
		return HTMLNodeType::Element;
	case GumboNodeType::GUMBO_NODE_TEMPLATE:
		return HTMLNodeType::Template;
	case GumboNodeType::GUMBO_NODE_TEXT:
		return HTMLNodeType::Text;
	case GumboNodeType::GUMBO_NODE_WHITESPACE:
		return HTMLNodeType::Whitespace;
	}
	Unreachable();
}
HTMLDocument::HTMLDocument(string source) {
	load(source);
	m_node = m_output->root;
}
HTMLDocument::~HTMLDocument() {
	reset();
}
auto HTMLDocument::root() const->HTMLQuery {
	return {shared_from_this(), m_output->root};
}
auto HTMLDocument::find(const string& selector) const->HTMLQuery {
	if (!m_output) throw std::runtime_error("HTML document not initialized");
	HTMLQuery qry{shared_from_this(), m_output->root};
	return qry.find(selector);
}
auto HTMLDocument::load(string_view src)->void {
	reset();
	m_source = src;
	m_output = gumbo_parse(m_source.c_str());
}
auto HTMLDocument::output() const->GumboOutput* {
	return m_output;
}
auto HTMLDocument::reset()->void {
	if (m_output) {
		gumbo_destroy_output(&kGumboDefaultOptions, m_output);
		m_output = nullptr;
	}
}