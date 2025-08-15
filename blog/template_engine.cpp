#include "template_engine.h"
#include <algorithm>
#include <regex>

TemplateEngine::TemplateEngine() {
    template_root = "./templates/";
}

TemplateEngine::~TemplateEngine() {
}

void TemplateEngine::set_template_root(const string& root_path) {
    template_root = root_path;
    if (template_root.back() != '/') {
        template_root += '/';
    }
}

string TemplateEngine::render(const string& template_name, const map<string, string>& variables) {
    // 合并全局变量和局部变量
    map<string, string> all_variables = global_variables;
    for (const auto& var : variables) {
        all_variables[var.first] = var.second;
    }
    
    // 读取模板文件
    string content = read_template_file(template_name);
    if (content.empty()) {
        return "<!-- Template not found: " + template_name + " -->";
    }
    
    // 处理包含
    content = process_includes(content);
    
    // 处理条件
    content = process_conditions(content, all_variables);
    
    // 处理循环
    content = process_loops(content, all_variables);
    
    // 替换变量
    content = replace_variables(content, all_variables);
    
    return content;
}

void TemplateEngine::set_variable(const string& key, const string& value) {
    global_variables[key] = value;
}

void TemplateEngine::set_variables(const map<string, string>& variables) {
    for (const auto& var : variables) {
        global_variables[var.first] = var.second;
    }
}

void TemplateEngine::clear_variables() {
    global_variables.clear();
}

string TemplateEngine::read_template_file(const string& template_name) {
    string file_path = template_root + template_name;
    ifstream file(file_path);
    
    if (!file.is_open()) {
        return "";
    }
    
    stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    return buffer.str();
}

string TemplateEngine::replace_variables(const string& content, const map<string, string>& variables) {
    string result = content;
    
    // 简单的变量替换 {{variable_name}}
    for (const auto& var : variables) {
        string pattern = "{{" + var.first + "}}";
        size_t pos = 0;
        while ((pos = result.find(pattern, pos)) != string::npos) {
            result.replace(pos, pattern.length(), escape_html(var.second));
            pos += var.second.length();
        }
    }
    
    // 处理原始变量 {{{variable_name}}} (不转义HTML)
    for (const auto& var : variables) {
        string pattern = "{{{" + var.first + "}}}";
        size_t pos = 0;
        while ((pos = result.find(pattern, pos)) != string::npos) {
            result.replace(pos, pattern.length(), var.second);
            pos += var.second.length();
        }
    }
    
    return result;
}

string TemplateEngine::process_loops(const string& content, const map<string, string>& variables) {
    string result = content;
    
    // 简化的循环处理，这里只是一个基础实现
    // 实际项目中可能需要更复杂的解析
    
    size_t start_pos = 0;
    while ((start_pos = result.find("{{#each ", start_pos)) != string::npos) {
        size_t end_tag_start = result.find("{{/each}}", start_pos);
        if (end_tag_start == string::npos) break;
        
        // 提取循环变量名
        size_t var_start = start_pos + 8; // "{{#each "的长度
        size_t var_end = result.find("}}", var_start);
        if (var_end == string::npos) break;
        
        string loop_var = result.substr(var_start, var_end - var_start);
        
        // 提取循环体
        size_t loop_body_start = var_end + 2;
        string loop_body = result.substr(loop_body_start, end_tag_start - loop_body_start);
        
        // 简单的循环处理：如果变量存在且非空，就重复一次
        string loop_result = "";
        if (variables.find(loop_var) != variables.end() && !variables.at(loop_var).empty()) {
            loop_result = loop_body;
        }
        
        // 替换整个循环块
        size_t end_tag_end = end_tag_start + 9; // "{{/each}}"的长度
        result.replace(start_pos, end_tag_end - start_pos, loop_result);
        start_pos += loop_result.length();
    }
    
    return result;
}

string TemplateEngine::process_conditions(const string& content, const map<string, string>& variables) {
    string result = content;
    
    size_t start_pos = 0;
    while ((start_pos = result.find("{{#if ", start_pos)) != string::npos) {
        size_t end_tag_start = result.find("{{/if}}", start_pos);
        if (end_tag_start == string::npos) break;
        
        // 提取条件变量名
        size_t var_start = start_pos + 6; // "{{#if "的长度
        size_t var_end = result.find("}}", var_start);
        if (var_end == string::npos) break;
        
        string condition_var = result.substr(var_start, var_end - var_start);
        
        // 提取条件体
        size_t condition_body_start = var_end + 2;
        string condition_body = result.substr(condition_body_start, end_tag_start - condition_body_start);
        
        // 判断条件
        string condition_result = "";
        if (variables.find(condition_var) != variables.end() && is_true(variables.at(condition_var))) {
            condition_result = condition_body;
        }
        
        // 替换整个条件块
        size_t end_tag_end = end_tag_start + 7; // "{{/if}}"的长度
        result.replace(start_pos, end_tag_end - start_pos, condition_result);
        start_pos += condition_result.length();
    }
    
    return result;
}

string TemplateEngine::process_includes(const string& content) {
    string result = content;
    
    size_t pos = 0;
    while ((pos = result.find("{{>", pos)) != string::npos) {
        size_t end_pos = result.find("}}", pos);
        if (end_pos == string::npos) break;
        
        // 提取包含的模板名
        string include_name = result.substr(pos + 3, end_pos - pos - 3);
        
        // 读取包含的模板
        string include_content = read_template_file("partials/" + include_name + ".html");
        
        // 替换包含标签
        result.replace(pos, end_pos - pos + 2, include_content);
        pos += include_content.length();
    }
    
    return result;
}

string TemplateEngine::escape_html(const string& str) {
    string result = str;
    
    size_t pos = 0;
    while ((pos = result.find("&", pos)) != string::npos) {
        result.replace(pos, 1, "&amp;");
        pos += 5;
    }
    
    pos = 0;
    while ((pos = result.find("<", pos)) != string::npos) {
        result.replace(pos, 1, "&lt;");
        pos += 4;
    }
    
    pos = 0;
    while ((pos = result.find(">", pos)) != string::npos) {
        result.replace(pos, 1, "&gt;");
        pos += 4;
    }
    
    pos = 0;
    while ((pos = result.find("\"", pos)) != string::npos) {
        result.replace(pos, 1, "&quot;");
        pos += 6;
    }
    
    pos = 0;
    while ((pos = result.find("'", pos)) != string::npos) {
        result.replace(pos, 1, "&#x27;");
        pos += 6;
    }
    
    return result;
}

bool TemplateEngine::is_true(const string& value) {
    if (value.empty()) return false;
    if (value == "0") return false;
    if (value == "false") return false;
    if (value == "null") return false;
    return true;
}

vector<string> TemplateEngine::split_string(const string& str, char delimiter) {
    vector<string> result;
    stringstream ss(str);
    string item;
    
    while (getline(ss, item, delimiter)) {
        result.push_back(item);
    }
    
    return result;
}