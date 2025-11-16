#include "server/webserver.h"


#define debug

int main()
{
    Webserver server(9090, 3, 60000, false);
    server.run();
}