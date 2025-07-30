#pragma once
#include <curl/curl.h>

#include <future>
#include <string>
#include <unordered_map>
#include <vector>
namespace rdm {
class HttpManager {
  HttpManager();
  CURLM* curl_multi;
  std::string baseUrl;

  enum RequestType {
    Get,
    Post,
  };

 public:
  static HttpManager* singleton();
  CURLM* getHandle() { return curl_multi; }
  void setBaseUrl(std::string baseUrl) { this->baseUrl = baseUrl; }

  struct Response {
    std::unordered_map<std::string, std::string> headers;
    std::vector<char> response;
    int statusCode;

    std::string getResponse() {
      if (response.empty()) return std::string("<empty response>");
      return std::string(response.begin(), response.end());
    }
  };

  struct Request {
    std::unordered_map<std::string, std::string> headers;
    std::function<void(Response&)> requestFinished;
    RequestType type;

    Request() { headers["Content-Type"] = "application/json"; }
  };

  std::future<HttpManager::Response> get(std::string url,
                                         Request rq = Request());
  std::future<HttpManager::Response> post(std::string url, std::string data,
                                          Request rq = Request());

 private:
  void handleWebRequest(CURL* handle, std::string url, Request rq,
                        std::promise<Response>* response);
};
}  // namespace rdm
