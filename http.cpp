#include "http.hpp"

#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/header.h>
#include <curl/multi.h>
#include <curl/urlapi.h>
#include <string.h>

#include <format>
#include <thread>

#include "logging.hpp"
#include "worker.hpp"

namespace rdm {
static HttpManager* _singleton = 0;

HttpManager::HttpManager() { curl_multi = curl_multi_init(); }

HttpManager* HttpManager::singleton() {
  if (!_singleton) _singleton = new HttpManager();
  return _singleton;
}

struct ResponseData {
  std::vector<char> data;
  size_t size;
};

static size_t __writeCallback(char* ptr, size_t size, size_t nmemb, void* ud) {
  size_t rsz = size * nmemb;
  ResponseData* data = (ResponseData*)ud;
  size_t oldSize = data->data.size();
  data->data.resize(data->data.size() + rsz + 1);
  memcpy(data->data.data() + data->size, ptr, rsz);
  data->size += rsz;
  return rsz;
}

void HttpManager::handleWebRequest(CURL* handle, std::string url, Request rq,
                                   std::promise<Response>* response) {
  if (!baseUrl.empty()) {
    CURLU* u = curl_url();
    curl_url_set(u, CURLUPART_URL, baseUrl.c_str(), 0);
    curl_url_set(u, CURLUPART_URL, url.c_str(), 0);
    char* _url;
    curl_url_get(u, CURLUPART_URL, &_url, 0);
    url = _url;
    curl_free(_url);
    curl_url_cleanup(u);
  }

  curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
  Response rsp;

  ResponseData rd;
  rd.size = 0;
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, __writeCallback);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &rd);
  struct curl_slist* list = NULL;
  for (auto& [name, value] : rq.headers) {
    curl_slist_append(list, std::format("{}: {}", name, value).c_str());
  }
  curl_easy_setopt(handle, CURLOPT_HTTPHEADER, list);
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, true);

  CURLcode code = curl_easy_perform(handle);
  if (code) {
    std::string errorMsg = curl_easy_strerror(code);
    Log::printf(LOG_ERROR, "curl_easy_perform error: %d '%s'", code,
                errorMsg.c_str());
    rsp.statusCode = -1;  // request couldn't be made
    rsp.response = std::vector<char>(errorMsg.begin(), errorMsg.end());
  } else {
    rsp.response = rd.data;
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &rsp.statusCode);
  }

  struct curl_header *prev, *h;
  prev = NULL;
  while ((h = curl_easy_nextheader(
              handle, CURLH_HEADER | CURLH_1XX | CURLH_TRAILER, 0, prev))) {
    rsp.headers[h->name] = h->value;
    prev = h;
  }

  char* ct = NULL;
  curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE, &ct);
  if (ct) {
    std::string cts = ct;
    rsp.headers["Content-Type"] = cts.substr(0, cts.find(';'));
  }

  response->set_value(rsp);

  delete response;
  // curl_multi_remove_handle(curl_multi, handle);
  curl_slist_free_all(list);
  curl_easy_cleanup(handle);
}

std::future<HttpManager::Response> HttpManager::get(std::string url,
                                                    Request rq) {
  std::promise<Response>* response = new std::promise<Response>();
  std::future<Response> future = response->get_future();
  rq.type = Get;
  CURL* handle = curl_easy_init();
  WorkerManager::singleton()->run(std::bind(&HttpManager::handleWebRequest,
                                            this, handle, url, rq, response));
  return future;
}

std::future<HttpManager::Response> HttpManager::post(std::string url,
                                                     std::string data,
                                                     Request rq) {
  std::promise<Response>* response = new std::promise<Response>();
  std::future<Response> future = response->get_future();
  rq.type = Get;
  CURL* handle = curl_easy_init();
  curl_easy_setopt(handle, CURLOPT_COPYPOSTFIELDS, data.c_str());
  WorkerManager::singleton()->run(std::bind(&HttpManager::handleWebRequest,
                                            this, handle, url, rq, response));
  return future;
}
};  // namespace rdm
