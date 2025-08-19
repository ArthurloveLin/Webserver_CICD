#ifndef MARKDOWN_PARSER_H
#define MARKDOWN_PARSER_H

#include <string>
#include <vector>
#include <regex>
#include <sstream>

using namespace std;

class MarkdownParser {
public:
    MarkdownParser();
    ~MarkdownParser();
    
    // 主要解析方法
    string parse(const string& markdown);
    
private:
    // 核心解析方法
    string parseBlock(const string& block);
    string parseLine(const string& line);
    
    // 块级元素解析
    string parseHeaders(const string& line);
    string parseCodeBlock(const vector<string>& lines, int& index);
    string parseList(const vector<string>& lines, int& index);
    string parseTable(const vector<string>& lines, int& index);
    string parseParagraph(const string& text);
    
    // 行内元素解析
    string parseInlineElements(const string& text);
    string parseLinks(const string& text);
    string parseImages(const string& text);
    string parseBold(const string& text);
    string parseItalic(const string& text);
    string parseCode(const string& text);
    
    // 工具方法
    string escapeHtml(const string& text);
    string trim(const string& str);
    bool isEmptyLine(const string& line);
    bool isHeader(const string& line);
    bool isCodeBlockStart(const string& line);
    bool isListItem(const string& line);
    bool isTableRow(const string& line);
    
    // 状态管理
    bool in_code_block;
    string code_language;
};

#endif