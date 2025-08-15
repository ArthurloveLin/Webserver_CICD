#ifndef TEMPLATE_ENGINE_H
#define TEMPLATE_ENGINE_H

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>

using namespace std;

class TemplateEngine {
public:
    TemplateEngine();
    ~TemplateEngine();
    
    // 设置模板根目录
    void set_template_root(const string& root_path);
    
    // 渲染模板
    string render(const string& template_name, const map<string, string>& variables);
    
    // 设置变量
    void set_variable(const string& key, const string& value);
    void set_variables(const map<string, string>& variables);
    
    // 清空变量
    void clear_variables();
    
private:
    string template_root;
    map<string, string> global_variables;
    
    // 读取模板文件
    string read_template_file(const string& template_name);
    
    // 替换变量
    string replace_variables(const string& content, const map<string, string>& variables);
    
    // 处理循环 {{#each items}}...{{/each}}
    string process_loops(const string& content, const map<string, string>& variables);
    
    // 处理条件 {{#if condition}}...{{/if}}
    string process_conditions(const string& content, const map<string, string>& variables);
    
    // 处理包含 {{>partial_name}}
    string process_includes(const string& content);
    
    // 辅助方法
    string escape_html(const string& str);
    bool is_true(const string& value);
    vector<string> split_string(const string& str, char delimiter);
};

#endif