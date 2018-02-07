#pragma once
#include <unordered_map>
#include "common.h"
#include "event.h"
#include "js.h"

namespace iTease {
	namespace Templating {
		struct Render {
			std::ostream& os;
			std::map<string, string> vars;

			Render(std::ostream& os) : os(os)
			{ }
		};

		class LoadError {
		public:
			LoadError(std::exception ex, long line) : m_ex(ex), m_line(line)
			{ }

		private:
			std::exception m_ex;
			long m_line;
		};

		class Block;
		class Node;
		class Conditional;

		using OnTemplateRender = const Event<Node&>;
		using OnBlockRender = const Event<Block&>;
		using OnBlockLoad = const Event<Block&>;

		enum class ElementType {
			Segment, Block, Variable, Conditional
		};

		class Element {
		public:
			Element(ElementType type) : m_type(type)
			{ }

			inline const ElementType type() const { return m_type; }
			auto is_enabled() const { return m_enabled; }
			auto enable() { m_enabled = true; }
			auto disable() { m_enabled = false; }

			virtual void render(Render) = 0;
			virtual std::shared_ptr<Element> copy() const = 0;

		private:
			ElementType m_type;
			bool m_enabled = true;
		};

		class Variable : public Element {
		public:
			Variable(string name) : Element(ElementType::Variable), m_name(name)
			{ }
			Variable(string name, string value) : Element(ElementType::Variable), m_name(name), m_value(value)
			{ }
			
			auto set(const string& value) {
				m_value = value;
			}
			auto name() const->const string& {
				return m_name;
			}
			auto value() const->const string& {
				static string empty_string;
				return m_value ? *m_value : empty_string;
			}
			auto has_value() const->bool {
				return (bool)m_value;
			}

			virtual void render(Render render) override {
				render.os << (m_value ? *m_value : "");
			}

		protected:
			virtual std::shared_ptr<Element> copy() const override {
				return std::make_unique<Variable>(*this);
			}

		private:
			string m_name;
			optional<string> m_value;
		};

		class Segment : public Element {
		public:
			Segment(string content) : Element(ElementType::Segment), m_content(content)
			{ }

			virtual auto render(Render out)->void override {
				out.os << m_content;
			}

		protected:
			virtual auto copy() const->std::shared_ptr<Element> override {
				return std::make_unique<Segment>(*this);
			}

		private:
			string m_content;
		};

		class Node : public std::enable_shared_from_this<Node> {
		public:
			using BlockIndexResult = pair<bool, std::size_t>;

		public:
			Node() = default;
			Node(Node* parent) : m_parent(parent) { }
			Node(const Node&);
			Node& operator=(const Node& v);

			// clear all elements, blocks, vars, etc.
			auto clear()->void;
			// parse template from an input stream
			auto load(std::istream&)->bool;
			// enable segments matching name
			auto enable_segment(const string&)->void;
			// disable segments matching name
			auto disable_segment(const string&)->void;
			// find a block by name
			auto block(const string&)->shared_ptr<Block>;
			// adds a block as the last element
			auto add_block(const string& name)->shared_ptr<Block>;
			// inserts a block as the first element
			auto insert_block(const string& name)->shared_ptr<Block>;
			// adds a block after another block
			auto add_block_at(const string& name, const string& after)->shared_ptr<Block>;
			// inserts a block before another block
			auto insert_block_at(const string& name, const string& before)->shared_ptr<Block>;
			// finds or adds a variable with the specified name
			auto var(const string&)->Variable&;
			// returns the value of a variable with the specified name, or empty string on failure
			auto get_var(const string&) const->string;
			// sets or adds a variable
			auto set_var(const string& name)->Variable&;
			auto set_var(const string& name, const string& value)->Variable&;
			// returns a vector containing all block pointes
			auto blocks()->vector<shared_ptr<Block>>;
			// returns a vector containing all variable pointers
			auto vars()->vector<shared_ptr<Variable>>;
			// returns a map ref of options defined in the template
			auto options() const->const std::unordered_map<string, string>&;
			// returns true if the block exists
			auto has_block(const string& name) const->bool;
			// returns true if the variable exists
			auto has_var(const string& name) const->bool;
			// returns the parent node
			auto* parent() const { return m_parent; }
			// sets the parent node
			virtual void set_parent(Node* parent);
			// updates this node based on a new child or grandchild node
			virtual void new_child(Node& child);
			//auto error() const->const string&;
			// renders the template to an output stream
			virtual void render(Render);

			// callback called when a block is about to be rendered
			OnBlockRender OnRenderBlock;
			// callback called when a descendent block is about to be rendered
			OnBlockRender OnRenderSubBlock;
			// (unused) (todo: remove or implement?)
			OnBlockLoad OnLoadBlock;

		protected:
			virtual void load_block(Block&);

		private:
			auto add_segment(const string& value, const string& segment_id = "")->void;
			auto add_variable(const string&, const string& segment_id = "")->void;
			auto add_conditional(const string& expr)->std::shared_ptr<Conditional>;

			auto insert_element_order(std::size_t at, std::size_t pos, bool before = false)->void;
			auto get_block_index(const string&) const->BlockIndexResult;

		private:
			Node* m_parent = nullptr;
			string m_error;
			bool m_enabled = false;

			// constant vector of m_elements indexes and a bool flag to enable/disable them, each element may be indexed multiple times
			vector<std::pair<bool, std::size_t>> m_index;
			// vector of m_index indexes, vector insert positions may vary
			vector<std::size_t> m_order;
			// constant vector of elements, each element will only appear once, m_index may have multiple indexes for each element
			vector<std::shared_ptr<Element>> m_elements;
			// map of m_index indexes that are segments, a segment may cover multiple elements
			std::unordered_multimap<string, std::size_t> m_segments;
			// map of m_elements indexes that are variables, key'd by the variable name
			std::unordered_map<string, std::size_t> m_vars;
			// map of m_elements indexes that are blocks, key'd by the block name
			std::unordered_map<string, std::size_t> m_blocks;
			// map of options declared within the node
			std::unordered_map<string, string> m_options;
		};

		class Block : public Node, public Element {
		public:
			Block() : Element(ElementType::Block) {};
			Block(const Block&);
			Block(string name, Node* parent = nullptr);
			Block& operator=(const Block&);

			auto& name() const { return m_name; }
			auto& path() const { return m_path; }
			auto enable(bool enable) { m_enabled = enable; }
			auto enabled() const { return m_enabled; }
			auto& get_array() { return m_array; }
			auto& get_array() const { return m_array; }
			auto array_size() const { return m_array.size(); }
			void clear_array() { m_array.clear(); }
			void set_array_size(std::size_t size) { m_array.resize(size); }
			void set_array(const vector<map<string, string>>& arr) {
				m_array = arr;
			}
			void add_to_array(const map<string, string>& value) {
				if (m_array.empty()) {
					for (auto& pr : value) {
						set_var(pr.first, pr.second);
					}
				}
				m_array.emplace_back(value);
			}
			
			virtual void set_parent(Node* new_parent) override {
				// if the node has a parent, we can safely assume it's a block and has a path, so prepend it to ours
				if (new_parent && new_parent->parent()) {
					if (!parent() || !parent()->parent() || static_cast<Block*>(parent())->path() != static_cast<Block*>(new_parent)->path())
						m_path = static_cast<Block*>(new_parent)->path() + "/" + m_name;
				}
				else m_path = m_name;
				// Node will actually set the parent and update any child blocks by prepending the path of this one, as this one now has a parent
				Node::set_parent(new_parent);
			}
			virtual void render(Render) override;
			virtual void load_block(Block&) override;

			// JS
			static constexpr const char* JSName = "\xff""\xff""WebTemplateBlock";
			//static constexpr const char* JSName = "TemplateBlock";

			void prototype(JS::Context& js);

		protected:
			virtual auto copy() const->std::shared_ptr<Element> override {
				return std::make_shared<Block>(*this);
			}

		private:
			string m_name;
			string m_path;
			vector<map<string, string>> m_array;
			bool m_enabled = true;
		};

		class Conditional : public Node, public Element {
		public:
			Conditional(string varname) : Element(ElementType::Conditional), m_expr(varname)
			{ }
			Conditional(const Conditional&);
			Conditional& operator=(const Conditional&);

			virtual void render(Render) override;

		protected:
			virtual std::shared_ptr<Element> copy() const override {
				return std::make_unique<Conditional>(*this);
			}

		private:
			string m_expr;
		};
	}
}