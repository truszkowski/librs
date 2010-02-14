#include "Http.hh"

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

using std::cerr;
using std::endl;
using std::map;

static curl_slist *attach_common_headers(void) throw()
{
  struct my_curl_list { 
    curl_slist *slist;

    my_curl_list(void) : slist(NULL)
    {
      if (!(slist = curl_slist_append(slist, "User-Agent: Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.1.8) Gecko/20071028 PLD/3.0 (Th) BonEcho/2.0.0.8")))
        throw EInternal("curl_slist_append() - fix it!");
      if (!(slist = curl_slist_append(slist, "Keep-Alive: 300")))
        throw EInternal("curl_slist_append() - fix it!");
      if (!(slist = curl_slist_append(slist, "Connection: close"))) 
        throw EInternal("curl_slist_append() - fix it!");
    }
    ~my_curl_list(void)
    {
      curl_slist_free_all(slist);
    }
  };

  static my_curl_list list;
  return list.slist;
}

const char *HttpError::what(void) const throw()
{
  switch (code) {
    case HTTP_CONTINUE:
      return "HTTP_CONTIMUE 100";
    case HTTP_SWITCHING_PROTOCOLS:
      return "HTTP_SWITCHING_PROTOCOLS 101";
    case HTTP_PROCESSING:
      return "HTTP_PROCESSING 102";
    case HTTP_OK:
      return "HTTP_OK 200";
    case HTTP_CREATED:
      return "HTTP_CREATED 201";
    case HTTP_ACCEPTED:
      return "HTTP_ACCEPTED 202";
    case HTTP_NON_AUTHORITATIVE:
      return "HTTP_NON_AUTHORITATIVE 203";
    case HTTP_NO_CONTENT:
      return "HTTP_NO_CONTENT 204";
    case HTTP_RESET_CONTENT:
      return "HTTP_RESET_CONTENT 205";
    case HTTP_PARTIAL_CONTENT:
      return "HTTP_PARTIAL_CONTENT 206";
    case HTTP_MULTI_STATUS:
      return "HTTP_MULTI_STATUS 207";
    case HTTP_MULTIPLE_CHOICES:
      return "HTTP_MULTIPLE_CHOICES 300";
    case HTTP_MOVED_PERMANENTLY:
      return "HTTP_MOVED_PERMANENTLY 301";
    case HTTP_MOVED_TEMPORARILY:
      return "HTTP_MOVED_TEMPORARILY 302";
    case HTTP_SEE_OTHER:
      return "HTTP_SEE_OTHER 303";
    case HTTP_NOT_MODIFIED:
      return "HTTP_NOT_MODIFIED 304";
    case HTTP_USE_PROXY:
      return "HTTP_USE_PROXY 305";
    case HTTP_TEMPORARY_REDIRECT:
      return "HTTP_TEMPORARY_REDIRECT 307";
    case HTTP_BAD_REQUEST:
      return "HTTP_BAD_REQUEST 400";
    case HTTP_UNAUTHORIZED:
      return "HTTP_UNAUTHORIZED 401";
    case HTTP_PAYMENT_REQUIRED:
      return "HTTP_PAYMENT_REQUIRED 402";
    case HTTP_FORBIDDEN:
      return "HTTP_FORBIDDEN 403";
    case HTTP_NOT_FOUND:
      return "HTTP_NOT_FOUND 404";
    case HTTP_METHOD_NOT_ALLOWED:
      return "HTTP_METHOD_NOT_ALLOWED 405";
    case HTTP_NOT_ACCEPTABLE:
      return "HTTP_NOT_ACCEPTABLE 406";
    case HTTP_PROXY_AUTHENTICATION_REQUIRED:
      return "HTTP_PROXY_AUTHENTICATION_REQUIRED 407";
    case HTTP_REQUEST_TIME_OUT:
      return "HTTP_REQUEST_TIME_OUT 408";
    case HTTP_CONFLICT:
      return "HTTP_CONFLICT 409";
    case HTTP_GONE:
      return "HTTP_GONE 410";
    case HTTP_LENGTH_REQUIRED:
      return "HTTP_LENGTH_REQUIRED 411";
    case HTTP_PRECONDITION_FAILED:
      return "HTTP_PRECONDITION_FAILED 412";
    case HTTP_REQUEST_ENTITY_TOO_LARGE:
      return "HTTP_REQUEST_ENTITY_TOO_LARGE 413";
    case HTTP_REQUEST_URI_TOO_LARGE:
      return "HTTP_REQUEST_URI_TOO_LARGE 414";
    case HTTP_UNSUPPORTED_MEDIA_TYPE:
      return "HTTP_UNSUPPORTED_MEDIA_TYPE 415";
    case HTTP_RANGE_NOT_SATISFIABLE:
      return "HTTP_RANGE_NOT_SATISFIABLE 416";
    case HTTP_EXPECTATION_FAILED:
      return "HTTP_EXPECTATION_FAILED 417";
    case HTTP_UNPROCESSABLE_ENTITY:
      return "HTTP_UNPROCESSABLE_ENTITY 422";
    case HTTP_LOCKED:
      return "HTTP_LOCKED 423";
    case HTTP_FAILED_DEPENDENCY:
      return "HTTP_FAILED_DEPENDENCY 424";
    case HTTP_UPGRADE_REQUIRED:
      return "HTTP_UPGRADE_REQUIRED 426";
    case HTTP_INTERNAL_SERVER_ERROR:
      return "HTTP_INTERNAL_SERVER_ERROR 500";
    case HTTP_NOT_IMPLEMENTED:
      return "HTTP_NOT_IMPLEMENTED 501";
    case HTTP_BAD_GATEWAY:
      return "HTTP_BAD_GATEWAY 502";
    case HTTP_SERVICE_UNAVAILABLE:
      return "HTTP_SERVICE_UNAVAILABLE 503";
    case HTTP_GATEWAY_TIME_OUT:
      return "HTTP_GATEWAY_TIME_OUT 504";
    case HTTP_VERSION_NOT_SUPPORTED:
      return "HTTP_VERSION_NOT_SUPPORTED 505";
    case HTTP_VARIANT_ALSO_VARIES:
      return "HTTP_VARIANT_ALSO_VARIES 506";
    case HTTP_INSUFFICIENT_STORAGE:
      return "HTTP_INSUFFICIENT_STORAGE 507";
    case HTTP_NOT_EXTENDED:
      return "HTTP_NOT_EXTENDED 510";
    default:
      return "unrecognized http code";
  }
}

const char *CurlError::what(void) const throw()
{
  return curl_easy_strerror(code);
}

Http::Http(void) throw() 
: curl(NULL),
  conn_tout_(default_conn_timeout_in_msec), 
  rdwr_tout_(default_rdwr_timeout_in_msec), 
  header_(NULL), header_len_(0), header_max_(0), 
  verbose_(false)
{
  if (!(curl = curl_easy_init()))
    throw EInternal("nie mozna zaalokowac pamieci dla CURLa");
}

Http::~Http(void) throw()
{
  if (curl) curl_easy_cleanup(curl);
  if (header_) delete[] header_;
}

void Http::clear(void)
{
  if (curl) curl_easy_reset(curl);
  
  if (header_) {
    delete[] header_;
    header_ = NULL;
    header_len_ = 0;
    header_max_ = 0;
  }

  headers_map_.clear();
}

size_t Http::head_fn(void *buf, size_t sz, size_t nmemb, void *arg) 
{
  size_t size = sz * nmemb;
  Http *h = (Http*)arg;

  if (!h->header_max_) {
    size_t max = (size+1) > h->header_def_len ? (size+1):h->header_def_len;
    if (!(h->header_ = new(std::nothrow) char[max])) return CURLE_WRITE_ERROR; 
    h->header_max_ = max;
  } else if (h->header_len_ + size + 1 >= h->header_max_) {
    size_t max = (h->header_len_ + size + 1) > (2*h->header_max_) ? 
      (h->header_len_ + size + 1) : (2*h->header_max_);

    char *ptr = new(std::nothrow) char[max];
    if (!ptr) return CURLE_WRITE_ERROR;

    strncpy(ptr, h->header_, h->header_len_+1);
    assert(h->header_[h->header_len_]);

    h->header_max_ = max;
  }

  assert(h->header_len_ + size + 1 <= h->header_max_);
  if (size) memcpy(h->header_+h->header_len_, (char*)buf, size);
  h->header_len_ += size;
  h->header_[h->header_len_] = 0;

  return size;
}

struct page_task {
  Http *http;
  char **page;
  size_t &len;
  size_t real;
  Http::callback_t fn;
  void *arg;
  
  page_task(Http *http, char **page, size_t &len, Http::callback_t fn, void *arg)
    : http(http), page(page), len(len), real(0), fn(fn), arg(arg) 
  { 
    (*page) = NULL; len = 0; 
  }
  ~page_task(void) 
  { 
    if (page) delete[] (*page);
  }
};

size_t Http::page_fn(void *buf, size_t sz, size_t nmemb, void *arg) 
{
  size_t size = sz*nmemb;
  page_task *task = (page_task*)arg;

  if (!task->real) {
    size_t max = page_def_len > (size+1) ? page_def_len:(size+1);
    if (!((*task->page) = new(std::nothrow) char[max])) return CURLE_WRITE_ERROR;
    
    task->real = max;
  } else if (task->len + size + 1 > task->real) {
    size_t dif = task->len + size + 1,
           max = (2*task->real < dif) ? dif : (2*task->real);
  
    char *ptr = new(std::nothrow) char[max];
    if (!ptr) return CURLE_WRITE_ERROR;
    memcpy(ptr, *task->page, task->len);
    delete[] (*task->page);
    
    (*task->page) = ptr;
    task->real = max;
  }
  
  memcpy((*task->page) + task->len, buf, size);
  task->len += size;
  (*task->page)[task->len] = 0;
  
  if (task->fn && !task->fn((const char*)buf, size, task->arg))
    return CURLE_WRITE_ERROR;

  return size;
}

int Http::get(char *&page, size_t &len, const char *url, 
    const char *post, const char *cookies,
    Http::callback_t fn, void *arg) throw(Exception)
{
  clear();
  page_task task(this, &page, len, fn, arg);

  if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK)
    throw EExternal("nie mozna ustawic urla: '%s'", url);
  if (post && curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post) != CURLE_OK)
    throw EExternal("nie mozna ustawic POSTa: '%s'", post);
  if (cookies && curl_easy_setopt(curl, CURLOPT_COOKIE, cookies) != CURLE_OK)
    throw EExternal("nie mozna ustawic ciastek: '%s'", cookies);
  if (rdwr_tout_ > 0 && curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, rdwr_tout_) != CURLE_OK)
    throw EExternal("nie mozna ustawic timeoutu na odczyt/zapis: '%d'", rdwr_tout_);
  if (conn_tout_ > 0 && curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, conn_tout_) != CURLE_OK)
    throw EExternal("nie mozna ustawic timeoutu na polaczenie: '%d'", conn_tout_);
  if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, attach_common_headers()) != CURLE_OK)
    throw EExternal("nie mozna ustawic dodatkowych naglowkow");
  if (verbose_ && curl_easy_setopt(curl, CURLOPT_VERBOSE, 1) != CURLE_OK)
    throw EInternal("nie mozna ustawic trybu glosnego dla CURLa");
  if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, page_fn) != CURLE_OK)
    throw EInternal("nie mozna ustawic funkcji do odbioru danych");
  if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, &task) != CURLE_OK)
    throw EInternal("nie mozna ustawic argumentu dla funkcji do odbioru danych");
  if (curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, head_fn) != CURLE_OK)
    throw EInternal("nie mozna ustawic funkcji do odbioru naglowkow");
  if (curl_easy_setopt(curl, CURLOPT_HEADERDATA, this) != CURLE_OK)
    throw EInternal("nie mozna ustawic argumentu dla funkcji do odbioru naglowkow");

  CURLcode cd = curl_easy_perform(curl);
  if (cd != CURLE_OK) {
    if (cd == CURLE_OPERATION_TIMEOUTED) 
      throw EOperationTimeout();
    if (cd == CURLE_COULDNT_CONNECT)
      throw ECouldntConnect();
    if (cd == CURLE_COULDNT_RESOLVE_HOST)
      throw ECouldntResolveHost();

    throw CurlError(cd);
  }

  int code = do_analyze();
  task.page = NULL; // zeby ~page_task() nie zwolnil pamieci, ktora zwracamy
  return code;
}

struct file_task {
  Http *http;
  const char *path;
  int fd;
  off_t &len;
  Http::callback_t fn;
  void *data;

  file_task(Http *http, const char *path, off_t &len, Http::callback_t fn, void *data)
    : http(http), path(path), fd(-1), len(len), fn(fn), data(data)
  { 
    if ((fd = open(path, O_CREAT|O_RDWR|O_TRUNC, 0644)) < 0) 
      throw EExternal("nie mozna otworzyc pliku '%s': %d, %s", 
          path, errno, strerror(errno));
    len = 0;
  }
  ~file_task(void) throw() 
  {
    if (fd >= 0 && close(fd)) 
      throw EInternal("nie mozna zamknac pliku '%s': %d, %s",
          path, errno, strerror(errno));
  }
};

size_t Http::file_fn(void *buf, size_t sz, size_t nmemb, void *data) {
  size_t size = sz*nmemb;
  file_task *task = (file_task*)data;
  int fd = task->fd;

  size_t done = 0;

  while (done < size) {
    int wr = write(fd, (char*)buf+done, size-done);
    if (wr < 0) {
      if (errno == EINTR || errno == EAGAIN) continue;
      return CURLE_WRITE_ERROR;
    }
    done += wr;
  }

  if (task->fn && !task->fn((const char*)buf, size, task->data))
    return CURLE_WRITE_ERROR;
  
  task->len += size;
  return size;
}

int Http::get(const char *path, off_t &len, const char *url, 
    const char *post, const char *cookies,
    Http::callback_t fn, void *arg) throw(Exception)
{
  clear();
  file_task task(this, path, len, fn, arg);

  if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK)
    throw EExternal("nie mozna ustawic urla: '%s'", url);
  if (post && curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post) != CURLE_OK)
    throw EExternal("nie mozna ustawic POSTa: '%s'", post);
  if (cookies && curl_easy_setopt(curl, CURLOPT_COOKIE, cookies) != CURLE_OK)
    throw EExternal("nie mozna ustawic ciastek: '%s'", cookies);
  if (rdwr_tout_ > 0 && curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, rdwr_tout_) != CURLE_OK)
    throw EExternal("nie mozna ustawic timeoutu na odczyt/zapis: '%d'", rdwr_tout_);
  if (conn_tout_ > 0 && curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, conn_tout_) != CURLE_OK)
    throw EExternal("nie mozna ustawic timeoutu na polaczenie: '%d'", conn_tout_);
  if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, attach_common_headers()) != CURLE_OK)
    throw EExternal("nie mozna ustawic dodatkowych naglowkow");
  if (verbose_ && curl_easy_setopt(curl, CURLOPT_VERBOSE, 1) != CURLE_OK)
    throw EInternal("nie mozna ustawic trybu glosnego dla CURLa");
  if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, file_fn) != CURLE_OK)
    throw EInternal("nie mozna ustawic funkcji do odbioru danych");
  if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, &task) != CURLE_OK)
    throw EInternal("nie mozna ustawic argumentu dla funkcji do odbioru danych");
  if (curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, head_fn) != CURLE_OK)
    throw EInternal("nie mozna ustawic funkcji do odbioru naglowkow");
  if (curl_easy_setopt(curl, CURLOPT_HEADERDATA, this) != CURLE_OK)
    throw EInternal("nie mozna ustawic argumentu dla funkcji do odbioru naglowkow");

  CURLcode cd = curl_easy_perform(curl);
  if (cd != CURLE_OK) {
    if (cd == CURLE_OPERATION_TIMEOUTED) 
      throw EOperationTimeout();
    if (cd == CURLE_COULDNT_CONNECT)
      throw ECouldntConnect();
    if (cd == CURLE_COULDNT_RESOLVE_HOST)
      throw ECouldntResolveHost();

    throw CurlError(cd);
  }

  return do_analyze();
}

int Http::do_analyze(void) 
{
  if (header_len_ < 10) 
    throw EInternal("zbyt krotki naglowek?");

  char *ptr = header_, *eol;
  headers_map_.clear();
  int code = 0;

  while ((eol = strstr(ptr, "\r\n"))) {
    if (eol == ptr) break;

    if (strncasecmp(ptr, "HTTP/", 5) == 0) {
      ptr += 5;
      while (ptr != eol && *ptr != ' ' && *ptr != '\t') ++ptr;
      if (ptr == eol) throw EInternal("nie mozna odczytac naglowka 'HTTP/<wersja> <kod>'?");
      while (ptr != eol && (*ptr == ' ' || *ptr == '\t')) ++ptr;
      code = strtol(ptr, NULL, 10);
      
      if (code < 100 || code >= 600) throw EInternal("odczytano nieprawidlowy numer kodu: %d", code);
    } else {
      char *to_change = NULL;
      std::pair<const char*, const char*> ent;
      ent.first = ptr;
      
      for (size_t i = 0; ptr[0] != ':'; ++i, ++ptr) {
        if (ptr == eol) throw EInternal("nie mozna odczytac naglowka?");
        if (ptr[0] >= 'A' && ptr[0] <= 'Z') ptr[0] -= ('A'-'a');
      }

      to_change = ptr;
      ptr[0] = 0; // oznaczamy koniec klucza
      ++ptr;

      while (ptr != eol && (*ptr == ' ' || *ptr == '\t')) ++ptr;
      ent.second = ptr;

      if (strcmp("set-cookie", ent.first)) {
        eol[0] = 0; // oznaczamy koniec wartosci
        headers_map_[ent.first] = ent.second;

        to_change[0] = ':';
        eol[0] = '\r';
      } else {
        // Dla cookies robimy dodatkowo tak:
        while (ptr[0] != ';') 
          if (ptr[0]) ++ptr; 
          else throw EInternal("nie mozna odczytac ciastka?");
        ptr[1] = 0; 

        Headers::iterator it = headers_map_.find(ent.first);
        if (it == headers_map_.end())
          headers_map_[ent.first] = ent.second;
        else 
          it->second  = it->second + " " + ent.second;

        to_change[0] = ':';
        ptr[1] = ';';
      }
    }

    ptr = eol + 2; // przewijamy na nastepny wpis
  }

  //for (Headers::const_iterator it = headers_map_.begin(); it != headers_map_.end(); ++it)
  //  cerr << "<> " << it->first << " => " << it->second << endl;

  if (!code) throw EInternal("brakujaca informacja o kodzie http?");
  if (code >= 400) throw HttpError(code);
  return code;
}

const char *Http::get_recv_header(const char *key) const
{
  Headers::const_iterator it;
  if ((it = headers_map_.find(key)) == headers_map_.end())
    return NULL;
  return it->second.c_str();
}

const char *Http::get_recv_cookies(void) const
{
  return get_recv_header("set-cookie");
}

const char *Http::get_recv_redirect(void) const
{
  return get_recv_header("location");
}


