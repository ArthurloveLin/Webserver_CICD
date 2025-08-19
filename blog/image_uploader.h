#ifndef IMAGE_UPLOADER_H
#define IMAGE_UPLOADER_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <random>
#include <chrono>

using namespace std;

struct UploadResult {
    bool success;
    string message;
    string file_url;
    string file_path;
    size_t file_size;
};

class ImageUploader {
public:
    ImageUploader();
    ~ImageUploader();
    
    // 主要上传方法
    UploadResult uploadImage(const string& content_type, const string& body);
    
    // 配置方法
    void setUploadPath(const string& path);
    void setMaxFileSize(size_t size);
    void setAllowedTypes(const vector<string>& types);
    
private:
    // 文件处理方法
    string generateFilename(const string& extension);
    string extractExtension(const string& filename);
    bool isValidImageType(const string& extension);
    bool createUploadDirectory();
    
    // multipart/form-data 解析
    map<string, string> parseMultipartData(const string& content_type, const string& body);
    string extractBoundary(const string& content_type);
    vector<string> splitByBoundary(const string& body, const string& boundary);
    map<string, string> parseFormField(const string& field_data);
    
    // 工具方法
    string getContentDisposition(const string& headers);
    string extractFilename(const string& content_disposition);
    string getMimeType(const string& extension);
    string trim(const string& str);
    
    // 配置参数
    string upload_path;
    size_t max_file_size;
    vector<string> allowed_types;
    
    // 随机数生成器
    mt19937 rng;
};

#endif