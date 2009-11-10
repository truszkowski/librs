

static const boost::regex rs_regex_correct_url(
    "http://rapidshare.com/files/[0-9]*/[a-zA-Z0-9._\\-]*");
static const boost::regex rs_regex_not_available(
    "This file has been deleted");
static const boost::regex rs_regex_illegal_file(
    "This file is suspected to contain illegal content and has been blocked.");
static const boost::regex rs_regex_not_found(
    "The file could not be found.");
static const boost::regex rs_regex_try_later(
    "Or try again in about ");
static const boost::regex rs_regex_server_busy(
    "Currently a lot of users are downloading files");
static const boost::regex rs_regex_already_downloading(
    "is already downloading a file.");
static const boost::regex rs_regex_reached_limit(
    "You have reached the download limit for free-users");

// Tutaj szukamy urla, rozmiaru i czasu oczekiwania
static const boost::regex rs_regex_url(
    "<form id=\"ff\" action=\"(http://[a-zA-Z0-9._/\\-]*)\" method=\"post\">");
static const boost::regex rs_regex_size(
    "<p class=\"downloadlink\">http://rapidshare.com/files/[0-9]*/[a-zA-Z0-9._\\-]* <font style=\"[a-zA-Z0-9._;:,#\\- ]*\">\\| ([0-9]*) KB</font></p>");
static const boost::regex rs_regex_time(
    "var c=([0-9]*);");

// Tutaj szukamy dwoch wartosci, adresu url do pliku i nazwy serwera na ktorym jest plik
static const boost::regex rs_regex_server(
    "onclick=\"document.dlf.action=\\\\'(http://[a-zA-Z0-9._/\\-]*)\\\\';\" /> ([a-zA-Z0-9._\\-# ]*)<br />");

// Ulubione serwery
static const char *rs_favorites_servers[] = {
  "TeliaSonera", "Cogent", "GlobalCrossing", "Teleglobe", "Deutsche Telekom", 
  "TeliaSonera #2", "GlobalCrossing #2", "Cogent #2", "Level(3)", "Level(3) #2", 
  "Level(3) #3", "Level(3) #4", NULL
};




