#include <Http.hpp>
#include <iostream>
using namespace std;

int main(int argc, char **argv)
{
  Http http;
  char *page = NULL;
  size_t size = 0;

  for (int i = 1; i < argc; ++i) {
    cerr << "== GET '" << argv[i] << "' ==" << endl;
    
    try {
      //http.set_verbose(true);
      int code = http.get(page, size, argv[i]);
      if (page) delete[] page;

      const Http::Headers &hdrs = http.get_headers();

      Http::Headers::const_iterator it;
      const char *loc = http.get_recv_header("location");
      const char *coo = http.get_recv_header("set-cookie");
      const char *len = http.get_recv_header("content-length");
    
      cerr << "== code = " << code << endl;
      cerr << "== headers size = " << hdrs.size() << endl;
      cerr << "== location = " << (loc?loc:"?") << endl;
      cerr << "== set-cookie = " << (coo?coo:"?") << endl;
      cerr << "== content-length = " << (len?len:"?") << " (strona: " << size << ")"<< endl;
    } 
    catch (Exception &e) {
      cerr << "== exception: " << e.what() << endl;
    }
  }

  return 0;
}

