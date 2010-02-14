#include <RS.hh>
#include <iostream>
#include <cstdlib>

using std::cout;
using std::endl;
using std::exception;

int main(void) 
{
  cout << "[TEST] Start" << endl;
  
  RS rs;
  RS::Info info;
  int i = 0;

  rs.set_log_stderr();

  while (true) {
    try {
      const char *url = "http://rapidshare.com/files/31754685/Napisy.rar";
      //char url[1024];
      //snprintf(url, 1024, "http://rapidshare.com/files/01234567/test_nr_%drar", i);

      cout << "[TEST] Dodajemy: " << url << endl;
      rs.download(url);

      do { 
        rs.get_info(info); 
        sleep(1); 
        cout << "[TEST] Czekamy..." << endl;
      } while (
          info.status != RS::Downloaded &&
          info.status != RS::Canceled &&
          info.status != RS::NotFound);


      cout << "[TEST] Mamy: " << url << endl;
      ++i;
      break;
    }
    catch (exception &e) {
      cout << "[TEST] Wyjatek: " << e.what() << endl;
    }
  }

  rs.close();
  return 0;
}
