#include <Http.hh>
#include <iostream>
using namespace std;

int main(int argc, char **argv)
{
  if (argc != 3) {
    cerr << argv[0] << " <adres-url> <sciezka-do-pliku" << endl;
    return 1;
  }

  Http http;
  off_t len;

  try {
    //http.set_verbose(true);
    int code = http.get(argv[2], len, argv[1]);
    const Http::Headers &hdrs = http.get_headers();

    Http::Headers::const_iterator it;
    const char *loc = http.get_recv_redirect();
    const char *coo = http.get_recv_cookies();
    const char *len = http.get_recv_header("content-length");
    
    cerr << "code = " << code << endl;
    cerr << "headers size = " << hdrs.size() << endl;
    cerr << "location = " << (loc?loc:"?") << endl;
    cerr << "set-cookie = " << (coo?coo:"?") << endl;
    cerr << "content-length = " << (len?len:"?") << " (plik: " << len << ")"<< endl;
  } 
  catch (Exception &e) {
    cerr << "exception: " << e.what() << endl;
  }

  return 0;
}

