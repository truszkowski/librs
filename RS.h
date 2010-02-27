#ifndef __RAPID_SHARE_H__
#define __RAPID_SHARE_H__

#include <Exception.h>
#include <string>

class RS {
  public:
    RS(void) throw();
    ~RS(void) throw();

    // @brief: status pobierania pliku
    enum Status {
      None         =  0,  // nic do roboty
      Downloaded   =  1,  // plik zostal pobrany
      Canceled     =  2,  // pobieranie zostalo anulowane
      NotFound     =  3,  // nie znaleziono pliku
      Preparing    =  4,  // przygotowanie do pobierania
      Downloading  =  5,  // plik jest pobierany
      Waiting      =  6,  // oczekiwanie na pobranie
      Later        =  7,  // sprobuj potem
      Rivalry      =  8,  // ktos rowniez korzysta z rapidshare.com
      Limit        =  9,  // wyczerpany limit
      Busy         =  10, // serwery sa przeciazone
      Unknown      =  11  // ?
    };
    
    // @brief: funkcja dajaca opis wskazanego statusu
    static const char *strstatus(Status s);

    // @brief: informacje o aktualnie sciaganym pliku
    class Info {
      public:
        Info(void) 
          : status(None), bytes(0), all_bytes(0), usecs(0), tmpspeed(0), waiting(0) { }
        ~Info(void) { }

        Status status;
        std::string url;
        uint64_t bytes, all_bytes, usecs;
        long double tmpspeed;
        uint32_t waiting;
    };

    // @brief: zlecenie pobrania pliku spod url'a
    void download(const char *url) throw(Exception);

    // @brief: pobranie statusu pobieranego aktualne pliku
    void get_info(Info &info) throw();

    // @brief: ustawienie sciezki do katalogu ze sciaganymi plikami
    void set_download_dir(const char *path) throw(Exception);
    // @brief: ustawienie sciezki do katalogu ze sciaganym http/html
    void set_sessions_dir(const char *path) throw(Exception);
    // @brief: ustawienie sciezki do pliku z logami
    void set_log_file(const char *path) throw(Exception);
    // @brief: ustawienie logowania na ekran
    void set_log_stderr(void) throw(Exception);

    // @brief: zamknij
    void close(void) throw(Exception);

  private:
};

#endif

