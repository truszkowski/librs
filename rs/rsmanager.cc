#include <Exception.hh>
#include <Time.hh>
#include <RS.hh>

// C
#include <signal.h>
#include <stdlib.h>

// BOOST
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

using namespace std;
using namespace boost;

// Glowny plik z kolejka zadan
const char *primary_queue = "primary.queue";
// Drugi plik z kolejka zadan, do niego dopisujemy nowe zadania
const char *extends_queue = "extends.queue";
// Tymczasowy plik na kolejkowanie zadan
const char *temporary_queue = "temporary.queue";
// Do tego katalogu pobieramy pliki
const char *download_dir = "./d/";
// Do tego katalogu zapisujemy strony html i naglowki http
const char *sessions_dir = "./s/";
// Do tego pliku RS loguje co robi
const char *log_file = "rs.log";

// Logowanie na ekran co sie dzieje
static void rsprintf(const char *fmt, ...)
{
  char stamp[Time::stamp_length];
  fprintf(stderr, "%s [RS::Manager] ", Time::stamp(stamp));
  
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  fflush(stderr);
}

void get_url(string &url)
{
  RS rs;
  RS::Info info;

  // Zlecamy pobranie pliku jak w url...
  try { rs.download(url.c_str()); }
  catch (std::exception &e) {
    rsprintf("Nie mozna dodac url'a '%s', analuje...\n", url.c_str());
    return;
  }

  rsprintf("Pobieramy plik : '%s'\n", url.c_str());

  // ... i teraz czakamy az plik zostanie pobrany albo wystapi jakis blad
  while (true) {
    rs.get_info(info);
    switch (info.status) {
      case RS::Downloaded:
        rsprintf("Plik pobrany, %6llu.%.3llu KB w %llu:%.2llu:%.2llu sek (%4llu.%.3llu KB/s)                                                            \n",
            info.bytes/1000, info.bytes%1000, info.usecs/3600000000ULL,
            (info.usecs/60000000ULL)%60ULL, (info.usecs/1000000ULL)%60ULL,
            info.usecs ? (1000ULL*info.bytes/info.usecs) : 0ULL,
            info.usecs ? (1000000ULL*info.bytes/info.usecs)%1000ULL : 0ULL);
        return;
      case RS::Canceled:
        rsprintf("Nie udalo sie pobrac pliku '%s'...\n", url.c_str());
        return;
      case RS::NotFound:
        rsprintf("Nie znaleziono pliku na serwerze: '%s'...\n", url.c_str());
        return;
      case RS::Downloading:
        {
          uint64_t eta = info.bytes ? 
            ((info.all_bytes-info.bytes)*info.usecs/info.bytes/1000000ULL):0ULL;
          uint64_t v1 = info.usecs ? (1000ULL*info.bytes/info.usecs):0ULL,
                   v2 = info.usecs ? ((1000000ULL*info.bytes/info.usecs)%1000ULL):0ULL,
                   th = info.usecs/3600000000ULL, 
                   tm = (info.usecs/60000000ULL)%60ULL,
                   ts = (info.usecs/1000000ULL)%60ULL,
                   eh = eta/3600ULL, em = (eta/60ULL)%60ULL, es = eta%60ULL;
          rsprintf("Postep: %6llu.%3llu KB %llu:%.2llu:%.2llu (sr: %4llu.%.3llu KB/s chw: %4llu.%.3llu KB/s roz: %6llu KB ETA: %llu:%.2llu:%.2llu)     \r",
              info.bytes/1000ULL, info.bytes%1000ULL, th, tm, ts, v1, v2, 
              ((uint64_t)info.tmpspeed)/1000ULL, ((uint64_t)info.tmpspeed)%1000ULL,
              info.all_bytes/1000ULL, eh, em, es);
        }
        break;
      case RS::Preparing:
        rsprintf("Przygotowywanie sie do pobierania pliku...          \r");
        break;
      case RS::Waiting:
      case RS::Later:
      case RS::Rivalry:
      case RS::Limit:
      case RS::Busy:
        rsprintf("Status '%s', oczekuje: %u sek               \r",
            rs.strstatus(info.status), info.waiting);
        break;
      default:
        throw EInternal("nieprawidlowy status?: %d, %s",
            info.status, rs.strstatus(info.status));
    };

    // A tyle odczekujemy pomiedzy kazdym sprawdzeniem statusu pobieranego pliku
    usleep(250000); 
  }
}

bool loop_fn(void)
{
  // Wczytanie danych z extends_queue na primary_queue
  try {
    if (filesystem::exists(extends_queue) && 
        filesystem::file_size(extends_queue) > 0) {
      filesystem::ifstream in(extends_queue);
      if (!in.is_open())
        throw EInternal("nie mozna otworzyc pliku: '%s'", extends_queue);

      // Zgranie primary na temporary
      filesystem::copy_file(primary_queue, temporary_queue);
      
      filesystem::ofstream out(temporary_queue);
      if (!out.is_open()) 
        throw EExternal("nie mozna otworzyc pliku: '%s'", temporary_queue);

      // Do temporary dopisujemy wpisy z extends
      string line;
      while (getline(in, line))
        out << line << endl;

      out.close(); // temporary
      in.close(); // extends

      // Do temporary wszystkie wpisy sa zgrane, wiec usuwamy primary...
      filesystem::remove(primary_queue);
      
      // ... obcinamy plik extends
      in.open(extends_queue, ios::out|ios::trunc); // trzeba dac ios::out !!!
      in.close();

      // ... i zastepujemy primary przez temporary
      filesystem::rename(temporary_queue, primary_queue);
    }
  }
  catch (std::exception &e) {
    rsprintf("Wyjatek: '%s'\n", e.what());
    return false;
  }

  string url;

  // Odczytujemy z primary url
  try {
    filesystem::ifstream in(primary_queue);
    if (!in.is_open()) 
      throw EInternal("nie mozna otworzyc pliku '%s'", primary_queue);

    if (!getline(in, url)) {
      rsprintf("Brak plikow do pobrania... \r");
      sleep(5);
      return true;
    }

    // do temporary zgramy wszystko procz pierwszego, pobranego, wpisu
    filesystem::ofstream out(temporary_queue);
    if (!out.is_open())
      throw EInternal("nie mozna otworzyc pliku '%s'", temporary_queue);

    string line;
    while (getline(in, line))
      out << line << endl;
    
    out.close();
    in.close();
  }
  catch (std::exception &e) {
    rsprintf("Wyjatek: '%s'\n", e.what());
    return false;
  }

  // Omijamy nieprawidlowe wpisy
  const char *prefix = "http://rapidshare.com";

  if (url.compare(0, strlen(prefix), prefix)) 
    rsprintf("Omijam wpis: '%s'\n", url.c_str());
  else 
    get_url(url); 

  // Na koniec zastepujemy primary przez temporary
  try {
    filesystem::remove(primary_queue);
    filesystem::rename(temporary_queue, primary_queue);
  } 
  catch (std::exception &e) {
    rsprintf("Wyjatek: '%s'\n", e.what());
    return false;
  }

  return true;
}

void escape(void)
{
  RS rs;
  try { rs.close(); }
  catch (Exception &e) {
    rsprintf("Modul RS rzucil wyjatkiem: %s", e.what());
  }

  rsprintf("\nKoniec!!\n");
}

void catch_sig(int)
{
  exit(0);
}

int main(void) 
{
  // Przechwytywanie sygnalow
  atexit(escape);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGTERM, catch_sig);
  signal(SIGQUIT, catch_sig);
  signal(SIGINT, catch_sig);
  signal(SIGHUP, catch_sig);

  rsprintf("Start!!\n");
  RS rs;

  try {
    // Ustawienie odpowiednich sciezek
    rs.set_log_file(log_file);
    rs.set_download_dir(download_dir);
    rs.set_sessions_dir(sessions_dir);

    // Wczytanie plikow z kolejka zadan
    if (filesystem::exists(primary_queue)) {
      if (filesystem::exists(temporary_queue))
        filesystem::remove(temporary_queue);
    } else {
      if (filesystem::exists(temporary_queue))
        filesystem::rename(temporary_queue, primary_queue);
      else {
        filesystem::ofstream f(primary_queue, ios::out|ios::trunc); // trzeba dac ios::out !!!
        if (!f.is_open())
          throw EInternal("nie mozna utworzyc pliku: '%s'", primary_queue);
      }
    }
  } 
  catch (std::exception &e) {
    rsprintf("Wyjatek: '%s'\n", e.what());
    return 1;
  }

  while (loop_fn());
  return 0;
}

