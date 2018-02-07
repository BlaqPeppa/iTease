#pragma once
#include "common.h"
#include "module.h"
#include <gumbo.h>

namespace iTease {
	class HTMLDocument;
	class HTMLNode;
	class HTMLSelector;

	class HTML : public Module {
	public:
		HTML() : Module("html")
		{ }

		virtual void init_module(Application&) override;
		virtual void init_js(JS::Context&) override;
	};

	enum class HTMLNodeType {
		Document, Element, Text, CDATA, Comment, Whitespace, Template
	};

	class HTMLSelector : public enable_shared_from_this<HTMLSelector> {
	public:
		enum class Operator {
			Dummy, Empty, OnlyChild, NthChild, Tag
		};

	public:
		HTMLSelector(Operator op = Operator::Dummy);
		HTMLSelector(bool ofType);
		HTMLSelector(unsigned int a, unsigned int b, bool last, bool ofType);
		HTMLSelector(GumboTag tag);
		virtual ~HTMLSelector();

		virtual auto match(GumboNode* node)->bool;
		auto filter(vector<GumboNode*> nodes)->vector<GumboNode*>;
		auto match_all(GumboNode* node)->vector<GumboNode*>;

	private:
		void init() {
			m_ofType = false;
			m_a = 0;
			m_b = 0;
			m_last = false;
			m_tag = GumboTag(0);
		}
		void match_all_into(GumboNode* node, vector<GumboNode*>& nodes);

	private:
		Operator m_op;
		GumboTag m_tag;
		unsigned int m_a = 0;
		unsigned int m_b = 0;
		bool m_last = false;
		bool m_ofType = false;
	};
	class HTMLUnarySelector : public HTMLSelector {
	public:
		enum class Operator {
			Not, HasDescendant, HasChild
		};

	public:
		HTMLUnarySelector(Operator op, shared_ptr<HTMLSelector> sel);
		virtual ~HTMLUnarySelector();

	public:
		virtual auto match(GumboNode* node)->bool;

	private:
		auto has_descendant_match(GumboNode* node, shared_ptr<HTMLSelector> sel)->bool;
		auto has_child_match(GumboNode* node, shared_ptr<HTMLSelector> sel)->bool;

	private:
		shared_ptr<HTMLSelector> m_sel;
		Operator m_op;
	};
	class HTMLBinarySelector : public HTMLSelector {
	public:
		enum class Operator {
			Union, Intersection, Child, Descendant, Adjacent
		};

	public:
		HTMLBinarySelector(Operator op, shared_ptr<HTMLSelector> pS1, shared_ptr<HTMLSelector> pS2);
		HTMLBinarySelector(shared_ptr<HTMLSelector> pS1, shared_ptr<HTMLSelector> pS2, bool adjacent);
		virtual ~HTMLBinarySelector();

	public:
		virtual auto match(GumboNode* node)->bool;

	private:
		shared_ptr<HTMLSelector> m_sel1;
		shared_ptr<HTMLSelector> m_sel2;
		Operator m_op;
		bool m_adjacent;
	};
	class HTMLAttributeSelector : public HTMLSelector {
	public:
		enum class Operator {
			Exists, Equals, Includes, DashMatch, Prefix, Suffix, SubString, RegexMatch
		};

	public:
		HTMLAttributeSelector(Operator op, string key, string value = "");
		virtual ~HTMLAttributeSelector();

	public:
		virtual auto match(GumboNode* node)->bool;

	private:
		string m_key;
		string m_value;
		Operator m_op;
	};
	class HTMLTextSelector : public HTMLSelector {
	public:
		enum class Operator {
			OwnContains, Contains, OwnRegexMatch, RegexMatch
		};

	public:
		HTMLTextSelector(Operator op, string value = "");
		virtual ~HTMLTextSelector();

	public:
		virtual auto match(GumboNode* node)->bool;

	private:
		string m_value;
		Operator m_op;
	};
	
	class HTMLParserError : public std::runtime_error {
	public:
		HTMLParserError(const char* msg) : runtime_error(msg)
		{ }
		HTMLParserError(const string& msg) : runtime_error(msg)
		{ }
	};

	class HTMLParser {
	private:
		HTMLParser(string src);

	public:
		static auto create(string src)->shared_ptr<HTMLSelector>;

	private:
		auto parse_selector_group()->shared_ptr<HTMLSelector>;
		auto parse_selector()->shared_ptr<HTMLSelector>;
		auto parse_simple_selector_sequence()->shared_ptr<HTMLSelector>;
		auto parse_nth(int& aA, int& aB)->void;
		auto parse_integer()->int;
		auto parse_pseudoclass_selector()->shared_ptr<HTMLSelector>;
		auto parse_attribute_selector()->shared_ptr<HTMLSelector>;
		auto parse_class_selector()->shared_ptr<HTMLSelector>;
		auto parse_id_selector()->shared_ptr<HTMLSelector>;
		auto parse_type_selector()->shared_ptr<HTMLSelector>;
		auto consume_until_escapable_parenthesis(string& out, char openParen = '(', char closeParen = ')')->bool;
		auto consume_closing_parenthesis()->bool;
		auto consume_parenthesis()->bool;
		auto skip_whitespace()->bool;
		auto parse_string()->string;
		auto parse_name()->string;
		auto parse_identifier()->string;
		auto name_char(char c)->bool;
		auto name_start(char c)->bool;
		auto hex_digit(char c)->bool;
		auto parse_escape()->string;
		auto error(string message)->HTMLParserError;

	private:
		string m_src;
		size_t m_offset = 0;
	};

	class HTMLQuery {
	public:
		static constexpr const char* JSName = "\xff""\xff""HTMLQuery";

		void prototype(JS::Context& js) {
			JS::StackAssert sa(js, 1);

			// return prototype
			js.get_global<void>("\xff""\xff""module-html");
			js.get_property<void>(-1, JSName);
			js.remove(-2);
		}

	public:
		HTMLQuery(HTMLNode node);
		HTMLQuery(shared_ptr<const HTMLDocument> doc, GumboNode* node);
		HTMLQuery(shared_ptr<const HTMLDocument> doc, vector<GumboNode*> nodes);
		~HTMLQuery();

		auto at(size_t i) const->HTMLNode;
		auto size() const->size_t;
		auto begin()->vector<GumboNode*>::iterator;
		auto end()->vector<GumboNode*>::iterator;
		auto find(string_view selector) const->HTMLQuery;
		auto doc() const->shared_ptr<const HTMLDocument> { return m_doc; }

	private:
		shared_ptr<const HTMLDocument> m_doc;
		vector<GumboNode*> m_nodes;
	};

	class HTMLNode {
		friend class HTMLQuery;

	public:
		static constexpr const char* JSName = "\xff""\xff""HTMLNode";

	public:
		HTMLNode();
		HTMLNode(GumboNode*);
		HTMLNode(shared_ptr<const HTMLDocument>, GumboNode*);
		virtual ~HTMLNode() { }

		auto ok() const { return m_node != nullptr; }
		auto tag() const->string;
		auto name() const->string;
		auto text() const->string;
		auto attr(string_view) const->optional<string>;
		auto attributes() const->map<string, string>;
		auto parent() const { return HTMLNode{m_doc, m_node->parent}; }
		auto index() const { return m_node->index_within_parent; }
		auto size() const { return m_node->type == GUMBO_NODE_ELEMENT ? m_node->v.element.children.length : 0; }
		auto child(size_t i) const->HTMLNode;
		auto next() const { return HTMLNode{parent().child(index() + 1)}; }
		auto prev() const { return HTMLNode{parent().child(index() - 1)}; }
		auto start_pos() const->size_t;
		auto end_pos() const->size_t;
		auto start_pos_outer() const->size_t;
		auto end_pos_outer() const->size_t;
		auto find(string_view) const->HTMLQuery;
		auto type() const->HTMLNodeType;
		virtual string_view html() const;

		operator bool() const { return ok(); }
		auto operator==(const HTMLNode& other) const { return m_node == other.m_node; }
		auto operator!=(const HTMLNode& other) const { return !(*this == other); }

		auto operator*() const { return *this; }
		auto& operator++() {
			if (ok()) *this = next();
			return *this;
		}
		auto& operator++(int) {
			auto copy(*this);
			if (ok()) *this = next();
			return copy;
		}
		auto& operator--() {
			if (ok()) *this = prev();
			return *this;
		}
		auto& operator--(int) {
			auto copy(*this);
			if (ok()) *this = prev();
			return copy;
		}

	protected:
		shared_ptr<const HTMLDocument> m_doc;
		GumboNode* m_node = nullptr;
	};
	class HTMLElement : public HTMLNode {
	public:
		HTMLElement() : HTMLNode()
		{ }
		HTMLElement(GumboNode* node) : HTMLNode(node)
		{ }
		HTMLElement(shared_ptr<const HTMLDocument> doc, GumboNode* node) : HTMLNode(doc, node)
		{ }
	};

	class HTMLDocument : public enable_shared_from_this<HTMLDocument>, public HTMLNode {
	public:
		HTMLDocument() = default;
		HTMLDocument(string source);
		~HTMLDocument();

		inline operator bool() const { return m_output != nullptr; }

		auto root() const->HTMLQuery;
		auto find(const string& selector) const->HTMLQuery;
		auto load(string_view src)->void;
		auto output() const->GumboOutput*;
		auto reset()->void;
		virtual string_view html() const override { return m_source; }

	private:
		GumboOutput* m_output = nullptr;
		string m_source;
	};

	namespace JS {
		template<>
		class TypeInfo<HTMLNode> {
		public:
			static void apply(Context& js, const HTMLNode& node) {
				StackAssert sa(js);
				js.put_properties(-1,
					Property<string>{"tagName", node.tag()},
					Property<string>{"nodeName", node.name()},
					Property<string>{"textContent", node.text()},
					Property<string>{"innerHTML", string{node.html()}},
					Property<unsigned int>{"childElementCount", node.size()}
				);

				js.push(JS::Array{});
				//for (int i=0; i<node)

				// this.prototype = html.__HTMLQuery
				js.get_global<void>("\xff""\xff""module-html");
				js.get_property<void>(-1, HTMLNode::JSName);
				js.set_prototype(-3);
				js.pop();
			}
			static void push(Context& js, const HTMLNode& node) {
				StackAssert sa(js, 1);
				js.push(JS::Object{ });
				apply(js, node);
			}
			static void construct(Context& js, const HTMLNode& node) {
				StackAssert sa(js);
				js.push_this_object();
				apply(js, node);
				js.pop();
			}
		};

		template<>
		class TypeInfo<HTMLQuery> {
		public:
			static void apply(Context& js, const HTMLQuery& query) {
				StackAssert sa(js);
				auto qry = std::make_shared<HTMLQuery>(query);
				js.put_properties(-1,
					Property<unsigned int>{"length", qry->size()}
				);

				//for (size_t i = 0; i < query.size(); ++i) {
				int i = 0;
				for (auto q : *qry) {
					js.put_property(-1, i++, HTMLNode{qry->doc(), q});
				}
			}
			static void push(Context& js, const HTMLQuery& doc) {
				StackAssert sa(js, 1);
				js.push(JS::Object{ });
				apply(js, doc);
			}
			static void construct(Context& js, const HTMLQuery& doc) {
				StackAssert sa(js);
				js.push_this_object();
				apply(js, doc);
				js.pop();
			}
		};
	};
}