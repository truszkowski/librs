#ifndef __HTTP_HH__
#define __HTTP_HH__

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#elif _FILE_OFFSET_BITS != 64
#error _FILE_OFFSET_BITS != 64
#endif

#include <cstdlib>
#include <cstring>
#include <map>

#include <curl/curl.h>
#include <httpd/httpd.h>

#include <Exception.hh>

// @brief: blad wychwycony na poziomie http.
class HttpError : public Exception {
  public:
    HttpError(int code) throw() : code(code) { }
    ~HttpError(void) throw() { }

    const char *what(void) const throw();
    const int code; // kod odpowiedzi http
};

// @brief: blad wychwycony na poziomie CURLa
class CurlError : public Exception {
  public: 
    CurlError(CURLcode code) throw() : code(code) { }
    ~CurlError(void) throw() { }

    const char *what(void) const throw();
    const CURLcode code; // kod bledu curla
};

// @brief: Nie mozna sie polaczyc
DEF_EXC(ECouldntConnect, Exception);
// @brief: Nie mozna dostac nazwy ip hosta
DEF_EXC(ECouldntResolveHost, Exception);
// @brief: Nie mozna zapisac odebranych danych
DEF_EXC(ECouldntWrite, Exception);
// @brief: Przekroczony timeout na operacje
DEF_EXC(EOperationTimeout, Exception);

class Http {
  public:
    Http(void) throw();
    ~Http(void) throw();

    // @brief: callback, mozna na bierzaco monitorowac proces pobierania strony/pliku itd
    // zwracajac false - anulujemy zadanie sciagania.
    typedef bool (*callback_t)(const char *buf, size_t len, void *arg);

    // @brief: mapa pol naglowka http (pola set-cookie sa sklejane)
    typedef std::map<std::string, std::string> Headers;

    // @brief: pobranie strony do pamieci, pod wskaznik 'page'
    // @return: zwraca kod http (patrz: <httpd/httpd.h>)
    int get(char *&page, size_t &len, const char *url, 
        const char *post = NULL, const char *cookies = NULL,
        callback_t fn = NULL, void *arg = NULL) throw(Exception);

    // @brief: pobranie strony na dysk, pod sciezke 'path'
    // @return: zwraca kod http (patrz: <httpd/httpd.h>)
    int get(const char *path, off_t &len, const char *url, 
        const char *post = NULL, const char *cookies = NULL,
        callback_t fn = NULL, void *arg = NULL) throw(Exception);

    // @brief: pobranie czystego naglowka http
    const char *get_recv_header(void) const { return header_; }
    // @brief: pobranie mapy otrzymanych naglowkow
    const Headers &get_headers(void) const { return headers_map_; }
    // @brief: pobranie wskazanego pola 'key' z naglowka
    const char *get_recv_header(const char *key) const;
    // @brief: pobranie pola 'location' z naglowka
    const char *get_recv_redirect(void) const;
    // @brief: pobranie pola 'set-cookie' z naglowka
    // zapamietuje tylko do pierwszego ';' w polu - reszta pomijana, 
    // jesli w naglowku jest wiele pol, to sa sklejane w jeden.
    // XXX: moze da sie to zrobic tak aby jednak pamietac wszystko?
    const char *get_recv_cookies(void) const;

    // @brief: domyslny timeout na operacje odczytu/zapisu
    static const int default_rdwr_timeout_in_msec = -1;
    // @brief: domyslny timeout na operacje polaczenia
    static const int default_conn_timeout_in_msec = 10000;

    // @brief: zmiana timeoutu na operacje odczytu/zapisu
    void set_conn_timeout(int t) { conn_tout_ = t; }
    // @brief: zmiana timeoutu na operacje polaczenia
    void set_rdwr_timeout(int t) { rdwr_tout_ = t; }
    // @brief: pobranie aktualnego timeoutu na operacje odczytu/zapisu
    int get_conn_timeout(void) const { return conn_tout_; }
    // @brief: pobranie aktualnego timeoutu na operacje polaczenia
    int get_rdwr_timeout(void) const { return rdwr_tout_; }

    // @brief: ustaw wypisywanie sie curla, co sie dzieje
    void set_verbose(bool yes_or_no) { verbose_ = yes_or_no; }

  private:
    Http(const Http &); // non-copyable
    static const size_t header_def_len = 8192; // domyslny rozmiar naglowka http
    static const size_t page_def_len = 8192; // domyslny rozmiar strony

    CURL *curl; // dla CURLa
    int conn_tout_, rdwr_tout_; // timeouty

    char *header_; // bufor na naglowek http
    size_t header_len_, header_max_; // rozmiar w/w buforu
    Headers headers_map_; // mapa pol w naglowku
    bool verbose_;

    void clear(void); // zresetowanie zasobow, zresetowanie CURLa (this->curl)
    int do_analyze(void); // analiza naglowka http.

    // funkcje do odczytu danych z sieci (naglowek, strona, plik)
    static size_t head_fn(void *buf, size_t size, size_t nmemb, void *arg);
    static size_t page_fn(void *buf, size_t size, size_t nmemb, void *arg);
    static size_t file_fn(void *buf, size_t size, size_t nmemb, void *arg);
};


#endif
