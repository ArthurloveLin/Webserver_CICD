#include "markdown_parser.h"
#include <algorithm>
#include <cctype>

MarkdownParser::MarkdownParser() : in_code_block(false) {
}

MarkdownParser::~MarkdownParser() {
}

string MarkdownParser::parse(const string& markdown) {
    if (markdown.empty()) {
        return "";
    }
    
    stringstream ss(markdown);
    string line;
    vector<string> lines;
    
    // 分割为行
    while (getline(ss, line)) {
        lines.push_back(line);
    }
    
    stringstream html;
    
    for (int i = 0; i < lines.size(); i++) {
        string current_line = lines[i];
        
        // 处理代码块
        if (isCodeBlockStart(current_line)) {
            html << parseCodeBlock(lines, i);
            continue;
        }
        
        // 处理列表
        if (isListItem(current_line)) {
            html << parseList(lines, i);
            continue;
        }
        
        // 处理表格
        if (isTableRow(current_line) && i + 1 < lines.size() && 
            lines[i + 1].find("---") != string::npos) {
            html << parseTable(lines, i);
            continue;
        }
        
        // 处理标题
        if (isHeader(current_line)) {
            html << parseHeaders(current_line);
            continue;
        }
        
        // 处理空行
        if (isEmptyLine(current_line)) {
            continue;
        }
        
        // 处理段落
        stringstream paragraph;
        paragraph << current_line;
        
        // 收集连续的非空行作为段落
        while (i + 1 < lines.size() && 
               !isEmptyLine(lines[i + 1]) && 
               !isHeader(lines[i + 1]) &&
               !isCodeBlockStart(lines[i + 1]) &&
               !isListItem(lines[i + 1])) {
            i++;
            paragraph << " " << lines[i];
        }
        
        html << parseParagraph(paragraph.str());
    }
    
    return html.str();
}

string MarkdownParser::parseHeaders(const string& line) {
    string trimmed = trim(line);
    int level = 0;
    
    // 计算#的数量
    while (level < trimmed.length() && trimmed[level] == '#') {
        level++;
    }
    
    if (level > 6) level = 6;
    if (level == 0) return parseParagraph(line);
    
    string content = trim(trimmed.substr(level));
    content = parseInlineElements(content);
    
    return "<h" + to_string(level) + ">" + content + "</h" + to_string(level) + ">\n";
}

string MarkdownParser::parseCodeBlock(const vector<string>& lines, int& index) {
    string start_line = trim(lines[index]);
    string language = "";
    
    // 提取语言标识
    if (start_line.length() > 3) {
        language = trim(start_line.substr(3));
    }
    
    stringstream code;
    index++; // 跳过开始的```
    
    while (index < lines.size()) {
        if (trim(lines[index]) == "```") {
            break;
        }
        code << escapeHtml(lines[index]) << "\n";
        index++;
    }
    
    string lang_class = language.empty() ? "" : " class=\"language-" + language + "\"";
    return "<pre><code" + lang_class + ">" + code.str() + "</code></pre>\n";
}

string MarkdownParser::parseList(const vector<string>& lines, int& index) {
    stringstream html;
    bool is_ordered = false;
    
    // 检查是否为有序列表
    string first_line = trim(lines[index]);
    if (!first_line.empty() && isdigit(first_line[0])) {
        is_ordered = true;
    }
    
    string list_tag = is_ordered ? "ol" : "ul";
    html << "<" << list_tag << ">\n";
    
    while (index < lines.size() && isListItem(lines[index])) {
        string line = trim(lines[index]);
        string content;
        
        // 移除列表标记
        if (is_ordered) {
            size_t dot_pos = line.find('.');
            if (dot_pos != string::npos) {
                content = trim(line.substr(dot_pos + 1));
            }
        } else {
            if (line[0] == '-' || line[0] == '*' || line[0] == '+') {
                content = trim(line.substr(1));
            }
        }
        
        html << "<li>" << parseInlineElements(content) << "</li>\n";
        index++;
    }
    index--; // 回退一行，因为外层循环会++
    
    html << "</" << list_tag << ">\n";
    return html.str();
}

string MarkdownParser::parseTable(const vector<string>& lines, int& index) {
    stringstream html;
    html << "<table>\n";
    
    // 解析表头
    string header_line = lines[index];
    vector<string> headers;
    stringstream ss(header_line);
    string cell;
    
    while (getline(ss, cell, '|')) {
        cell = trim(cell);
        if (!cell.empty()) {
            headers.push_back(cell);
        }
    }
    
    if (!headers.empty()) {
        html << "<thead>\n<tr>\n";
        for (const string& header : headers) {
            html << "<th>" << parseInlineElements(header) << "</th>\n";
        }
        html << "</tr>\n</thead>\n";
    }
    
    index += 2; // 跳过表头和分隔行
    
    // 解析表格内容
    html << "<tbody>\n";
    while (index < lines.size() && isTableRow(lines[index])) {
        string row_line = lines[index];
        vector<string> cells;
        stringstream row_ss(row_line);
        string row_cell;
        
        while (getline(row_ss, row_cell, '|')) {
            row_cell = trim(row_cell);
            if (!row_cell.empty()) {
                cells.push_back(row_cell);
            }
        }
        
        if (!cells.empty()) {
            html << "<tr>\n";
            for (const string& cell : cells) {
                html << "<td>" << parseInlineElements(cell) << "</td>\n";
            }
            html << "</tr>\n";
        }
        index++;
    }
    index--; // 回退一行
    
    html << "</tbody>\n</table>\n";
    return html.str();
}

string MarkdownParser::parseParagraph(const string& text) {
    if (trim(text).empty()) {
        return "";
    }
    return "<p>" + parseInlineElements(text) + "</p>\n";
}

string MarkdownParser::parseInlineElements(const string& text) {
    string result = text;
    
    // 按顺序解析行内元素
    result = parseImages(result);    // 先解析图片，避免与链接冲突
    result = parseLinks(result);     // 然后解析链接
    result = parseCode(result);      // 代码（防止代码内的格式被解析）
    result = parseBold(result);      // 加粗
    result = parseItalic(result);    // 斜体
    
    return result;
}

string MarkdownParser::parseLinks(const string& text) {
    string result = text;
    regex link_regex(R"(\[([^\]]*)\]\(([^\)]*)\))");
    smatch match;
    
    string::const_iterator start = result.cbegin();
    stringstream new_result;
    
    while (regex_search(start, result.cend(), match, link_regex)) {
        new_result << string(start, match[0].first);
        
        string link_text = match[1].str();
        string link_url = match[2].str();
        
        new_result << "<a href=\"" << escapeHtml(link_url) << "\">" 
                   << escapeHtml(link_text) << "</a>";
        
        start = match[0].second;
    }
    new_result << string(start, result.cend());
    
    return new_result.str();
}

string MarkdownParser::parseImages(const string& text) {
    string result = text;
    regex image_regex(R"(!\[([^\]]*)\]\(([^\)]*)\))");
    smatch match;
    
    string::const_iterator start = result.cbegin();
    stringstream new_result;
    
    while (regex_search(start, result.cend(), match, image_regex)) {
        new_result << string(start, match[0].first);
        
        string alt_text = match[1].str();
        string image_url = match[2].str();
        
        new_result << "<img src=\"" << escapeHtml(image_url) 
                   << "\" alt=\"" << escapeHtml(alt_text) << "\" />";
        
        start = match[0].second;
    }
    new_result << string(start, result.cend());
    
    return new_result.str();
}

string MarkdownParser::parseBold(const string& text) {
    string result = text;
    regex bold_regex(R"(\*\*([^*]+)\*\*)");
    
    result = regex_replace(result, bold_regex, "<strong>$1</strong>");
    
    return result;
}

string MarkdownParser::parseItalic(const string& text) {
    string result = text;
    regex italic_regex(R"(\*([^*]+)\*)");
    
    result = regex_replace(result, italic_regex, "<em>$1</em>");
    
    return result;
}

string MarkdownParser::parseCode(const string& text) {
    string result = text;
    regex code_regex(R"(`([^`]+)`)");
    smatch match;
    
    string::const_iterator start = result.cbegin();
    stringstream new_result;
    
    while (regex_search(start, result.cend(), match, code_regex)) {
        new_result << string(start, match[0].first);
        new_result << "<code>" << escapeHtml(match[1].str()) << "</code>";
        start = match[0].second;
    }
    new_result << string(start, result.cend());
    
    return new_result.str();
}

string MarkdownParser::escapeHtml(const string& text) {
    string result = text;
    
    // 替换HTML特殊字符
    string chars[][2] = {
        {"&", "&amp;"},
        {"<", "&lt;"},
        {">", "&gt;"},
        {"\"", "&quot;"},
        {"'", "&#39;"}
    };
    
    for (auto& pair : chars) {
        size_t pos = 0;
        while ((pos = result.find(pair[0], pos)) != string::npos) {
            result.replace(pos, pair[0].length(), pair[1]);
            pos += pair[1].length();
        }
    }
    
    return result;
}

string MarkdownParser::trim(const string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool MarkdownParser::isEmptyLine(const string& line) {
    return trim(line).empty();
}

bool MarkdownParser::isHeader(const string& line) {
    string trimmed = trim(line);
    return !trimmed.empty() && trimmed[0] == '#';
}

bool MarkdownParser::isCodeBlockStart(const string& line) {
    string trimmed = trim(line);
    return trimmed.length() >= 3 && trimmed.substr(0, 3) == "```";
}

bool MarkdownParser::isListItem(const string& line) {
    string trimmed = trim(line);
    if (trimmed.empty()) return false;
    
    // 无序列表
    if (trimmed[0] == '-' || trimmed[0] == '*' || trimmed[0] == '+') {
        return true;
    }
    
    // 有序列表
    if (isdigit(trimmed[0])) {
        size_t dot_pos = trimmed.find('.');
        return dot_pos != string::npos && dot_pos < 4;
    }
    
    return false;
}

bool MarkdownParser::isTableRow(const string& line) {
    return line.find('|') != string::npos;
}