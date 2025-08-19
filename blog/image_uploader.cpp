#include "image_uploader.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

ImageUploader::ImageUploader() 
    : upload_path("/home/arthur/Public/TinyWebServer/root/uploads/images/"),
      max_file_size(5 * 1024 * 1024), // 5MB
      rng(chrono::steady_clock::now().time_since_epoch().count()) {
    
    // 设置允许的图片类型
    allowed_types = {"jpg", "jpeg", "png", "gif", "webp"};
    
    // 创建上传目录
    createUploadDirectory();
}

ImageUploader::~ImageUploader() {
}

UploadResult ImageUploader::uploadImage(const string& content_type, const string& body) {
    UploadResult result;
    result.success = false;
    
    // 检查content-type
    if (content_type.find("multipart/form-data") == string::npos) {
        result.message = "Invalid content type. Expected multipart/form-data";
        return result;
    }
    
    // 解析multipart数据
    auto form_data = parseMultipartData(content_type, body);
    
    if (form_data.find("file_content") == form_data.end() ||
        form_data.find("filename") == form_data.end()) {
        result.message = "No file data found in request";
        return result;
    }
    
    string filename = form_data["filename"];
    string file_content = form_data["file_content"];
    
    // 检查文件大小
    if (file_content.size() > max_file_size) {
        result.message = "File size exceeds maximum limit (" + 
                        to_string(max_file_size / 1024 / 1024) + "MB)";
        return result;
    }
    
    if (file_content.empty()) {
        result.message = "Empty file content";
        return result;
    }
    
    // 验证文件类型
    string extension = extractExtension(filename);
    if (!isValidImageType(extension)) {
        result.message = "Invalid file type. Allowed types: ";
        for (size_t i = 0; i < allowed_types.size(); i++) {
            result.message += allowed_types[i];
            if (i < allowed_types.size() - 1) result.message += ", ";
        }
        return result;
    }
    
    // 生成新文件名
    string new_filename = generateFilename(extension);
    string file_path = upload_path + new_filename;
    
    // 保存文件
    ofstream output_file(file_path, ios::binary);
    if (!output_file.is_open()) {
        result.message = "Failed to create output file";
        return result;
    }
    
    output_file.write(file_content.c_str(), file_content.size());
    output_file.close();
    
    // 验证文件是否保存成功
    struct stat file_stat;
    if (stat(file_path.c_str(), &file_stat) != 0) {
        result.message = "Failed to save file";
        return result;
    }
    
    // 返回成功结果
    result.success = true;
    result.message = "File uploaded successfully";
    result.file_url = "/uploads/images/" + new_filename;
    result.file_path = file_path;
    result.file_size = file_stat.st_size;
    
    return result;
}

void ImageUploader::setUploadPath(const string& path) {
    upload_path = path;
    if (!upload_path.empty() && upload_path.back() != '/') {
        upload_path += '/';
    }
    createUploadDirectory();
}

void ImageUploader::setMaxFileSize(size_t size) {
    max_file_size = size;
}

void ImageUploader::setAllowedTypes(const vector<string>& types) {
    allowed_types = types;
}

string ImageUploader::generateFilename(const string& extension) {
    // 使用时间戳 + 随机数生成唯一文件名
    auto now = chrono::system_clock::now();
    auto timestamp = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
    
    uniform_int_distribution<int> dist(1000, 9999);
    int random_num = dist(rng);
    
    stringstream ss;
    ss << timestamp << "_" << random_num << "." << extension;
    return ss.str();
}

string ImageUploader::extractExtension(const string& filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == string::npos) {
        return "";
    }
    
    string ext = filename.substr(dot_pos + 1);
    transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

bool ImageUploader::isValidImageType(const string& extension) {
    return find(allowed_types.begin(), allowed_types.end(), extension) != allowed_types.end();
}

bool ImageUploader::createUploadDirectory() {
    // 创建目录（递归）
    string path = upload_path;
    size_t pos = 0;
    
    while ((pos = path.find('/', pos + 1)) != string::npos) {
        string dir_path = path.substr(0, pos);
        mkdir(dir_path.c_str(), 0755);
    }
    
    // 创建最终目录
    mkdir(upload_path.c_str(), 0755);
    
    // 验证目录是否存在
    struct stat st;
    return (stat(upload_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
}

map<string, string> ImageUploader::parseMultipartData(const string& content_type, const string& body) {
    map<string, string> result;
    
    string boundary = extractBoundary(content_type);
    if (boundary.empty()) {
        return result;
    }
    
    vector<string> parts = splitByBoundary(body, boundary);
    
    for (const string& part : parts) {
        if (part.empty()) continue;
        
        auto field_data = parseFormField(part);
        for (const auto& pair : field_data) {
            result[pair.first] = pair.second;
        }
    }
    
    return result;
}

string ImageUploader::extractBoundary(const string& content_type) {
    size_t boundary_pos = content_type.find("boundary=");
    if (boundary_pos == string::npos) {
        return "";
    }
    
    return content_type.substr(boundary_pos + 9);
}

vector<string> ImageUploader::splitByBoundary(const string& body, const string& boundary) {
    vector<string> parts;
    string delimiter = "--" + boundary;
    
    size_t start = 0;
    size_t end = body.find(delimiter, start);
    
    while (end != string::npos) {
        if (start != end) {
            string part = body.substr(start, end - start);
            parts.push_back(part);
        }
        
        start = end + delimiter.length();
        end = body.find(delimiter, start);
    }
    
    // 添加最后一部分
    if (start < body.length()) {
        string part = body.substr(start);
        parts.push_back(part);
    }
    
    return parts;
}

map<string, string> ImageUploader::parseFormField(const string& field_data) {
    map<string, string> result;
    
    // 分割头部和内容
    size_t header_end = field_data.find("\r\n\r\n");
    if (header_end == string::npos) {
        header_end = field_data.find("\n\n");
        if (header_end == string::npos) {
            return result;
        }
    }
    
    string headers = field_data.substr(0, header_end);
    string content = field_data.substr(header_end + 4); // 跳过\r\n\r\n
    
    // 解析Content-Disposition头
    string content_disposition = getContentDisposition(headers);
    if (content_disposition.empty()) {
        return result;
    }
    
    // 提取字段名
    size_t name_start = content_disposition.find("name=\"");
    if (name_start != string::npos) {
        name_start += 6;
        size_t name_end = content_disposition.find("\"", name_start);
        if (name_end != string::npos) {
            string field_name = content_disposition.substr(name_start, name_end - name_start);
            
            // 如果是文件字段，也提取文件名
            size_t filename_start = content_disposition.find("filename=\"");
            if (filename_start != string::npos) {
                filename_start += 10;
                size_t filename_end = content_disposition.find("\"", filename_start);
                if (filename_end != string::npos) {
                    string filename = content_disposition.substr(filename_start, filename_end - filename_start);
                    result["filename"] = filename;
                    result["file_content"] = content;
                }
            } else {
                result[field_name] = trim(content);
            }
        }
    }
    
    return result;
}

string ImageUploader::getContentDisposition(const string& headers) {
    size_t pos = headers.find("Content-Disposition:");
    if (pos == string::npos) {
        return "";
    }
    
    size_t line_end = headers.find("\r\n", pos);
    if (line_end == string::npos) {
        line_end = headers.find("\n", pos);
        if (line_end == string::npos) {
            line_end = headers.length();
        }
    }
    
    return headers.substr(pos + 20, line_end - pos - 20); // 20 = strlen("Content-Disposition:")
}

string ImageUploader::extractFilename(const string& content_disposition) {
    size_t filename_start = content_disposition.find("filename=\"");
    if (filename_start == string::npos) {
        return "";
    }
    
    filename_start += 10; // strlen("filename=\"")
    size_t filename_end = content_disposition.find("\"", filename_start);
    if (filename_end == string::npos) {
        return "";
    }
    
    return content_disposition.substr(filename_start, filename_end - filename_start);
}

string ImageUploader::getMimeType(const string& extension) {
    if (extension == "jpg" || extension == "jpeg") return "image/jpeg";
    if (extension == "png") return "image/png";
    if (extension == "gif") return "image/gif";
    if (extension == "webp") return "image/webp";
    return "application/octet-stream";
}

string ImageUploader::trim(const string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}