#ifndef __YAML_PARSER_H__
#define __YAML_PARSER_H__

#include <vector>
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>

struct YamlNode {
    std::string key;
    std::string value;
    std::vector<YamlNode> children;
    int indent_level = 0;
    bool is_block_text = false;
    YamlNode* parent = nullptr;

    bool HasChildren() const { return !children.empty(); }
    bool IsEmpty() const { return key.empty() && value.empty() && !HasChildren(); }
};

class YamlParser {
public:
    struct Option {
        char comment_char;
        char block_char;
        char list_char;
        std::string indent_chars;

        Option() : 
            comment_char('#'),
            block_char('|'),
            list_char('-'),
            indent_chars("  ") {}
    };

    explicit YamlParser(const std::string& file_path, const Option& opt = Option()) 
        : file_path_(file_path), opt_(opt) {
        Parse();
    }

    std::string GetNodeValue(const std::vector<std::string>& keys) const {
        const YamlNode* node = FindNode(keys);
        return node ? node->value : "";
    }

    void SetNodeValue(const std::vector<std::string>& keys, const std::string& value) {
        if (keys.empty()) return;
        
        YamlNode* node = FindOrCreateNode(keys);
        if (node) node->value = value;
    }

    void PrintNodes() const { WriteNodes(std::cout); }
    void Save() const { 
        std::ofstream file(file_path_);
        if (!file) {
            throw std::runtime_error("无法打开文件进行写入: " + file_path_);
        }
        WriteNodes(file); 
    }
private:
    void WriteNodes(std::ostream& out) const {
        for (const auto& node : nodes_) {
            if (!node.parent) WriteNode(node, 0, out);
        }
    }

    void Parse() {
        std::ifstream file(file_path_);
        if (!file) {
            throw std::runtime_error("无法打开文件: " + file_path_);
        }

        ParseFile(file);
        BuildHierarchy();
    }

    void ParseFile(std::ifstream& file) {
        std::string line;
        BlockState block_state;

        while (std::getline(file, line)) {
            line = ProcessLine(line);
            if (line.empty()) continue;

            if (block_state.active) {
                ProcessBlockContent(line, block_state);
            } else {
                ParseNormalLine(line, block_state);
            }
        }
    }

    struct BlockState {
        bool active = false;
        std::string content;
        int indent = 0;
    };

    void ProcessBlockContent(const std::string& line, BlockState& state) {
        int indent = GetIndentLevel(line);
        if (indent < state.indent) {
            nodes_.back().value = state.content;
            state.active = false;
            ParseNormalLine(line, state);
        } else {
            state.content += line.substr(state.indent * opt_.indent_chars.length()) + "\n";
        }
    }

    void ParseNormalLine(const std::string& line, BlockState& state) {
        YamlNode node;
        node.indent_level = GetIndentLevel(line);
        
        std::string content = line.substr(node.indent_level * opt_.indent_chars.length());
        if (content.empty()) return;

        if (content[0] == opt_.list_char) {
            content = TrimLeft(content.substr(1));
        }

        ParseNodeContent(content, node);
        nodes_.push_back(node);

        CheckBlockStart(line, state);
    }

    void ParseNodeContent(const std::string& content, YamlNode& node) {
        auto colon_pos = content.find(':');
        if (colon_pos != std::string::npos) {
            node.key = content.substr(0, colon_pos);
            if (colon_pos + 1 < content.length()) {
                node.value = Trim(content.substr(colon_pos + 1));
            }
        } else {
            node.value = content;
        }
    }

    void CheckBlockStart(const std::string& line, BlockState& state) {
        if (!nodes_.empty() && line.find(opt_.block_char) != std::string::npos) {
            state.active = true;
            state.content.clear();
            state.indent = nodes_.back().indent_level + 1;
            nodes_.back().is_block_text = true;
        }
    }

    std::string ProcessLine(const std::string& line) {
        auto comment_pos = line.find(opt_.comment_char);
        std::string processed = (comment_pos != std::string::npos) ? 
                              line.substr(0, comment_pos) : line;
        return (processed.find_first_not_of(" \t") != std::string::npos) ? 
               processed : "";
    }

    template<typename NodeType>
    static NodeType* FindNodeInTree(std::vector<NodeType>& nodes, 
                                  const std::vector<std::string>& keys,
                                  size_t start_index = 0) {
        if (start_index >= keys.size()) return nullptr;

        for (auto& node : nodes) {
            if (node.key == keys[start_index]) {
                return (start_index == keys.size() - 1) ? 
                       &node : FindNodeInTree(node.children, keys, start_index + 1);
            }
        }
        return nullptr;
    }

    const YamlNode* FindNode(const std::vector<std::string>& keys) const {
        return FindNodeInTree(const_cast<std::vector<YamlNode>&>(nodes_), keys);
    }

    YamlNode* FindOrCreateNode(const std::vector<std::string>& keys) {
        YamlNode* current = FindOrCreateRootNode(keys[0]);
        for (size_t i = 1; i < keys.size(); ++i) {
            current = FindOrCreateChildNode(current, keys[i]);
        }
        return current;
    }

    YamlNode* FindOrCreateRootNode(const std::string& key) {
        for (auto& node : nodes_) {
            if (node.key == key) return &node;
        }
        nodes_.push_back(YamlNode{key, "", {}, 0});
        return &nodes_.back();
    }

    YamlNode* FindOrCreateChildNode(YamlNode* parent, const std::string& key) {
        for (auto& child : parent->children) {
            if (child.key == key) return &child;
        }
        parent->children.push_back(YamlNode{
            key, "", {}, parent->indent_level + 1, false, parent
        });
        return &parent->children.back();
    }

    void BuildHierarchy() {
        if (nodes_.empty()) return;
        
        std::vector<YamlNode*> stack{&nodes_[0]};
        for (size_t i = 1; i < nodes_.size(); ++i) {
            while (!stack.empty() && stack.back()->indent_level >= nodes_[i].indent_level) {
                stack.pop_back();
            }
            
            if (!stack.empty()) {
                nodes_[i].parent = stack.back();
                stack.back()->children.push_back(nodes_[i]);
            }
            stack.push_back(&nodes_[i]);
        }
    }

    void WriteNode(const YamlNode& node, int level, std::ostream& out) const {
        std::string indent(level * opt_.indent_chars.length(), ' ');
        
        if (!node.key.empty()) {
            out << indent << node.key;
            if (node.is_block_text) {
                out << ": |" << std::endl;
                std::istringstream iss(node.value);
                std::string line;
                while (std::getline(iss, line)) {
                    out << indent << opt_.indent_chars << line << std::endl;
                }
            } else if (!node.value.empty()) {
                out << ": " << node.value << std::endl;
            } else if (node.HasChildren()) {
                out << ":" << std::endl;
            }
        }

        for (const auto& child : node.children) {
            WriteNode(child, level + 1, out);
        }
    }

    int GetIndentLevel(const std::string& line) const {
        size_t count = 0;
        const size_t indent_len = opt_.indent_chars.length();
        while (count * indent_len < line.length() && 
               line.substr(count * indent_len, indent_len) == opt_.indent_chars) {
            count++;
        }
        return count;
    }

    static std::string Trim(const std::string& str) {
        auto first = str.find_first_not_of(" \t\r\n");
        auto last = str.find_last_not_of(" \t\r\n");
        return (first != std::string::npos && last != std::string::npos) ? 
               str.substr(first, last - first + 1) : "";
    }

    static std::string TrimLeft(const std::string& str) {
        auto first = str.find_first_not_of(" \t\r\n");
        return first != std::string::npos ? str.substr(first) : "";
    }

private:
    std::string file_path_;
    Option opt_;
    std::vector<YamlNode> nodes_;
};

#endif // __YAML_PARSER_H__
