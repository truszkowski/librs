#include "RS.hh"
#include "Time.hh"
#include "Http.hh"

// C
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

// C++
#include <cerrno>
#include <cstdlib>
#include <iostream>

// BOOST
#include <boost/regex.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

// regexy
#include "RS_regex.hh"

using namespace std;
using namespace boost;

// @brief: do sciagania plikow
static Http http;

// @brief: oczekiwanie z roznych powodow:
static const size_t rs_waiting_try_later =  60; 
static const size_t rs_waiting_busy      = 120; 
static const size_t rs_waiting_rivalry   =  60;
static const size_t rs_waiting_limit     = 120;

// @brief: lokalizacja katalogow na pliki:
static filesystem::path rs_download_dir_path = "./d/"; // !TODO!boost::filesystem!
static filesystem::path rs_sessions_dir_path = "./s/"; // !TODO!boost::filesystem!

// @brief: strumien, jak z C, do zapisu logow
static FILE *rs_log = NULL;

// @brief: zapis po logow, jak printf z C
static void rsprintf(const char *fmt, ...)
{
  if (!rs_log) return;

  char stamp[Time::stamp_length];
  fprintf(rs_log, "%s [RS] ", Time::stamp(stamp));
  
  va_list args;
  va_start(args, fmt);
  vfprintf(rs_log, fmt, args);
  va_end(args);

  fprintf(rs_log, "\n");
  fflush(rs_log);
}

// @brief: generowanie sciezki do pliku
static const filesystem::path &rs_sessions_get_path(const string &url, const char *suffix)
{
  static filesystem::path path;
  path = rs_sessions_dir_path / (url.substr(url.rfind('/')) + "-" + Time::stamp() + suffix);
  return path;
}

// @brief: generowanie sciezki do pliku
static const filesystem::path &rs_download_get_path(const string &url)
{
  static filesystem::path path;
  path = rs_download_dir_path / url.substr(url.rfind('/'));
  return path;
}

// @brief: informacje o aktualnie pobieranym pliku
static RS::Info rs_info;
// @brief: ochrona danych
static boost::mutex rs_mutex;
// @brief: zmienna warunekowa
static boost::condition_variable rs_cond;
// @brief: czy jest co sciagac
static bool rs_is_ready = false;

struct DExc { };
struct DAgain : public DExc { };
struct DBreak : public DExc { };
struct DAbort : public DExc { 
  DAbort(RS::Status s) : status(s) { }
  const RS::Status status;
};

static void rs_wait(RS::Status pre, RS::Status post, size_t secs)
{
  {
    unique_lock<mutex> lock(rs_mutex);
    rs_info.status = pre;
    rs_info.waiting = secs + 1;
  }

  while (--rs_info.waiting) sleep(1);

  unique_lock<mutex> lock(rs_mutex);
  rs_info.status = post;
}

static uint64_t rs_progress_start = 0;

static bool rs_progress_callback(const char *, size_t len, void *)
{
  uint64_t now = Time::in_usec(),
           dif = now - rs_progress_start;

  rs_info.bytes += len;
  rs_info.usecs += dif;
  rs_info.tmpspeed = ((long double)len) / ((long double)dif) * 1.0e6;
  rs_progress_start = now;
  return true;
}

struct rs_loop {
  void operator()(void)
  {
    while (true) {
      rsprintf("Oczekiwanie na nowe zadanie...");

      {
        unique_lock<mutex> lock(rs_mutex);
        while (!rs_is_ready)
          rs_cond.wait(lock);
    
        if (rs_info.url == "") {
          rsprintf("Pusty utr, watek pobierajacy konczy dzialanie");
          return;
        }
      }

      rsprintf("Nowy plik do pobrania: '%s'", rs_info.url.c_str());
      uint32_t tries = 0;

      while (true) {
        try { 
          string url = rs_info.url;
    
          stage_1(url);
          stage_2(url);
          stage_3(url);
        
          rsprintf("Plik pobrany: '%s', %lluB w %llu.%.3llu sek (%.3f KB/s)",
              rs_info.url.c_str(), rs_info.bytes, rs_info.usecs/100000, (rs_info.usecs/1000)%1000, 
              1.0e3*(((double)rs_info.bytes)/((double)rs_info.usecs)));

          unique_lock<mutex> lock(rs_mutex);
          rs_info.status = RS::Downloaded;
          rs_is_ready = false;
          break; // wychodzimy z 'while'
        }
        catch (DBreak) { // nastepna proba pobrania
          rsprintf("Nie udalo sie pobrac pliku: '%s' (proba %u)", rs_info.url.c_str(), tries);
          ++tries; 
          if (tries < 5) continue; // wracamy na poczatek 'while'
        } 
        catch (DAgain) { // od nowa jeszcze raz
          rsprintf("Nie udalo sie pobrac pliku: '%s', (proba %u i od nowa)", rs_info.url.c_str(), tries);
          tries = 0; 
          continue; // wracamy na poczatek 'while'
        } 
        catch (DAbort &e) { // odrzucono 
          rsprintf("Nie udalo sie pobrac pliku: '%s', odrzucono przez rapidshare.com", rs_info.url.c_str());

          unique_lock<mutex> lock(rs_mutex);
          rs_info.status = e.status; //RS::Canceled;
          rs_is_ready = false;
          break; // wychodzimu z 'while'
        }

        rsprintf("Nie udalo sie pobrac pliku: '%s', osiagniety limit prob", rs_info.url.c_str());

        unique_lock<mutex> lock(rs_mutex);
        rs_info.status = RS::Canceled;
        rs_is_ready = false;
        break; // wyjscie z petli
      }
    }
  }

  void stage_1(string &url)
  {
    rsprintf("Lacze z '%s'... (1)", url.c_str());
    char *buffer = NULL;
    size_t buflen = 0;

    try {
      int code = http.get(buffer, buflen, url.c_str());

      filesystem::ofstream head(rs_sessions_get_path(url, "-head1.http"));
      head.write(http.get_recv_header(), strlen(http.get_recv_header()));

      if (code == HTTP_OK) // Oczekujemy kodu 200
        rsprintf("Polaczono, code = %d, page = %p,%zd", code, buffer, buflen);
      else { 
        rsprintf("Polaczono, brak strony? code = %d, page = %p,%zd, sprobuje za chwile...", code, buffer, buflen);
        if (buffer) delete[] buffer;
        throw DBreak();
      }

      filesystem::ofstream page(rs_sessions_get_path(url, "-page1.html"));
      page.write(buffer, buflen);
    }
    catch (Exception &e) {
      rsprintf("Nastapil wyjatek: %s", e.what());
      throw DBreak();
    }

    // Sprawdzamy czy plik jest dostepny...
    if (regex_search(buffer, rs_regex_illegal_file) ||
        regex_search(buffer, rs_regex_illegal_file2) ||
        regex_search(buffer, rs_regex_not_available) ||
        regex_search(buffer, rs_regex_not_found)) {
      rsprintf("Plik '%s' jest niedostepny", url.c_str());
      delete[] buffer;
      //rs_info.status = RS::NotFound;
      throw DAbort(RS::NotFound);
    }

    // Szukamy nastepnego url'a...
    cmatch what;
    if (!regex_search(buffer, what, rs_regex_url)) {
      rsprintf("Brak odnosnika do nastepnej strony, sprobuje jeszcze raz...");
      delete[] buffer;
      throw DBreak();
    }

    url = what[1]; // ustawiamy nowy url
    delete[] buffer;
  }

  void stage_2(string &url)
  {
    rsprintf("Lacze z '%s' (2)", url.c_str());
    char *buffer = NULL;
    size_t buflen = 0;

    try {
      int code = http.get(buffer, buflen, url.c_str(), "dl.start=Free");

      filesystem::ofstream head(rs_sessions_get_path(url, "-head2.http"));
      head.write(http.get_recv_header(), strlen(http.get_recv_header()));

      if (code == HTTP_OK) // Oczekujemy kodu 200
        rsprintf("Polaczono, code = %d, page = %p,%zd", code, buffer, buflen);
      else {
        rsprintf("Polaczono, brak strony? code = %d, page = %p,%zd, try again", code, buffer, buflen);
        if (buffer) delete[] buffer;
        throw DBreak();
      }

      filesystem::ofstream page(rs_sessions_get_path(url, "-page2.html"));
      page.write(buffer, buflen);
    }
    catch (Exception &e) {
      rsprintf("Nastapil wyjatek: %s", e.what());
      throw DBreak();
    }

    if (regex_search(buffer, rs_regex_try_later)) {
      rsprintf("Sprobuje za chwile...");
      rs_wait(RS::Waiting, RS::Preparing, rs_waiting_try_later);
      delete[] buffer;
      throw DAgain();
    }

    if (regex_search(buffer, rs_regex_reached_limit)) {
      rsprintf("Wyczerpal sie limit pobran, czakam...");
      rs_wait(RS::Limit, RS::Preparing, rs_waiting_limit);
      delete[] buffer;
      throw DAgain();
    }

    if (regex_search(buffer, rs_regex_server_busy)) {
      rsprintf("Serwery sa przeciazone, czekam...");
      rs_wait(RS::Busy, RS::Preparing, rs_waiting_busy);
      delete[] buffer;
      throw DAgain();
    }

    if (regex_search(buffer, rs_regex_already_downloading)) {
      rsprintf("Ktos o tym samym IP pobiera juz jakis plik, czekam...");
      rs_wait(RS::Rivalry, RS::Preparing, rs_waiting_rivalry);
      delete[] buffer;
      throw DAgain();
    }

    size_t wait_for = 0;
    string ssize;
    cmatch what;

    if (regex_search(buffer, what, rs_regex_time)) {
      wait_for = strtol(what[1].str().c_str(), 0, 10) + 5;
      rsprintf("Czekam %zd sek...", wait_for);
    } else {
      wait_for = 5;
      rsprintf("Nieznany czas oczekiwania, sprobuje %zd sek...", wait_for);
    }

    if (!regex_search(buffer, what, rs_regex_size)) {
      rsprintf("Nieznany rozmiar pliku, jeszcze raz...");
      delete[] buffer;
      throw DBreak();
    }
    ssize = what[1];
    rs_info.all_bytes = 1000*strtol(ssize.c_str(), 0, 10);

    try {
      vector<pair<string, string> > srvs;
      for (cregex_iterator it(buffer, buffer+buflen, rs_regex_server), end; it != end; ++it)
        srvs.push_back(make_pair<string, string>((*it)[2], (*it)[1]));

      if (!srvs.size()) {
        rsprintf("Brak serwerow, przerywam...");
        throw DBreak();
      }

      size_t i = 0;
      while (rs_favorites_servers[i]) {
        size_t found = 0, end = srvs.size();

        while (found < end) {
          if (srvs[found].first == rs_favorites_servers[i]) break;
          ++found;
        }

        if (found == end) {
          rsprintf("Brak serwera '%s', szukam dalej...", rs_favorites_servers[i]);
          ++i;
          continue;
        }

        rsprintf("Wybieram serwer '%s'...", rs_favorites_servers[i]);
        url = srvs[found].second;
        break;
      }

      if (!rs_favorites_servers[i]) {
        url = srvs[0].second;
        rsprintf("Brak jakiegokolwiek ulubionego serwera, wybieram pierwszy z brzegu '%s'",
            srvs[0].first.c_str());
      }
    }
    catch (DBreak) { delete[] buffer; throw; }
    catch (DExc &e) { delete[] buffer; throw e; }

    rs_wait(RS::Waiting, RS::Preparing, wait_for);
    delete[] buffer;
  }

  void stage_3(string &url)
  {
    rsprintf("Lacze z '%s' (3)", url.c_str());
    
    rs_progress_start = Time::in_usec();
    rs_info.status = RS::Downloading;
    rs_info.bytes = 0;
    rs_info.tmpspeed = 0;
    off_t length = 0;

    try { // TODO
      int code = http.get(rs_download_get_path(rs_info.url.c_str()).string().c_str(), length, url.c_str(), 
          "mirror=", NULL, rs_progress_callback, NULL);
     
      filesystem::ofstream head(rs_sessions_get_path(url, "-head3.http"));
      head.write(http.get_recv_header(), strlen(http.get_recv_header()));

      if (code == HTTP_OK) // Oczekujemy kodu 200
        rsprintf("Polaczono, code = %d, length = %lld", code, length);
      else {
        rsprintf("Polaczono, brak strony? code = %d, length = %lld, try again", code, length);
        throw DBreak();
      }

      //string clen = http.get_recv_header("content-length");
      //rsprintf("");
    } 
    catch (Exception &e) {
      rsprintf("Nastapil wyjatek: %s", e.what());
      throw DBreak();
    }  
  }

} rs_loop_object;

// @brief: watek w petli sciagajacy pliki z rapidshare.com
static boost::thread rs_thread(rs_loop_object);

// @brief: kontruktor - nic nie robi
RS::RS(void) throw() { }
// @brief: destruktor - nic nie robi
RS::~RS(void) throw() { }

void RS::download(const char *url) throw(Exception)
{
  unique_lock<mutex> lock(rs_mutex);

  // jak status nie jest taki jak nizej to ok
  if (rs_info.status != None && rs_info.status != Downloaded &&
      rs_info.status != Canceled && rs_info.status != NotFound)
    throw EExternal("teraz sie pobierany plik: '%s' (status: '%s')",
        rs_info.url.c_str(), strstatus(rs_info.status));

  if (!boost::regex_match(url, rs_regex_correct_url)) {
    rsprintf("Nieprawidlowy url: '%s'", url);
    throw EExternal("nieprawidlowy url '%s'", url);
  }

  rs_info.status = Preparing;
  rs_info.url = url;
  rs_info.bytes = 0;
  rs_info.all_bytes = 0;
  rs_info.usecs = 0;
  rs_info.tmpspeed = 0.;
  rs_info.waiting = 0;

  rs_is_ready = true;
  rs_cond.notify_one();
  rsprintf("Dodano plik do pobrania: '%s'", url);
}

const char *RS::strstatus(Status s)
{
  switch (s) {
    case None:
      return "nic do roboty";
    case Downloaded:
      return "plik zostal pobrany";
    case Canceled:
      return "pobieranie zostalo anulowane";
    case NotFound:
      return "nie znaleziono pliku";
    case Preparing:
      return "przygotowanie do pobierania";
    case Downloading:
      return "plik jest pobierany";
    case Waiting:
      return "oczekiwanie na pobranie";
    case Later:
      return "sprobuje za chwile";
    case Rivalry:
      return "ktos rowniez korzysta z rapidshare.com";
    case Limit:
      return "wyczerpany limit";
    case Busy:
      return "serwery sa przeciezone";
    default:
      return "nieznany status";
  };
}

void RS::set_download_dir(const char *path) throw(Exception)
{
  if (!filesystem::is_directory(path)) 
    throw EExternal("'%s' nie istnieje lub nie jest katalogiem", path);

  rs_download_dir_path = path;
}

void RS::set_sessions_dir(const char *path) throw(Exception)
{
  if (!filesystem::is_directory(path)) 
    throw EExternal("'%s' nie istnieje lub nie jest katalogiem", path);

  rs_sessions_dir_path = path;
}

void RS::set_log_file(const char *path) throw(Exception)
{
  if (!(rs_log = fopen(path, "a"))) 
    throw EExternal("nie mozna otworzyc pliku: '%s': %d, %s",
        path, errno, strerror(errno));

  rsprintf("Loguje do pliku: '%s'", path);
}

void RS::set_log_stderr(void) throw(Exception)
{
  rs_log = stderr;
  rsprintf("Loguje na stderr");
}

void RS::get_info(Info &info) throw()
{ 
  unique_lock<mutex> lock(rs_mutex); 
  info = rs_info; 
}

void RS::close(void) throw(Exception)
{
  {
    unique_lock<mutex> lock(rs_mutex);

    // jak status nie jest taki jak nizej to ok
    if (rs_info.status != None && rs_info.status != Downloaded &&
        rs_info.status != Canceled && rs_info.status != NotFound) 
      throw EExternal("nie mozna zamknac, plik '%s' jest pobierany (status: '%s')",
          rs_info.url.c_str(), strstatus(rs_info.status));
  
    rs_info.status = Preparing;
    rs_info.url = "";
    rs_info.bytes = 0;
    rs_info.all_bytes = 0;
    rs_info.usecs = 0;
    rs_info.tmpspeed = 0;
    rs_info.waiting = 0;
    rs_is_ready = true;
    rs_cond.notify_one();
  }

  rs_thread.join();
  rsprintf("Zamknieto watek pobierajacy");
}

