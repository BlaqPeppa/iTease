#include "stdinc.h"
#include "template.h"
#include "web.h"
#include "cpp/string.h"

using namespace iTease;
using namespace iTease::Templating;

Node::Node(const Node& v) : m_order(v.m_order), m_index(v.m_index), m_vars(v.m_vars), m_blocks(v.m_blocks), m_options(v.m_options), m_segments(v.m_segments), m_enabled(v.m_enabled) {
	for (auto& el : v.m_elements) {
		m_elements.emplace_back(el->copy());
	}
}
auto Node::operator=(const Node& v)->Node& {
	if (&v == this) return *this;
	m_order = v.m_order;
	m_index = v.m_index;
	m_vars = v.m_vars;
	m_blocks = v.m_blocks;
	m_options = v.m_options;
	m_segments = v.m_segments;
	m_enabled = v.m_enabled;

	m_elements.clear();
	for (auto& el : v.m_elements) {
		m_elements.emplace_back(el->copy());
	}
	for (auto& pr : m_blocks) {
		std::static_pointer_cast<Block>(m_elements[pr.second])->set_parent(this);
	}
	if (m_parent) m_parent->new_child(*this);
	return *this;
}
auto Node::clear()->void {
	m_elements.clear();
	m_options.clear();
	m_segments.clear();
	m_blocks.clear();
	m_vars.clear();
	m_order.clear();
	m_error = "";
}
auto Node::load(std::istream& in)->bool {
	long ln = 0;
	try {
		in.unsetf(std::ios::skipws);
		string buff;
		buff.reserve(1024);
		std::stack<Element*> blocks;
		bool skipnl = false;
		string segment = "";
		for (string str; std::getline(in, str); ++ln) {
			if (ln && !skipnl) buff += "\n";
			auto start = str.begin();
			skipnl = false;
			for (auto cur = str.begin(); cur != str.end();) {
				auto it = std::find(cur, str.end(), '{');
				if (it == str.end()) break;
				auto beg = it++;
				if (it != str.end() && *it == '%') {
					auto it2 = std::find(++it, str.end(), '%');
					if (it2 != str.end()) {
						auto end = it2 + 1;
						if (end != str.end() && *end++ == '}') {
							string inside(it, it2);
							auto p = std::find(inside.begin(), inside.end(), ':');
							string name(inside.begin(), p),
								param(p != inside.end() ? p + 1 : p, inside.end());
							std::transform(name.begin(), name.end(), name.begin(), [](char c) { return std::tolower(c); });

							// get top stack block
							auto topElement = !blocks.empty() ? blocks.top() : nullptr;
							Node* topNode = nullptr;
							Block* topBlock = nullptr;
							Conditional* topConditional = nullptr;
							if (topElement) {
								if (topElement->type() == ElementType::Block) {
									topBlock = static_cast<Block*>(topElement);
									topNode = topBlock;
								}
								else if (topElement->type() == ElementType::Conditional) {
									topConditional = static_cast<Conditional*>(topElement);
									topNode = topConditional;
								}
							}

							// check tag
							if (name != "block" && name != "var" && name != "seg" &&
								name != "begin" && name != "end" &&
								name != "if" && name != "endif" &&
								name != "opt") name = "";
							auto inv = name == "opt" ? param.end() : std::find_if_not(param.begin(), param.end(), [](char c) { return std::isalnum(c) || c == '_'; });
							if (inv == param.end() && !name.empty()) {
								// erase tag
								str.replace(beg, end, "");

								//auto cbeg = std::find_if(str.begin(), beg, [](char c) { return !std::isspace(c); });
								//if (cbeg != beg) {
								auto line_left = string(start, beg);
								buff += /*(ln && name != "begin" && name != "end" ? "\n\r" : "") +*/ line_left;
								//}

								// process option tags
								if (name == "opt") {
									if (topNode) topNode->add_segment(buff);
									else add_segment(buff);
									auto p = std::find(param.begin(), param.end(), '=');
									string optname(param.begin(), p),
										optval(p != param.end() ? p + 1 : p, param.end());
									if (topNode)
										topNode->m_options.emplace(optname, optval);
									else
										m_options.emplace(optname, optval);
									skipnl = ltrim(str).empty();
								}
								// process named segments
								else if (name == "seg") {
									if (topNode) topNode->add_segment(buff);
									else add_segment(buff);
									if (!segment.empty())
										throw std::runtime_error("segments cannot be nested");
									segment = param;
									skipnl = ltrim(str).empty();
								}
								// process var/block tags
								else if (name == "begin") {
									if (!segment.empty())
										throw std::runtime_error("cannot nest a block within a segment");
									if (!blocks.empty()) topNode->add_segment(buff);
									else add_segment(buff);
									// begin a sub-block
									if (param == "block" || param == "var" || param == "blocks" || param == "vars")
										throw std::runtime_error("disallowed block/var name");
									if (topNode)
										blocks.push(topNode->add_block(param).get());
									else
										blocks.push(add_block(param).get());
									skipnl = ltrim(str).empty();
								}
								else if (name == "end") {
									if (!segment.empty()) {
										if (segment != param)
											throw std::runtime_error("end of segment name mismatch, expected '" + segment + "'");
									}
									else if (!topBlock)
										throw std::runtime_error("end found when not in a block or segment");
									else if (param != topBlock->name())
										throw std::runtime_error("end of block name mismatch, expected '" + topBlock->name() + "'");

									if (!segment.empty()) {
										if (topNode) topNode->add_segment(buff, segment);
										else add_segment(buff, segment);
										segment = "";
									}
									else {
										// end the sub-block
										if (!buff.empty()) {
											topBlock->add_segment(buff, segment);
											buff = "";
										}
										blocks.pop();
									}
									skipnl = ltrim(str).empty();
								}
								// process conditional segments
								else if (name == "if") {
									if (!segment.empty())
										throw std::runtime_error("cannot nest an if-block within a segment");
									if (!blocks.empty()) topNode->add_segment(buff);
									else add_segment(buff);
									// begin a conditional block
									if (topNode)
										blocks.push(topNode->add_conditional(param).get());
									else
										blocks.push(add_conditional(param).get());
									skipnl = ltrim(str).empty();
									//if (!blocks.empty()))
									//blocks.push(blocks
								}
								else if (name == "endif") {
									if (!param.empty())
										throw std::runtime_error("unexpected param in endif tag");
									if (!topConditional)
										throw std::runtime_error("end of if-block found when not in an if-block");
									if (!segment.empty())
										throw std::runtime_error("end of if-block found while still in segment");

									// end the sub-block
									if (!buff.empty()) {
										topConditional->add_segment(buff, segment);
										buff = "";
									}
									blocks.pop();
									skipnl = ltrim(str).empty();
								}
								else {
									// if in a sub-block, add vars and blocks to it instead
									if (param == "block" || param == "var" || param == "blocks" || param == "vars")
										throw std::runtime_error("disallowed block/var name");
									if (topNode) {
										topNode->add_segment(buff, segment);

										if (name == "var") topNode->add_variable(param);
										else if (name == "block") topNode->add_block(param);
									}
									else {
										add_segment(buff, segment);

										if (name == "var") add_variable(param);
										else if (name == "block") add_block(param);
									}
								}

								buff = "";
								start = cur = beg;
							}
							else cur = end;
						}
						else cur = end;
					}
					else cur = it2;
				}
				else {
					cur = it;
				}
			}
			if (start != str.end()) {
				buff += string(start, str.end());
			}
		}
		if (!blocks.empty()) throw std::runtime_error("still in block at end of template");
		if (!segment.empty()) throw std::runtime_error("still in segment at end of template");
		if (!buff.empty()) add_segment(buff);
		m_enabled = m_vars.empty();
	}
	catch (const std::exception& ex) {
		throw Templating::LoadError(ex, ln);
	}
	return true;
}
auto Node::render(Render out)->void {
	// manage var scope
	auto num_set_vars = 0;
	for (auto& pr : m_vars) {
		auto var = std::static_pointer_cast<Variable>(m_elements[pr.second]);
		if (!var->has_value()) {
			auto it = out.vars.find(pr.first);
			if (it != out.vars.end())
				var->set(it->second);
			else
				continue;
		}
		else out.vars[pr.first] = var->value();
		++num_set_vars;
	}
	if (m_vars.size() == num_set_vars || m_enabled) {
		for (auto i : m_order) {
			// detect problems with managing m_order...
			if (i >= m_index.size())
				throw std::range_error("template element order index out of range");
			auto pr = m_index[i];
			i = pr.second;
			if (i >= m_elements.size())
				throw std::range_error("template element order index out of range");
			if (!pr.first) continue;
			auto& element = m_elements[i];
			element->render(out);
		}
	}
}
auto Node::enable_segment(const string& name)->void {
	auto pr = m_segments.equal_range(name);
	for (auto it = pr.first; it != pr.second; ++it) {
		m_index[it->second].first = true;
	}
}
auto Node::disable_segment(const string& name)->void {
	auto pr = m_segments.equal_range(name);
	for (auto it = pr.first; it != pr.second; ++it) {
		m_index[it->second].first = false;
	}
}
auto Node::block(const string& name)->shared_ptr<Block> {
	auto it = m_blocks.find(name);
	if (it != m_blocks.end())
		return std::static_pointer_cast<Block>(m_elements[it->second]);
	return nullptr;
}
auto Node::add_block(const string& v)->shared_ptr<Block> {
	auto idx = m_elements.size();
	m_elements.push_back(std::move(std::make_shared<Block>(v, this)));
	m_blocks.emplace(v, idx);
	m_order.emplace_back(m_index.size());
	m_index.emplace_back(true, idx);
	return std::static_pointer_cast<Block>(m_elements.back());
}
auto Node::insert_block(const string& v)->shared_ptr<Block> {
	auto idx = m_elements.size();
	m_elements.push_back(std::move(std::make_shared<Block>(v, this)));
	m_blocks.emplace(v, idx);
	m_order.insert(m_order.begin(), m_index.size());
	m_index.emplace_back(true, idx);
	return std::static_pointer_cast<Block>(m_elements.back());
}
auto Node::add_block_at(const string& name, const string& after)->shared_ptr<Block> {
	auto idx = get_block_index(after);
	if (idx.first) {
		auto i = m_elements.size();
		m_elements.push_back(std::move(std::make_shared<Block>(name, this)));
		m_blocks.emplace(name, i);
		insert_element_order(idx.second, i);
		auto ptr = std::static_pointer_cast<Block>(m_elements.back());
		load_block(*ptr);
		return ptr;
	}
	return nullptr;
}
auto Node::insert_block_at(const string& name, const string& before)->shared_ptr<Block> {
	auto idx = get_block_index(before);
	if (idx.first) {
		auto i = m_elements.size();
		m_elements.push_back(std::move(std::make_shared<Block>(name, this)));
		m_blocks.emplace(name, i);
		insert_element_order(idx.second, i, true);
		auto ptr = std::static_pointer_cast<Block>(m_elements.back());
		load_block(*ptr);
		return ptr;
	}
	return nullptr;
}
auto Node::var(const string& name)->Variable& {
	auto it = m_vars.find(name);
	if (it != m_vars.end())
		return *static_cast<Variable*>(m_elements[it->second].get());
	return set_var(name);
}
auto Node::get_var(const string& name) const->string {
	auto it = m_vars.find(name);
	if (it != m_vars.end())
		return std::static_pointer_cast<Variable>(m_elements[it->second])->value();
	return "";
}
auto Node::blocks()->vector<shared_ptr<Block>> {
	vector<shared_ptr<Block>> blocks;
	blocks.reserve(m_blocks.size());
	for (auto it = m_order.begin(); it != m_order.end(); ++it) {
		auto id = m_index[*it].second;
		if (m_elements[id]->type() == ElementType::Block)
			blocks.emplace_back(std::static_pointer_cast<Block>(m_elements[id]));
	}
	return blocks;
}
auto Node::vars()->vector<shared_ptr<Variable>> {
	vector<shared_ptr<Variable>> vars;
	vars.reserve(m_vars.size());
	for (auto& var : m_vars) {
		vars.emplace_back(std::static_pointer_cast<Variable>(m_elements[var.second]));
	}
	return vars;
}
auto Node::options() const->const std::unordered_map<string, string>& {
	return m_options;
}
auto Node::set_var(const string& name)->Variable& {
	m_enabled = true;
	auto it = m_vars.find(name);
	if (it != m_vars.end())
		return static_cast<Variable&>(*m_elements[it->second]);

	auto i = m_elements.size();
	auto var = std::make_shared<Variable>(name);
	auto& r = *var.get();
	m_elements.push_back(std::move(var));
	m_vars[name] = i;
	return r;
}
auto Node::set_var(const string& name, const string& value)->Variable& {
	auto& var = set_var(name);
	var.set(value);
	return var;
}
auto Node::has_var(const string& name) const->bool {
	return m_vars.find(name) != m_vars.end();
}
auto Node::has_block(const string& name) const->bool {
	return m_blocks.find(name) != m_blocks.end();
}
auto Node::add_segment(const string& v, const string& name)->void {
	if (!v.empty()) {
		m_elements.push_back(std::move(std::make_shared<Segment>(v)));
		if (!name.empty()) m_segments.emplace(name, m_index.size());
		m_order.emplace_back(m_index.size());
		m_index.emplace_back(true, m_elements.size() - 1);
	}
}
auto Node::add_variable(const string& v, const string& segment)->void {
	auto it = m_vars.find(v);
	size_t idx;
	if (it != m_vars.end())
		idx = it->second;
	else {
		idx = m_elements.size();
		m_vars.emplace(v, idx);
		m_elements.push_back(std::move(std::make_shared<Variable>(v)));
	}
	if (!segment.empty()) m_segments.emplace(segment, m_index.size());
	m_order.emplace_back(m_index.size());
	m_index.emplace_back(true, idx);
}
auto Node::add_conditional(const string& v)->std::shared_ptr<Conditional> {
	auto idx = m_elements.size();
	m_elements.push_back(std::move(std::make_shared<Conditional>(v)));
	m_order.emplace_back(m_index.size());
	m_index.emplace_back(true, idx);
	return std::static_pointer_cast<Conditional>(m_elements.back());
}
void Node::load_block(Block& block) {
	for (auto& idx : m_vars) {
		auto var = static_cast<Variable*>(m_elements[idx.second].get());
		if (var->value().empty()) continue;
		block.set_var(var->name(), var->value());
	}

	OnLoadBlock(block);
}
auto Node::insert_element_order(size_t i, size_t n, bool before)->void {
	for (auto it = m_order.begin(); it != m_order.end(); ++it) {
		if (it != m_order.begin() && *it == i) {
			if (before) m_order.insert(it, m_index.size());
			else m_order.insert(++it, m_index.size());
			break;
		}
	}
	m_index.emplace_back(true, n);
}
auto Node::get_block_index(const string& name) const->Node::BlockIndexResult {
	auto it = m_blocks.find(name);
	if (it != m_blocks.end())
		return{true, it->second};
	return{false, 0};
}
void Node::set_parent(Node* parent) {
	m_parent = parent;
	// update the path of child blocks
	for (auto& pr : m_blocks) {
		std::static_pointer_cast<Block>(m_elements[pr.second])->set_parent(this);
	}
}
void Node::new_child(Node& node) {
	if (!node.m_enabled) {
		for (auto& node_var : node.m_vars) {
			auto it = m_vars.find(node_var.first);
			if (it != m_vars.end())
				node.set_var(it->first, std::static_pointer_cast<Variable>(m_elements[it->second])->value());
		}
	}
	if (m_parent) m_parent->new_child(node);
}

Block::Block(const Block& v) : Node(v), Element(ElementType::Block), m_array(v.m_array) {
	if (m_name.empty() && !v.m_name.empty())
		m_name = v.m_name;
	m_path = (parent() && parent()->parent() ? static_cast<Block*>(parent())->path() + "/" : "") + m_name;
}
Block::Block(string name, Node* parent) : Node(parent), Element(ElementType::Block), m_name(name) {
	m_path = (parent && parent->parent() ? static_cast<Block*>(parent)->path() + "/" : "") + m_name;
}
auto Block::operator=(const Block& v)->Block& {
	static_cast<Node&>(*this) = static_cast<const Node&>(v);
	set_array(v.m_array);
	m_path = (parent() && parent()->parent() ? static_cast<Block*>(parent())->path() + "/" : "") + m_name;
	return *this;
}
auto Block::render(Render out)->void {
	// we'll send 2 event notifications per block rendered, one for the block and one for the topmost node
	auto top_parent = parent();
	if (top_parent) {
		while (top_parent->parent())
			top_parent = top_parent->parent();
	}
	if (top_parent) top_parent->OnRenderBlock(*this);

	OnRenderBlock(*this);

	if (enabled()) {
		// add vars in this block to the scope if none already exist with a value
		auto vec = vars();
		for (auto& var : vec) {
			if (var->has_value())
				out.vars[var->name()] = var->value();
		}
		// render node
		if (m_array.empty())
			Node::render(std::move(out));
		else {
			// in the event of an array, render the node over and over with each set of values
			for (auto& map : m_array) {
				for (auto& pr : map) {
					set_var(pr.first, pr.second);
				}
				Node::render(out);
			}
		}
	}
}
auto Block::load_block(Block& block)->void {
	block.m_path = m_path + "/" + block.m_path;
	OnLoadBlock(block);
}
auto Block::prototype(JS::Context& js)->void {
	JS::StackAssert sa(js, 1);

	js.pop();
	js.push(*this);

	// return prototype
	js.get_global<void>("\xff""\xff""module-web");
	js.get_property<void>(-1, JSName);
	js.remove(-2);
}
Conditional::Conditional(const Conditional& v) : Node(v), Element(ElementType::Conditional), m_expr(v.m_expr)
{ }
auto Conditional::operator=(const Conditional& v)->Conditional& {
	static_cast<Node&>(*this) = static_cast<const Node&>(v);
	m_expr = v.m_expr;
	return *this;
}
auto Conditional::render(Render render)->void {
	auto it = render.vars.find(m_expr);
	if (it != render.vars.end() && !it->second.empty())
		Node::render(std::move(render));
}